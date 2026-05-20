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

#include "USBHostHIDKeyboard.h"

#if SOC_USB_OTG_SUPPORTED
#if CFG_TUH_HID

#include "tusb.h"
#include "class/hid/hid.h"
#include "Arduino.h"
#include <string.h>

USBHostHIDKeyboard::USBHostHIDKeyboard()
  : USBHostHIDDevice(),
    _modifiers(0),
    _keys{0, 0, 0, 0, 0, 0},
    _has_report(false),
    _notify_on_change_only(false),
    _last_modifiers(0),
    _last_keys{0, 0, 0, 0, 0, 0},
    _last_valid(false),
    _report_cb(nullptr),
    _report_cb_arg(nullptr) {
}

void USBHostHIDKeyboard::_ensureRegistered() {
  static bool registered = false;
  if (!registered) {
    registered = true;
    USBHostHIDInstance.addDevice(this);
  }
}

bool USBHostHIDKeyboard::claim(uint8_t dev_addr, uint8_t idx, uint8_t protocol,
                                const uint8_t *report_desc, uint16_t desc_len) {
  (void)report_desc;
  (void)desc_len;
  if (protocol != HID_ITF_PROTOCOL_KEYBOARD) {
    return false;
  }
  if (!bindHidInterface(dev_addr, idx)) {
    return false;
  }
  _has_report = false;
  _last_valid = false;
  _modifiers = 0;
  for (int i = 0; i < 6; i++) {
    _keys[i] = 0;
  }
  log_v("[USBHostKeyboard] claim dev_addr=%u idx=%u", (unsigned)dev_addr, (unsigned)idx);
  return true;
}

void USBHostHIDKeyboard::onUnmount(uint8_t dev_addr, uint8_t idx) {
  if (dev_addr == _dev_addr && idx == _idx) {
    log_v("[USBHostKeyboard] unmount dev_addr=%u idx=%u", (unsigned)dev_addr, (unsigned)idx);
    clearHidInterfaceBinding();
    _has_report = false;
    _last_valid = false;
  }
}

void USBHostHIDKeyboard::_applyBootReport(const uint8_t *boot, uint16_t boot_len) {
  if (boot_len < 8u) {
    return;
  }
  _modifiers = boot[0];
  for (int i = 0; i < 6; i++) {
    _keys[i] = boot[2 + i];
  }
}

bool USBHostHIDKeyboard::_sameAsLastNotified() const {
  if (!_last_valid) {
    return false;
  }
  if (_last_modifiers != _modifiers) {
    return false;
  }
  for (int i = 0; i < 6; i++) {
    if (_last_keys[i] != _keys[i]) {
      return false;
    }
  }
  return true;
}

void USBHostHIDKeyboard::onReport(uint8_t dev_addr, uint8_t idx, const uint8_t *report, uint16_t len) {
  const uint8_t *boot = report;
  uint16_t boot_len = len;

  /* Many keyboards prefix the 8-byte boot report with a report ID. */
  if (len > 8u) {
    boot = report + 1;
    boot_len = len - 1u;
  }

  if (boot_len < 8u) {
    log_v("[USBHostKeyboard] report len=%u (need 8-byte boot or 9 with ID)", (unsigned)len);
    tuh_hid_receive_report(dev_addr, idx);
    return;
  }

  _applyBootReport(boot, boot_len);

  if (_notify_on_change_only && _sameAsLastNotified()) {
    tuh_hid_receive_report(dev_addr, idx);
    return;
  }

  _has_report = true;
  _last_modifiers = _modifiers;
  memcpy(_last_keys, (const void *)_keys, 6);
  _last_valid = true;

  if (_report_cb) {
    uint8_t kcopy[6];
    getKeys(kcopy);
    _report_cb(_modifiers, kcopy, _report_cb_arg);
  }
  log_v("[USBHostKeyboard] mod=0x%02x keys %02x %02x %02x %02x %02x %02x",
        (unsigned)_modifiers,
        (unsigned)_keys[0], (unsigned)_keys[1], (unsigned)_keys[2],
        (unsigned)_keys[3], (unsigned)_keys[4], (unsigned)_keys[5]);

  tuh_hid_receive_report(dev_addr, idx);
}

bool USBHostHIDKeyboard::available() {
  _ensureRegistered();
  USBHostHIDInstance.serviceReceives();
  return _has_report;
}

void USBHostHIDKeyboard::getKeys(uint8_t keys[6]) const {
  if (keys == nullptr) {
    return;
  }
  for (int i = 0; i < 6; i++) {
    keys[i] = _keys[i];
  }
}

bool USBHostHIDKeyboard::isKeyDown(uint8_t hid_usage) const {
  for (int i = 0; i < 6; i++) {
    if (_keys[i] == hid_usage) {
      return true;
    }
  }
  return false;
}

void USBHostHIDKeyboard::clear() {
  _has_report = false;
}

USBHostHIDKeyboard USBHostKeyboard;

#endif /* CFG_TUH_HID */
#endif /* SOC_USB_OTG_SUPPORTED */
