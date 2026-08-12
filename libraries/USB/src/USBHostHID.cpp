// Copyright 2015-2024 Espressif Systems (Shanghai) PTE LTD
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

// Ensure tusb_config.h (and thus CFG_TUH_HID) is seen before USBHostHID.h so the class is defined
#if __has_include("tusb_config.h")
#include "tusb_config.h"
#endif

#include "USBHostHID.h"
/* In this file we need the class name USBHostHID for member definitions; undef the alias macro. */
#undef USBHostHID

#if CFG_TUH_HID

#include "tusb.h"
#include "class/hid/hid.h"
#include "Arduino.h"
#include "USBHost.h"

USBHostHID::USBHostHID()
  : _num_devices(0), _start_receive_pending(false),
    _pending_dev_addr(0), _pending_idx(0), _rearm_peers(false), _rearm_after_ms(0),
    _rearm_skip_addr(0), _rearm_skip_idx(0) {
  for (size_t i = 0; i < MAX_DEVICES; i++) {
    _devices[i] = nullptr;
  }
}

bool USBHostHID::hasOtherMounted(uint8_t dev_addr, uint8_t idx) const {
  for (size_t i = 0; i < _num_devices; i++) {
    USBHostHIDDevice *dev = _devices[i];
    if (dev != nullptr && dev->mounted() && !dev->matches(dev_addr, idx)) {
      return true;
    }
  }
  return false;
}

void USBHostHID::requestPeerRearm(uint8_t new_dev_addr, uint8_t new_idx) {
  if (!hasOtherMounted(new_dev_addr, new_idx)) {
    return; /* Sole device: startReceiveIfPending is enough; aborting it crashes DWC2. */
  }
  /* Deferred: aborting peers in the same tuh_task window as enum races the HCD. */
  _rearm_peers = true;
  _rearm_skip_addr = new_dev_addr;
  _rearm_skip_idx = new_idx;
  _rearm_after_ms = millis() + 150;
}

bool USBHostHID::addDevice(USBHostHIDDevice *dev) {
  if (dev == nullptr || _num_devices >= MAX_DEVICES) {
    return false;
  }
  _devices[_num_devices++] = dev;
  return true;
}

void USBHostHID::startReceiveIfPending() {
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

void USBHostHID::_onMount(uint8_t dev_addr, uint8_t idx, uint8_t const *report_desc, uint16_t desc_len) {
  const uint8_t protocol = tuh_hid_interface_protocol(dev_addr, idx);
  for (size_t i = 0; i < _num_devices; i++) {
    USBHostHIDDevice *dev = _devices[i];
    if (dev != nullptr && dev->claim(dev_addr, idx, protocol, report_desc, desc_len)) {
      log_v("[USBHostHID] mount dev=%u idx=%u protocol=%u -> claimed",
            (unsigned)dev_addr, (unsigned)idx, (unsigned)protocol);
      _start_receive_pending = true;
      _pending_dev_addr = dev_addr;
      _pending_idx = idx;
      requestPeerRearm(dev_addr, idx);
      return;
    }
  }
  log_v("[USBHostHID] mount dev=%u idx=%u protocol=%u (no handler claimed)",
        (unsigned)dev_addr, (unsigned)idx, (unsigned)protocol);
}

void USBHostHID::clearPendingIf(uint8_t dev_addr, uint8_t idx) {
  if (_pending_dev_addr == dev_addr && _pending_idx == idx) {
    _start_receive_pending = false;
    _pending_dev_addr = 0;
    _pending_idx = 0;
  }
}

void USBHostHID::releaseClaimedInterface(USBHostHIDDevice *dev, uint8_t dev_addr, uint8_t idx) {
  if (dev == nullptr) {
    return;
  }
  /*
   * tuh_umount_cb runs before class-driver close — iface is still open. Aborting a busy
   * interrupt IN here frees the DWC2 channel so the next plug can enumerate.
   * (Do not abort peer devices here.)
   */
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

void USBHostHID::_onUnmount(uint8_t dev_addr, uint8_t idx) {
  for (size_t i = 0; i < _num_devices; i++) {
    USBHostHIDDevice *dev = _devices[i];
    if (dev != nullptr && dev->matches(dev_addr, idx)) {
      releaseClaimedInterface(dev, dev_addr, idx);
      return;
    }
  }
}

void USBHostHID::_onDeviceUnmount(uint8_t dev_addr) {
  for (size_t i = 0; i < _num_devices; i++) {
    USBHostHIDDevice *dev = _devices[i];
    if (dev == nullptr || dev->devAddr() != dev_addr) {
      continue;
    }
    releaseClaimedInterface(dev, dev_addr, dev->interfaceIndex());
  }
}

void USBHostHID::_onReport(uint8_t dev_addr, uint8_t idx, uint8_t const *report, uint16_t len) {
  /* TinyUSB may deliver a failed/short IN during remove — never resubmit then. */
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
  log_v("[USBHostHID] report dev=%u idx=%u len=%u (no handler)",
        (unsigned)dev_addr, (unsigned)idx, (unsigned)len);
}

// TinyUSB host HID callbacks (strong symbols); dispatch to USBHostHID
extern "C" {

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t idx, uint8_t const *report_desc, uint16_t desc_len) {
  USBHostHIDInstance._onMount(dev_addr, idx, report_desc, desc_len);
  /* Receive arming runs after tuh_task() returns (usbhTuh / USBHost.task). */
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t idx) {
  USBHostHIDInstance._onUnmount(dev_addr, idx);
}

void tuh_umount_cb(uint8_t daddr) {
  /* Fires before class-driver close — clear all handlers for this address. */
  USBHostHIDInstance._onDeviceUnmount(daddr);
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t idx, uint8_t const *report, uint16_t len) {
  USBHostHIDInstance._onReport(dev_addr, idx, report, len);
}

/** Strong override of core weak stub — keeps HID arming on the host task. */
void arduino_usb_host_hid_service(void) {
  USBHostHIDInstance.serviceReceivesFromHostTask();
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
  /* Global singletons: do not steal from an already-claimed device (e.g. 2nd mouse). */
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
  /* Host-task only: TinyUSB queries are not safe from loop() / other cores. */
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
  /* Cache only — loop/stats must not call TinyUSB while usbhTuh owns the stack. */
  return _mounted && _itf_binding_valid && _dev_addr != 0;
}

void USBHostHID::serviceReceives() {
  /* Loop/available() must not submit/abort while usbhTuh owns the host controller. */
  if (USBHost.tuhBackgroundActive()) {
    return;
  }
  serviceReceivesFromHostTask();
}

void USBHostHID::serviceReceivesFromHostTask() {
  for (size_t i = 0; i < _num_devices; i++) {
    USBHostHIDDevice *dev = _devices[i];
    if (dev != nullptr) {
      dev->syncHostMountState();
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
    /* Re-check stack state — never abort/submit on an address mid-remove. */
    if (!tuh_mounted(a) || !tuh_hid_mounted(a, x)) {
      continue;
    }
    const bool is_new = (a == _rearm_skip_addr && x == _rearm_skip_idx);
    if (!tuh_hid_receive_ready(a, x)) {
      /* Abort only stuck peers — never the interface that just claimed/started IN. */
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

USBHostHID USBHostHIDInstance;

#endif /* CFG_TUH_HID */
