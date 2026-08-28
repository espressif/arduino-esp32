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
#include "USBHostHIDKeyboardDecode.h"
#include <Print.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define USBHOST_KEY_MOD_LEFT_CTRL   0x01
#define USBHOST_KEY_MOD_LEFT_SHIFT  0x02
#define USBHOST_KEY_MOD_LEFT_ALT    0x04
#define USBHOST_KEY_MOD_LEFT_GUI    0x08
#define USBHOST_KEY_MOD_RIGHT_CTRL  0x10
#define USBHOST_KEY_MOD_RIGHT_SHIFT 0x20
#define USBHOST_KEY_MOD_RIGHT_ALT   0x40
#define USBHOST_KEY_MOD_RIGHT_GUI   0x80

/**
 * Boot modifier bit → short name (same strings as printReport).
 *
 * Sketch:
 *   #define ON_MOD(bit, name) if (modifiers & (bit)) { ... #name ... }
 *   USBHOST_KEY_MOD_MAP(ON_MOD)
 *   #undef ON_MOD
 *   if ((modifiers & (LEFT_CTRL | LEFT_ALT)) && USBHostKeyboard.toVirtualKey(keys[0]) == KEY_F1)
 */
#define USBHOST_KEY_MOD_MAP(X)                \
  X(USBHOST_KEY_MOD_LEFT_CTRL, LEFT_CTRL)     \
  X(USBHOST_KEY_MOD_LEFT_SHIFT, LEFT_SHIFT)   \
  X(USBHOST_KEY_MOD_LEFT_ALT, LEFT_ALT)       \
  X(USBHOST_KEY_MOD_LEFT_GUI, LEFT_GUI)       \
  X(USBHOST_KEY_MOD_RIGHT_CTRL, RIGHT_CTRL)   \
  X(USBHOST_KEY_MOD_RIGHT_SHIFT, RIGHT_SHIFT) \
  X(USBHOST_KEY_MOD_RIGHT_ALT, RIGHT_ALT)     \
  X(USBHOST_KEY_MOD_RIGHT_GUI, RIGHT_GUI)

#ifndef LEFT_CTRL
#define LEFT_CTRL USBHOST_KEY_MOD_LEFT_CTRL
#endif
#ifndef LEFT_SHIFT
#define LEFT_SHIFT USBHOST_KEY_MOD_LEFT_SHIFT
#endif
#ifndef LEFT_ALT
#define LEFT_ALT USBHOST_KEY_MOD_LEFT_ALT
#endif
#ifndef LEFT_GUI
#define LEFT_GUI USBHOST_KEY_MOD_LEFT_GUI
#endif
#ifndef RIGHT_CTRL
#define RIGHT_CTRL USBHOST_KEY_MOD_RIGHT_CTRL
#endif
#ifndef RIGHT_SHIFT
#define RIGHT_SHIFT USBHOST_KEY_MOD_RIGHT_SHIFT
#endif
#ifndef RIGHT_ALT
#define RIGHT_ALT USBHOST_KEY_MOD_RIGHT_ALT
#endif
#ifndef RIGHT_GUI
#define RIGHT_GUI USBHOST_KEY_MOD_RIGHT_GUI
#endif

typedef void (*USBHostHIDKeyboardReportCb)(uint8_t modifiers, const uint8_t keys[6], void *arg);

/**
 * @brief USB Host HID boot keyboard.
 *
 * Claims boot-protocol keyboard interfaces and parses the 8-byte boot report
 * (optional leading report ID). Call registerWithHost() before USBHost.begin().
 */
class USBHostHIDKeyboard : public USBHostHIDDevice {
public:
  USBHostHIDKeyboard();

  bool claim(uint8_t dev_addr, uint8_t idx, uint8_t protocol, const uint8_t *report_desc, uint16_t desc_len) override;
  void onUnmount(uint8_t dev_addr, uint8_t idx) override;
  void onReport(uint8_t dev_addr, uint8_t idx, const uint8_t *report, uint16_t len) override;

  bool available();
  uint8_t getModifiers() const {
    return _modifiers;
  }
  void getKeys(uint8_t keys[6]) const;
  bool isKeyDown(uint8_t hid_usage) const;
  void clear();

  /**
   * Decode the current boot report to ASCII using a USBHIDKeyboard layout
   * (default US). Same as toAscii(buf, cap, getModifiers(), keys).
   */
  size_t toAscii(char *buf, size_t cap, const uint8_t *layout = KeyboardLayout_en_US) const;
  /** Decode a boot report (e.g. from the report callback). */
  size_t toAscii(char *buf, size_t cap, uint8_t modifiers, const uint8_t keys[6], const uint8_t *layout = KeyboardLayout_en_US) const;
  /** Map a keyboard-page HID usage to Arduino KEY_* (0 if not a special). */
  uint8_t toVirtualKey(uint8_t hid_usage) const;
  /** KEY_* identifier string for that usage, or nullptr. */
  const char *toVirtualKeyName(uint8_t hid_usage) const;
  /**
   * Print modifiers as LEFT_CTRL / LEFT_SHIFT / … and keys as ASCII or KEY_*.
   * Example: LEFT_CTRL+KEY_F1
   */
  void printReport(Print &out, uint8_t modifiers, const uint8_t keys[6], const uint8_t *layout = KeyboardLayout_en_US) const;

  /** Skip identical held-key reports (default false). */
  void setNotifyOnChangeOnly(bool enable) {
    _notify_on_change_only = enable;
  }
  bool notifyOnChangeOnly() const {
    return _notify_on_change_only;
  }

  void registerWithHost() {
    _ensureRegistered();
  }

  /**
   * Called from USBHost.task() (loop context), not from the TinyUSB worker.
   * Keep the callback reasonably short; Serial is OK here.
   */
  void setReportCallback(USBHostHIDKeyboardReportCb cb, void *arg = nullptr) {
    _report_cb = cb;
    _report_cb_arg = arg;
  }

private:
  void _ensureRegistered();
  void _applyBootReport(const uint8_t *boot, uint16_t boot_len);
  bool _sameAsLastNotified() const;
  void dispatchReportCallback() override;

  struct ReportEvent {
    uint8_t modifiers;
    uint8_t keys[6];
  };
  static const uint8_t CB_QUEUE = 8;

  volatile uint8_t _modifiers;
  volatile uint8_t _keys[6];
  volatile bool _has_report;
  bool _notify_on_change_only;
  bool _strip_report_id;  ///< descriptor has REPORT_ID; try that offset first
  uint8_t _last_modifiers;
  uint8_t _last_keys[6];
  bool _last_valid;
  USBHostHIDKeyboardReportCb _report_cb;
  void *_report_cb_arg;
  ReportEvent _cb_q[CB_QUEUE];
  volatile uint8_t _cb_w;
  volatile uint8_t _cb_r;
};

extern USBHostHIDKeyboard USBHostKeyboard;

#endif /* CFG_TUH_HID */
#endif /* SOC_USB_OTG_SUPPORTED */
