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

#pragma once

#include "soc/soc_caps.h"
#if SOC_USB_OTG_SUPPORTED

#if __has_include("tusb_config.h")
#include "tusb_config.h"
#endif
#ifndef CFG_TUH_HID
#define CFG_TUH_HID 0
#endif

#if CFG_TUH_HID

#include "USBHostHID.h"
#include <stdint.h>
#include <stdbool.h>

/** Boot keyboard modifier bits (USB HID keyboard). */
#define USBHOST_KEY_MOD_LEFT_CTRL   0x01
#define USBHOST_KEY_MOD_LEFT_SHIFT  0x02
#define USBHOST_KEY_MOD_LEFT_ALT    0x04
#define USBHOST_KEY_MOD_LEFT_GUI    0x08
#define USBHOST_KEY_MOD_RIGHT_CTRL  0x10
#define USBHOST_KEY_MOD_RIGHT_SHIFT 0x20
#define USBHOST_KEY_MOD_RIGHT_ALT   0x40
#define USBHOST_KEY_MOD_RIGHT_GUI   0x80

/** Optional callback when a boot keyboard report is received. */
typedef void (*USBHostHIDKeyboardReportCb)(uint8_t modifiers, const uint8_t keys[6], void *arg);

/**
 * @brief USB Host HID boot keyboard (Arduino API).
 * Claims HID interfaces with boot protocol Keyboard. Parses the standard 8-byte
 * boot report (optionally prefixed by a 1-byte report ID).
 */
class USBHostHIDKeyboard : public USBHostHIDDevice {
public:
  USBHostHIDKeyboard();

  bool claim(uint8_t dev_addr, uint8_t idx, uint8_t protocol,
             const uint8_t *report_desc, uint16_t desc_len) override;
  void onUnmount(uint8_t dev_addr, uint8_t idx) override;
  void onReport(uint8_t dev_addr, uint8_t idx, const uint8_t *report, uint16_t len) override;

  /** True if a report is waiting. Also calls USBHostHID.serviceReceives() (no-op with host worker). */
  bool available();

  /** Modifier bitmask (USBHOST_KEY_MOD_*). */
  uint8_t getModifiers() const { return _modifiers; }

  /** Up to six non-modifier key usages (HID key codes); 0 = empty slot. */
  void getKeys(uint8_t keys[6]) const;

  /** True if @p hid_usage appears in the current key slots. */
  bool isKeyDown(uint8_t hid_usage) const;

  void clear();

  /**
   * If true, identical boot reports do not set available() or invoke the callback
   * (reduces spam while a key is held). Default false for backward compatibility.
   */
  void setNotifyOnChangeOnly(bool enable) { _notify_on_change_only = enable; }
  bool notifyOnChangeOnly() const { return _notify_on_change_only; }

  void setReportCallback(USBHostHIDKeyboardReportCb cb, void *arg = nullptr) {
    _report_cb = cb;
    _report_cb_arg = arg;
  }

  /** Call in setup() before USBHost.begin() if the keyboard may enumerate before loop(). */
  void registerWithHost() { _ensureRegistered(); }

private:
  void _ensureRegistered();
  void _applyBootReport(const uint8_t *boot, uint16_t boot_len);
  bool _sameAsLastNotified() const;

  volatile uint8_t _modifiers;
  volatile uint8_t _keys[6];
  volatile bool _has_report;
  bool _notify_on_change_only;
  uint8_t _last_modifiers;
  uint8_t _last_keys[6];
  bool _last_valid;
  USBHostHIDKeyboardReportCb _report_cb;
  void *_report_cb_arg;
};

extern USBHostHIDKeyboard USBHostKeyboard;

#endif /* CFG_TUH_HID */
#endif /* SOC_USB_OTG_SUPPORTED */
