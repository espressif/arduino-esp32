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

#include "USBHostHIDGamepad.h"

#if SOC_USB_OTG_SUPPORTED
#if CFG_TUH_HID

#include <string.h>
#include "tusb.h"
#include "class/hid/hid.h"
#include "Arduino.h"
#include "esp32-hal-log.h"

/** True if collection looks like a Generic Desktop Mouse (usage 0x02). */
static bool desc_is_desktop_mouse(const uint8_t *d, uint16_t len) {
  if (d == nullptr || len < 4) {
    return false;
  }
  for (uint16_t i = 0; i + 3 < len; i++) {
    if (d[i] == 0x05 && d[i + 1] == 0x01 && d[i + 2] == 0x09 && d[i + 3] == 0x02) {
      return true;
    }
  }
  for (uint16_t i = 0; i + 4 < len; i++) {
    if (d[i] == 0x06 && d[i + 1] == 0x01 && d[i + 2] == 0x00 && d[i + 3] == 0x09 && d[i + 4] == 0x02) {
      return true;
    }
  }
  return false;
}

/** True if report descriptor declares Generic Desktop Game Pad or Joystick. */
static bool desc_is_gamepad_or_joystick(const uint8_t *d, uint16_t len) {
  if (d == nullptr || len < 4) {
    return false;
  }
  /* Common pattern: Usage Page (Generic Desktop 0x0001) + Usage (Game Pad 0x05 or Joystick 0x04) */
  for (uint16_t i = 0; i + 3 < len; i++) {
    if (d[i] == 0x05 && d[i + 1] == 0x01 && d[i + 2] == 0x09) {
      uint8_t u = d[i + 3];
      if (u == 0x04 || u == 0x05 || u == 0x08) { /* Joystick, Game Pad, Multi-axis */
        return true;
      }
    }
  }
  /* Long usage page 0x0001: 0x06 0x01 0x00 then usage 0x09 0x05 */
  for (uint16_t i = 0; i + 4 < len; i++) {
    if (d[i] == 0x06 && d[i + 1] == 0x01 && d[i + 2] == 0x00 && d[i + 3] == 0x09 &&
        (d[i + 4] == 0x05 || d[i + 4] == 0x04 || d[i + 4] == 0x08)) {
      return true;
    }
  }
  /* Usage Page Desktop then later Usage Game Pad / Joystick (not necessarily adjacent) */
  for (uint16_t i = 0; i + 1 < len; i++) {
    if (d[i] != 0x05 || d[i + 1] != 0x01) {
      continue;
    }
    uint16_t end = i + 2 + 160;
    if (end > len) {
      end = len;
    }
    for (uint16_t j = i + 2; j + 1 < end; j++) {
      if (d[j] == 0x09) {
        uint8_t u = d[j + 1];
        if (u == 0x04 || u == 0x05 || u == 0x08) {
          return true;
        }
      }
    }
  }
  return false;
}

/** Generic Desktop + Rx(0x33)/Ry(0x34) — common on dual-stick pads (e.g. many Genius). */
static bool desc_has_rx_ry(const uint8_t *d, uint16_t len) {
  bool desk = false;
  for (uint16_t i = 0; i + 1 < len; i++) {
    if (d[i] == 0x05 && d[i + 1] == 0x01) {
      desk = true;
    }
    if (d[i] == 0x06 && i + 2 < len && d[i + 1] == 0x01 && d[i + 2] == 0x00) {
      desk = true;
    }
    if (desk && d[i] == 0x09 && i + 1 < len) {
      uint8_t u = d[i + 1];
      if (u == 0x33 || u == 0x34) {
        return true;
      }
    }
  }
  return false;
}

/** Desktop + Hat switch (0x39) — typical game controllers. */
static bool desc_has_hat(const uint8_t *d, uint16_t len) {
  bool desk = false;
  for (uint16_t i = 0; i + 1 < len; i++) {
    if (d[i] == 0x05 && d[i + 1] == 0x01) {
      desk = true;
    }
    if (desk && d[i] == 0x09 && d[i + 1] == 0x39) {
      return true;
    }
  }
  return false;
}

/** 3+ distinct stick usages (0x30–0x35) after Desktop — pads with many axes. */
static bool desc_many_desktop_axes(const uint8_t *d, uint16_t len) {
  bool desk = false;
  uint8_t mask = 0;
  for (uint16_t i = 0; i + 1 < len; i++) {
    if (d[i] == 0x05 && d[i + 1] == 0x01) {
      desk = true;
    }
    if (!desk) {
      continue;
    }
    if (d[i] == 0x09 && i + 1 < len) {
      uint8_t u = d[i + 1];
      if (u >= 0x30 && u <= 0x35) {
        mask |= (uint8_t)(1u << (u - 0x30));
      }
    }
  }
  int n = 0;
  for (int b = 0; b < 6; b++) {
    if (mask & (1 << b)) {
      n++;
    }
  }
  return n >= 3;
}

/** Desktop + Button page + X axis — typical cheap gamepads (e.g. Genius). */
static bool desc_desktop_buttons_and_x(const uint8_t *d, uint16_t len) {
  bool desk = false, btn_page = false, x_axis = false;
  for (uint16_t i = 0; i + 1 < len; i++) {
    if (d[i] == 0x05 && d[i + 1] == 0x01) {
      desk = true;
    }
    if (d[i] == 0x05 && d[i + 1] == 0x09) {
      btn_page = true;
    }
    if (desk && d[i] == 0x09 && d[i + 1] == 0x30) {
      x_axis = true;
    }
  }
  return desk && btn_page && x_axis && len >= 14;
}

static bool desc_should_claim_gamepad(const uint8_t *d, uint16_t desc_len) {
  if (desc_is_gamepad_or_joystick(d, desc_len)) {
    return true;
  }
  if (desc_len < 8) {
    return false;
  }
  return desc_has_rx_ry(d, desc_len) || desc_has_hat(d, desc_len) || desc_many_desktop_axes(d, desc_len) ||
         desc_desktop_buttons_and_x(d, desc_len);
}

USBHostHIDGamepad::USBHostHIDGamepad()
  : USBHostHIDDevice(),
    _report_len(0),
    _has_report(false),
    _report_cb(nullptr),
    _report_cb_arg(nullptr),
    _notify_on_change_only(false),
    _last_notified_len(0) {
  memset(_report, 0, sizeof(_report));
  memset(_last_notified, 0, sizeof(_last_notified));
}

void USBHostHIDGamepad::_ensureRegistered() {
  static bool registered = false;
  if (!registered) {
    registered = true;
    USBHostHIDInstance.addDevice(this);
  }
}

bool USBHostHIDGamepad::claim(uint8_t dev_addr, uint8_t idx, uint8_t protocol,
                              const uint8_t *report_desc, uint16_t desc_len) {
  if (protocol == HID_ITF_PROTOCOL_MOUSE || protocol == HID_ITF_PROTOCOL_KEYBOARD) {
    return false;
  }
  /* Broad gamepad heuristics also match many report-protocol mice — never steal those. */
  if (desc_is_desktop_mouse(report_desc, desc_len)) {
    return false;
  }
  if (!desc_should_claim_gamepad(report_desc, desc_len)) {
    return false;
  }
  if (!bindHidInterface(dev_addr, idx)) {
    return false;
  }
  _has_report = false;
  _report_len = 0;
  _last_notified_len = 0;
  memset(_last_notified, 0, sizeof(_last_notified));
  log_v("[USBHostGamepad] claim dev=%u idx=%u protocol=%u desc_len=%u",
        (unsigned)dev_addr, (unsigned)idx, (unsigned)protocol, (unsigned)desc_len);
  return true;
}

void USBHostHIDGamepad::onUnmount(uint8_t dev_addr, uint8_t idx) {
  if (dev_addr == _dev_addr && idx == _idx) {
    log_v("[USBHostGamepad] unmount dev_addr=%u idx=%u", (unsigned)dev_addr, (unsigned)idx);
    clearHidInterfaceBinding();
    _report_len = 0;
    _has_report = false;
    _last_notified_len = 0;
  }
}

void USBHostHIDGamepad::onReport(uint8_t dev_addr, uint8_t idx, const uint8_t *report, uint16_t len) {
  (void)dev_addr;
  (void)idx;
  if (len > REPORT_CAP) {
    len = REPORT_CAP;
  }
  if (_notify_on_change_only && _last_notified_len == len &&
      len > 0 && memcmp(_last_notified, report, len) == 0) {
    return;
  }
  memcpy(_report, report, len);
  _report_len = len;
  _has_report = true;
  if (_report_cb) {
    _report_cb(_report, _report_len, _report_cb_arg);
  }
  if (_notify_on_change_only && len > 0) {
    memcpy(_last_notified, _report, len);
    _last_notified_len = len;
  }
}

bool USBHostHIDGamepad::available() {
  _ensureRegistered();
  USBHostHIDInstance.serviceReceives();
  return _has_report;
}

void USBHostHIDGamepad::clear() {
  _has_report = false;
}

uint16_t USBHostHIDGamepad::getReport(uint8_t *dst, uint16_t max_len) const {
  if (dst == nullptr || max_len == 0 || _report_len == 0) {
    return 0;
  }
  uint16_t n = _report_len;
  if (n > max_len) {
    n = max_len;
  }
  memcpy(dst, _report, n);
  return n;
}

uint16_t USBHostHIDGamepad::getButtons16() const {
  if (_report_len < 2) {
    return 0;
  }
  return (uint16_t)_report[0] | ((uint16_t)_report[1] << 8);
}

static int8_t u8_axis_to_s8(uint8_t v) {
  int16_t x = (int16_t)v - 128;
  if (x > 127) {
    x = 127;
  }
  if (x < -128) {
    x = -128;
  }
  return (int8_t)x;
}

void USBHostHIDGamepad::getSticks8(int8_t *lx, int8_t *ly, int8_t *rx, int8_t *ry,
                                   bool skip_id_byte) const {
  if (lx) {
    *lx = 0;
  }
  if (ly) {
    *ly = 0;
  }
  if (rx) {
    *rx = 0;
  }
  if (ry) {
    *ry = 0;
  }
  unsigned off = skip_id_byte ? 1u : 0u;
  if (_report_len < off + 5) {
    return;
  }
  const uint8_t *p = _report + off;
  if (lx) {
    *lx = u8_axis_to_s8(p[1]);
  }
  if (ly) {
    *ly = u8_axis_to_s8(p[2]);
  }
  if (rx) {
    *rx = u8_axis_to_s8(p[3]);
  }
  if (ry) {
    *ry = u8_axis_to_s8(p[4]);
  }
}

USBHostHIDGamepad USBHostGamepad;

#endif /* CFG_TUH_HID */
#endif /* SOC_USB_OTG_SUPPORTED */
