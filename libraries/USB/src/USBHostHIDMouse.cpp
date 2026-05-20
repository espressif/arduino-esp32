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

#include "USBHostHIDMouse.h"

#if SOC_USB_OTG_SUPPORTED
#if CFG_TUH_HID

#include "tusb.h"
#include "class/hid/hid.h"
#include "Arduino.h"

/** Same signals as USBHostHIDGamepad — do not treat controllers as mice. */
static bool desc_is_gamepad_or_joystick(const uint8_t *d, uint16_t len) {
  if (d == nullptr || len < 4) {
    return false;
  }
  for (uint16_t i = 0; i + 3 < len; i++) {
    if (d[i] == 0x05 && d[i + 1] == 0x01 && d[i + 2] == 0x09) {
      uint8_t u = d[i + 3];
      if (u == 0x04 || u == 0x05 || u == 0x08) {
        return true;
      }
    }
  }
  for (uint16_t i = 0; i + 4 < len; i++) {
    if (d[i] == 0x06 && d[i + 1] == 0x01 && d[i + 2] == 0x00 && d[i + 3] == 0x09 &&
        (d[i + 4] == 0x05 || d[i + 4] == 0x04 || d[i + 4] == 0x08)) {
      return true;
    }
  }
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

/** Desktop Hat Switch (0x39) — common on gamepads, not typical on mice. */
static bool desc_has_desktop_hat(const uint8_t *d, uint16_t len) {
  bool desk = false;
  for (uint16_t i = 0; i + 1 < len; i++) {
    if (d[i] == 0x05 && d[i + 1] == 0x01) {
      desk = true;
    }
    if (d[i] == 0x06 && i + 2 < len && d[i + 1] == 0x01 && d[i + 2] == 0x00) {
      desk = true;
    }
    if (desk && d[i] == 0x09 && i + 1 < len && d[i + 1] == 0x39) {
      return true;
    }
  }
  return false;
}

/** Report descriptor declares Generic Desktop Mouse (common when bInterfaceProtocol is 0). */
static bool desc_is_mouse(const uint8_t *d, uint16_t len) {
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
  for (uint16_t i = 0; i + 1 < len; i++) {
    if (d[i] != 0x05 || d[i + 1] != 0x01) {
      continue;
    }
    uint16_t end = i + 2 + 160;
    if (end > len) {
      end = len;
    }
    for (uint16_t j = i + 2; j + 1 < end; j++) {
      if (d[j] == 0x09 && d[j + 1] == 0x02) {
        return true;
      }
    }
  }
  return false;
}

USBHostHIDMouse::USBHostHIDMouse()
  : USBHostHIDDevice(),
    _x(0), _y(0), _buttons(0), _wheel(0), _has_report(false), _strip_report_id(false),
    _report_cb(nullptr), _report_cb_arg(nullptr) {
}

void USBHostHIDMouse::_ensureRegistered() {
  static bool registered = false;
  if (!registered) {
    registered = true;
    USBHostHIDInstance.addDevice(this);
  }
}

bool USBHostHIDMouse::claim(uint8_t dev_addr, uint8_t idx, uint8_t protocol,
                            const uint8_t *report_desc, uint16_t desc_len) {
  bool boot_mouse = (protocol == HID_ITF_PROTOCOL_MOUSE);
  bool report_mouse = false;
  if (!boot_mouse) {
    if (protocol == HID_ITF_PROTOCOL_KEYBOARD) {
      return false;
    }
    if (desc_is_gamepad_or_joystick(report_desc, desc_len) || desc_has_desktop_hat(report_desc, desc_len)) {
      return false;
    }
    report_mouse = desc_is_mouse(report_desc, desc_len);
    if (!report_mouse) {
      return false;
    }
  }

  if (!bindHidInterface(dev_addr, idx)) {
    return false;
  }
  _has_report = false;
  _strip_report_id = report_mouse;
  log_v("[USBHostMouse] claim dev=%u idx=%u boot=%u", (unsigned)dev_addr, (unsigned)idx,
        (unsigned)(boot_mouse ? 1u : 0u));
  return true;
}

void USBHostHIDMouse::onUnmount(uint8_t dev_addr, uint8_t idx) {
  if (dev_addr == _dev_addr && idx == _idx) {
    log_v("[USBHostMouse] unmount dev_addr=%u idx=%u", (unsigned)dev_addr, (unsigned)idx);
    clearHidInterfaceBinding();
    _strip_report_id = false;
  }
}

void USBHostHIDMouse::onReport(uint8_t dev_addr, uint8_t idx, const uint8_t *report, uint16_t len) {
  const uint8_t *p = report;
  uint16_t n = len;
  if (_strip_report_id && n > 3u) {
    p++;
    n--;
  }

  if (n >= 3u) {
    _buttons = p[0];
    _x = (int8_t)p[1];
    _y = (int8_t)p[2];
    _wheel = (n >= 4u) ? (int8_t)p[3] : 0;
    _has_report = true;
    if (_report_cb) {
      _report_cb(_x, _y, _buttons, _wheel, _report_cb_arg);
    }
    log_v("[USBHostMouse] report len=%u x=%d y=%d btns=0x%02x wheel=%d",
          (unsigned)len, (int)_x, (int)_y, (unsigned)_buttons, (int)_wheel);
  } else {
    log_v("[USBHostMouse] report len=%u (ignored, need >= 3)", (unsigned)len);
  }
  tuh_hid_receive_report(dev_addr, idx);
}

bool USBHostHIDMouse::available() {
  _ensureRegistered();
  USBHostHIDInstance.serviceReceives();
  return _has_report;
}

void USBHostHIDMouse::clear() {
  _has_report = false;
}

USBHostHIDMouse USBHostMouse;

#endif /* CFG_TUH_HID */
#endif /* SOC_USB_OTG_SUPPORTED */
