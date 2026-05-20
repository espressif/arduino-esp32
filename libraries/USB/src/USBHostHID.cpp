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

USBHostHID::USBHostHID()
  : _num_devices(0), _start_receive_pending(false),
    _pending_dev_addr(0), _pending_idx(0) {
  for (size_t i = 0; i < MAX_DEVICES; i++) {
    _devices[i] = nullptr;
  }
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
  log_v("[USBHostHID] start receive dev=%u idx=%u",
        (unsigned)_pending_dev_addr, (unsigned)_pending_idx);
  _start_receive_pending = false;
  tuh_hid_receive_report(_pending_dev_addr, _pending_idx);
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
      return;
    }
  }
  log_v("[USBHostHID] mount dev=%u idx=%u protocol=%u (no handler claimed)",
        (unsigned)dev_addr, (unsigned)idx, (unsigned)protocol);
}

void USBHostHID::_onUnmount(uint8_t dev_addr, uint8_t idx) {
  for (size_t i = 0; i < _num_devices; i++) {
    USBHostHIDDevice *dev = _devices[i];
    if (dev != nullptr && dev->matches(dev_addr, idx)) {
      log_v("[USBHostHID] unmount dev=%u idx=%u", (unsigned)dev_addr, (unsigned)idx);
      dev->onUnmount(dev_addr, idx);
      if (_pending_dev_addr == dev_addr && _pending_idx == idx) {
        _start_receive_pending = false;
        _pending_dev_addr = 0;
        _pending_idx = 0;
      }
      return;
    }
  }
}

void USBHostHID::_onReport(uint8_t dev_addr, uint8_t idx, uint8_t const *report, uint16_t len) {
  for (size_t i = 0; i < _num_devices; i++) {
    USBHostHIDDevice *dev = _devices[i];
    if (dev != nullptr && dev->matches(dev_addr, idx)) {
      dev->onReport(dev_addr, idx, report, len);
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
  /* Retry pending submit and arm IN for all registered handlers (multi-HID). */
  USBHostHIDInstance.serviceReceives();
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t idx) {
  USBHostHIDInstance._onUnmount(dev_addr, idx);
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t idx, uint8_t const *report, uint16_t len) {
  USBHostHIDInstance._onReport(dev_addr, idx, report, len);
}

void tuh_hid_report_sent_cb(uint8_t dev_addr, uint8_t idx, uint8_t const *report, uint16_t len) {
  (void)dev_addr;
  (void)idx;
  (void)report;
  (void)len;
}

void tuh_hid_get_report_complete_cb(uint8_t dev_addr, uint8_t idx, uint8_t report_id, uint8_t report_type, uint16_t len) {
  (void)dev_addr;
  (void)idx;
  (void)report_id;
  (void)report_type;
  (void)len;
}

void tuh_hid_set_report_complete_cb(uint8_t dev_addr, uint8_t idx, uint8_t report_id, uint8_t report_type, uint16_t len) {
  (void)dev_addr;
  (void)idx;
  (void)report_id;
  (void)report_type;
  (void)len;
}

void tuh_hid_set_protocol_complete_cb(uint8_t dev_addr, uint8_t idx, uint8_t protocol) {
  (void)dev_addr;
  (void)idx;
  (void)protocol;
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
  if (!tuh_hid_mounted(_dev_addr, _idx)) {
    onUnmount(_dev_addr, _idx);
    return;
  }
  const uint8_t idx_now = tuh_hid_itf_get_index(_dev_addr, _itf_number);
  if (idx_now == TUSB_INDEX_INVALID_8 || idx_now != _idx) {
    onUnmount(_dev_addr, _idx);
  }
}

bool USBHostHIDDevice::mounted() const {
  if (!_mounted || !_itf_binding_valid || _dev_addr == 0) {
    return false;
  }
  if (!tuh_hid_mounted(_dev_addr, _idx)) {
    return false;
  }
  const uint8_t idx_now = tuh_hid_itf_get_index(_dev_addr, _itf_number);
  return (idx_now != TUSB_INDEX_INVALID_8 && idx_now == _idx);
}

void USBHostHID::serviceReceives() {
  for (size_t i = 0; i < _num_devices; i++) {
    USBHostHIDDevice *dev = _devices[i];
    if (dev != nullptr) {
      dev->syncHostMountState();
    }
  }
  if (_pending_dev_addr != 0 && !tuh_hid_mounted(_pending_dev_addr, _pending_idx)) {
    _start_receive_pending = false;
    _pending_dev_addr = 0;
    _pending_idx = 0;
  }
  startReceiveIfPending();
  for (size_t i = 0; i < _num_devices; i++) {
    USBHostHIDDevice *dev = _devices[i];
    if (dev == nullptr || !dev->mounted()) {
      continue;
    }
    const uint8_t a = dev->devAddr();
    const uint8_t x = dev->interfaceIndex();
    if (tuh_hid_receive_ready(a, x)) {
      (void)tuh_hid_receive_report(a, x);
    }
  }
}

USBHostHID USBHostHIDInstance;

#endif /* CFG_TUH_HID */
