// Copyright 2015-2026 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#if __has_include("tusb_config.h")
#include "tusb_config.h"
#endif

#include "USBHostHID.h"

#if CFG_TUH_HID

#include "tusb.h"
#include "class/hid/hid.h"
#include "Arduino.h"
#include "USBHost.h"

USBHostHIDClass::USBHostHIDClass()
  : _num_devices(0), _start_receive_pending(false), _pending_dev_addr(0), _pending_idx(0),
    _rearm_peers(false), _rearm_after_ms(0), _rearm_skip_addr(0), _rearm_skip_idx(0) {
  for (size_t i = 0; i < MAX_DEVICES; i++) {
    _devices[i] = nullptr;
  }
}

bool USBHostHIDClass::hasOtherMounted(uint8_t dev_addr, uint8_t idx) const {
  for (size_t i = 0; i < _num_devices; i++) {
    USBHostHIDDevice *dev = _devices[i];
    if (dev != nullptr && dev->mounted() && !dev->matches(dev_addr, idx)) {
      return true;
    }
  }
  return false;
}

void USBHostHIDClass::requestPeerRearm(uint8_t new_dev_addr, uint8_t new_idx) {
  /* Sole device: startReceiveIfPending is enough; aborting it can crash DWC2. */
  if (!hasOtherMounted(new_dev_addr, new_idx)) {
    return;
  }
  /* Defer peer abort so it does not race hub enum in the same tuh_task window. */
  _rearm_peers = true;
  _rearm_skip_addr = new_dev_addr;
  _rearm_skip_idx = new_idx;
  _rearm_after_ms = millis() + 150;
}

bool USBHostHIDClass::addDevice(USBHostHIDDevice *dev) {
  if (dev == nullptr || _num_devices >= MAX_DEVICES) {
    return false;
  }
  _devices[_num_devices++] = dev;
  return true;
}

void USBHostHIDClass::startReceiveIfPending() {
  if (!_start_receive_pending || _pending_dev_addr == 0) {
    return;
  }
  const uint8_t a = _pending_dev_addr;
  const uint8_t x = _pending_idx;
  if (!tuh_mounted(a) || !tuh_hid_mounted(a, x)) {
    _start_receive_pending = false;
    _pending_dev_addr = 0;
    _pending_idx = 0;
    return;
  }
  log_v("[USBHostHID] start receive dev=%u idx=%u", (unsigned)a, (unsigned)x);
  _start_receive_pending = false;
  _pending_dev_addr = 0;
  _pending_idx = 0;
  (void)tuh_hid_receive_report(a, x);
}

void USBHostHIDClass::_onMount(uint8_t dev_addr, uint8_t idx, uint8_t const *report_desc, uint16_t desc_len) {
  const uint8_t protocol = tuh_hid_interface_protocol(dev_addr, idx);
  for (size_t i = 0; i < _num_devices; i++) {
    USBHostHIDDevice *dev = _devices[i];
    if (dev != nullptr && dev->claim(dev_addr, idx, protocol, report_desc, desc_len)) {
      log_v("[USBHostHID] mount dev=%u idx=%u protocol=%u claimed", (unsigned)dev_addr, (unsigned)idx,
            (unsigned)protocol);
      _start_receive_pending = true;
      _pending_dev_addr = dev_addr;
      _pending_idx = idx;
      requestPeerRearm(dev_addr, idx);
      return;
    }
  }
  log_v("[USBHostHID] mount dev=%u idx=%u protocol=%u (no handler)", (unsigned)dev_addr, (unsigned)idx,
        (unsigned)protocol);
}

void USBHostHIDClass::clearPendingIf(uint8_t dev_addr, uint8_t idx) {
  if (_pending_dev_addr == dev_addr && _pending_idx == idx) {
    _start_receive_pending = false;
    _pending_dev_addr = 0;
    _pending_idx = 0;
  }
}

void USBHostHIDClass::releaseClaimedInterface(USBHostHIDDevice *dev, uint8_t dev_addr, uint8_t idx) {
  if (dev == nullptr) {
    return;
  }
  /* Abort busy IN on the leaving iface so the DWC2 channel can be reused on replug. */
  if (tuh_hid_mounted(dev_addr, idx) && !tuh_hid_receive_ready(dev_addr, idx)) {
    log_v("[USBHostHID] abort IN on unmount dev=%u idx=%u", (unsigned)dev_addr, (unsigned)idx);
    (void)tuh_hid_receive_abort(dev_addr, idx);
  }
  if (_rearm_skip_addr == dev_addr && _rearm_skip_idx == idx) {
    _rearm_peers = false;
    _rearm_skip_addr = 0;
    _rearm_skip_idx = 0;
  }
  clearPendingIf(dev_addr, idx);
  log_v("[USBHostHID] unmount dev=%u idx=%u", (unsigned)dev_addr, (unsigned)idx);
  dev->onUnmount(dev_addr, idx);
}

void USBHostHIDClass::_onUnmount(uint8_t dev_addr, uint8_t idx) {
  for (size_t i = 0; i < _num_devices; i++) {
    USBHostHIDDevice *dev = _devices[i];
    if (dev != nullptr && dev->matches(dev_addr, idx)) {
      releaseClaimedInterface(dev, dev_addr, idx);
      return;
    }
  }
}

void USBHostHIDClass::_onDeviceUnmount(uint8_t dev_addr) {
  for (size_t i = 0; i < _num_devices; i++) {
    USBHostHIDDevice *dev = _devices[i];
    if (dev == nullptr || dev->devAddr() != dev_addr) {
      continue;
    }
    releaseClaimedInterface(dev, dev_addr, dev->interfaceIndex());
  }
}

void USBHostHIDClass::_onReport(uint8_t dev_addr, uint8_t idx, uint8_t const *report, uint16_t len) {
  /* May see a short/failed IN during remove — never resubmit then. */
  if (!tuh_hid_mounted(dev_addr, idx)) {
    return;
  }
  for (size_t i = 0; i < _num_devices; i++) {
    USBHostHIDDevice *dev = _devices[i];
    if (dev != nullptr && dev->matches(dev_addr, idx)) {
      dev->onReport(dev_addr, idx, report, len);
      if (tuh_hid_mounted(dev_addr, idx) && tuh_hid_receive_ready(dev_addr, idx)) {
        (void)tuh_hid_receive_report(dev_addr, idx);
      }
      return;
    }
  }
}

extern "C" {

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t idx, uint8_t const *report_desc, uint16_t desc_len) {
  USBHostHID._onMount(dev_addr, idx, report_desc, desc_len);
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t idx) {
  USBHostHID._onUnmount(dev_addr, idx);
}

void tuh_umount_cb(uint8_t daddr) {
  USBHostHID._onDeviceUnmount(daddr);
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t idx, uint8_t const *report, uint16_t len) {
  USBHostHID._onReport(dev_addr, idx, report, len);
}

void arduino_usb_host_hid_service(void) {
  USBHostHID.serviceReceivesFromHostTask();
}

}  // extern "C"

void USBHostHIDDevice::clearHidInterfaceBinding() {
  _itf_binding_valid = false;
  _itf_number = 0;
  _mounted = false;
  _dev_addr = 0;
  _idx = 0;
}

bool USBHostHIDDevice::bindHidInterface(uint8_t dev_addr, uint8_t idx) {
  if (_mounted && _itf_binding_valid && (_dev_addr != dev_addr || _idx != idx)) {
    log_v("[USBHostHID] bind refused: already own dev=%u idx=%u", (unsigned)_dev_addr, (unsigned)_idx);
    return false;
  }
  tuh_itf_info_t info;
  if (!tuh_hid_itf_get_info(dev_addr, idx, &info)) {
    return false;
  }
  _dev_addr = dev_addr;
  _idx = idx;
  _itf_number = info.desc.bInterfaceNumber;
  _itf_binding_valid = true;
  _mounted = true;
  return true;
}

void USBHostHIDDevice::syncHostMountState() {
  if (!_mounted || !_itf_binding_valid || _dev_addr == 0) {
    return;
  }
  if (!tuh_mounted(_dev_addr) || !tuh_hid_mounted(_dev_addr, _idx)) {
    onUnmount(_dev_addr, _idx);
    return;
  }
  const uint8_t idx_now = tuh_hid_itf_get_index(_dev_addr, _itf_number);
  if (idx_now == TUSB_INDEX_INVALID_8 || idx_now != _idx) {
    onUnmount(_dev_addr, _idx);
  }
}

bool USBHostHIDDevice::mounted() const {
  return _mounted && _itf_binding_valid && _dev_addr != 0;
}

void USBHostHIDClass::serviceReceives() {
  if (USBHost.tuhBackgroundActive()) {
    return;
  }
  serviceReceivesFromHostTask();
}

void USBHostHIDClass::serviceReceivesFromHostTask() {
  for (size_t i = 0; i < _num_devices; i++) {
    if (_devices[i] != nullptr) {
      _devices[i]->syncHostMountState();
    }
  }
  if (_pending_dev_addr != 0 &&
      (!tuh_mounted(_pending_dev_addr) || !tuh_hid_mounted(_pending_dev_addr, _pending_idx))) {
    _start_receive_pending = false;
    _pending_dev_addr = 0;
    _pending_idx = 0;
  }
  startReceiveIfPending();

  bool rearm_peers = false;
  if (_rearm_peers && (int32_t)(millis() - _rearm_after_ms) >= 0) {
    rearm_peers = true;
    _rearm_peers = false;
  }

  for (size_t i = 0; i < _num_devices; i++) {
    USBHostHIDDevice *dev = _devices[i];
    if (dev == nullptr || !dev->mounted()) {
      continue;
    }
    const uint8_t a = dev->devAddr();
    const uint8_t x = dev->interfaceIndex();
    if (!tuh_mounted(a) || !tuh_hid_mounted(a, x)) {
      continue;
    }
    const bool is_new = (a == _rearm_skip_addr && x == _rearm_skip_idx);
    if (!tuh_hid_receive_ready(a, x)) {
      /* Peer-only rearm — never abort the interface that just claimed. */
      if (!rearm_peers || is_new) {
        continue;
      }
      log_v("[USBHostHID] peer rearm abort+receive dev=%u idx=%u", (unsigned)a, (unsigned)x);
      (void)tuh_hid_receive_abort(a, x);
      if (!tuh_mounted(a) || !tuh_hid_mounted(a, x) || !tuh_hid_receive_ready(a, x)) {
        continue;
      }
    }
    (void)tuh_hid_receive_report(a, x);
  }
}

USBHostHIDClass USBHostHID;

#endif /* CFG_TUH_HID */
