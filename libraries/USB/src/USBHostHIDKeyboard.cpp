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

/** Walk HID short/long items; true if any REPORT_ID is present. */
static bool desc_has_report_id(const uint8_t *d, uint16_t len) {
  if (d == nullptr || len == 0) {
    return false;
  }
  uint16_t i = 0;
  while (i < len) {
    const uint8_t b0 = d[i++];
    if (b0 == 0xFE) {
      if ((uint16_t)(i + 2) > len) {
        break;
      }
      const uint8_t sz = d[i++];
      i++; /* long-item tag */
      if ((uint16_t)(i + sz) > len) {
        break;
      }
      i = (uint16_t)(i + sz);
      continue;
    }
    uint8_t sz = b0 & 0x03u;
    if (sz == 3u) {
      sz = 4u;
    }
    const uint8_t tag = (uint8_t)(b0 & 0xFCu);
    if ((uint16_t)(i + sz) > len) {
      break;
    }
    if (tag == 0x84u) { /* REPORT_ID */
      return true;
    }
    i = (uint16_t)(i + sz);
  }
  return false;
}

/** 8-byte boot keyboard: reserved byte 0, key usages in the keyboard page. */
static bool boot_report_plausible(const uint8_t *q, uint16_t m) {
  if (q == nullptr || m < 8u) {
    return false;
  }
  if (q[1] != 0) {
    return false;
  }
  for (uint8_t i = 0; i < 6; i++) {
    if (q[2 + i] > 0xE7u) {
      return false;
    }
  }
  return true;
}

USBHostHIDKeyboard::USBHostHIDKeyboard()
  : USBHostHIDDevice(), _modifiers(0), _keys{0, 0, 0, 0, 0, 0}, _has_report(false), _notify_on_change_only(false), _strip_report_id(false), _last_modifiers(0),
    _last_keys{0, 0, 0, 0, 0, 0}, _last_valid(false), _report_cb(nullptr), _report_cb_arg(nullptr), _cb_w(0), _cb_r(0) {}

void USBHostHIDKeyboard::_ensureRegistered() {
  static bool registered = false;
  if (!registered) {
    registered = true;
    USBHostHID.addDevice(this);
  }
}

bool USBHostHIDKeyboard::claim(uint8_t dev_addr, uint8_t idx, uint8_t protocol, const uint8_t *report_desc, uint16_t desc_len) {
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
  _strip_report_id = desc_has_report_id(report_desc, desc_len);
  return true;
}

void USBHostHIDKeyboard::onUnmount(uint8_t dev_addr, uint8_t idx) {
  if (dev_addr == _dev_addr && idx == _idx) {
    clearHidInterfaceBinding();
    _has_report = false;
    _last_valid = false;
    _strip_report_id = false;
    _cb_w = 0;
    _cb_r = 0;
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
  (void)dev_addr;
  (void)idx;
  if (report == nullptr || len < 8u) {
    return;
  }

  /* Same idea as the mouse: descriptor REPORT_ID first, then the other offset. */
  const uint8_t *boot = nullptr;
  uint16_t boot_len = 0;
  auto take = [&](const uint8_t *q, uint16_t m) -> bool {
    if (!boot_report_plausible(q, m)) {
      return false;
    }
    boot = q;
    boot_len = m;
    return true;
  };

  if (_strip_report_id && len > 8u) {
    take(report + 1, (uint16_t)(len - 1u));
  }
  if (boot == nullptr) {
    take(report, len);
  }
  if (boot == nullptr && !_strip_report_id && len > 8u) {
    take(report + 1, (uint16_t)(len - 1u));
  }
  if (boot == nullptr) {
    /* Last resort: hinted offset, else raw (still need 8 bytes). */
    if (_strip_report_id && len > 8u) {
      boot = report + 1;
      boot_len = (uint16_t)(len - 1u);
    } else {
      boot = report;
      boot_len = len;
    }
    if (boot_len < 8u) {
      return;
    }
  }

  _applyBootReport(boot, boot_len);

  if (_notify_on_change_only && _sameAsLastNotified()) {
    return;
  }

  _has_report = true;
  _last_modifiers = _modifiers;
  memcpy(_last_keys, (const void *)_keys, 6);
  _last_valid = true;

  if (_report_cb != nullptr) {
    const uint8_t w = _cb_w;
    const uint8_t n = (uint8_t)((w + 1u) % CB_QUEUE);
    if (n != _cb_r) {
      _cb_q[w].modifiers = _modifiers;
      for (int i = 0; i < 6; i++) {
        _cb_q[w].keys[i] = _keys[i];
      }
      _cb_w = n;
    }
  }
}

void USBHostHIDKeyboard::dispatchReportCallback() {
  if (_report_cb == nullptr) {
    _cb_r = _cb_w;
    return;
  }
  while (_cb_r != _cb_w) {
    const uint8_t r = _cb_r;
    const uint8_t modifiers = _cb_q[r].modifiers;
    uint8_t keys[6];
    for (int i = 0; i < 6; i++) {
      keys[i] = _cb_q[r].keys[i];
    }
    _cb_r = (uint8_t)((r + 1u) % CB_QUEUE);
    _report_cb(modifiers, keys, _report_cb_arg);
  }
}

bool USBHostHIDKeyboard::available() {
  _ensureRegistered();
  USBHostHID.serviceReceives();
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

size_t USBHostHIDKeyboard::toAscii(char *buf, size_t cap, const uint8_t *layout) const {
  uint8_t keys[6];
  getKeys(keys);
  return toAscii(buf, cap, _modifiers, keys, layout);
}

size_t USBHostHIDKeyboard::toAscii(char *buf, size_t cap, uint8_t modifiers, const uint8_t keys[6], const uint8_t *layout) const {
  if (layout == NULL) {
    layout = KeyboardLayout_en_US;
  }
  return usbHostHidBootReportAppendAscii(buf, cap, modifiers, keys, layout);
}

uint8_t USBHostHIDKeyboard::toVirtualKey(uint8_t hid_usage) const {
  return usbHostHidKeyboardUsageToArduinoVirtualKey(hid_usage);
}

const char *USBHostHIDKeyboard::toVirtualKeyName(uint8_t hid_usage) const {
  return usbHostHidArduinoVirtualKeyName(toVirtualKey(hid_usage));
}

void USBHostHIDKeyboard::printReport(Print &out, uint8_t modifiers, const uint8_t keys[6], const uint8_t *layout) const {
  if (layout == NULL) {
    layout = KeyboardLayout_en_US;
  }

  bool any = false;
#define USBHOST_EMIT_MOD(bit, name) \
  do {                              \
    if ((modifiers & (bit)) != 0) { \
      if (any) {                    \
        out.print('|');             \
      }                             \
      out.print(#name);             \
      any = true;                   \
    }                               \
  } while (0);
  USBHOST_KEY_MOD_MAP(USBHOST_EMIT_MOD)
#undef USBHOST_EMIT_MOD

  if (keys == nullptr) {
    if (!any) {
      out.print(F("(released)"));
    }
    return;
  }

  /* Boot 6KRO: too many keys → ErrorRollOver (0x01) in every slot. Print once. */
  bool rollover = false;
  bool only_ovf = true;
  for (int i = 0; i < 6; i++) {
    if (keys[i] == 0) {
      continue;
    }
    rollover = true;
    if (keys[i] != 0x01u) {
      only_ovf = false;
      break;
    }
  }
  if (rollover && only_ovf) {
    if (any) {
      out.print('+');
    }
    out.print(F("ERR_OVF"));
    return;
  }

  for (int i = 0; i < 6; i++) {
    const uint8_t u = keys[i];
    if (u == 0) {
      continue;
    }
    if (any) {
      out.print('+');
    }
    any = true;

    const char *name = usbHostHidBootUsageLogName(u);
    if (name == nullptr) {
      name = toVirtualKeyName(u);
    }
    if (name != nullptr) {
      out.print(name);
      continue;
    }
    const char c = usbHostHidBootReportUsageToAscii(modifiers, u, layout);
    if (c > 32 && c < 127) {
      out.print(c);
      continue;
    }
    out.printf("hid=0x%02x", (unsigned)u);
  }

  if (!any) {
    out.print(F("(released)"));
  }
}

USBHostHIDKeyboard USBHostKeyboard;

#endif /* CFG_TUH_HID */
#endif /* SOC_USB_OTG_SUPPORTED */
