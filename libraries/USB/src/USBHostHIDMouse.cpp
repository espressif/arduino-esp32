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

/** Walk short/long HID items. */
static bool hid_desc_next(const uint8_t *d, uint16_t len, uint16_t *ioff, uint8_t *tag, uint32_t *data) {
  uint16_t i = *ioff;
  if (i >= len) {
    return false;
  }
  const uint8_t b0 = d[i++];
  if (b0 == 0xFE) {
    if ((uint16_t)(i + 2) > len) {
      return false;
    }
    const uint8_t data_size = d[i++];
    *tag = d[i++];
    *data = 0;
    if ((uint16_t)(i + data_size) > len) {
      return false;
    }
    i = (uint16_t)(i + data_size);
    *ioff = i;
    return true;
  }
  uint8_t sz = b0 & 0x03u;
  if (sz == 3u) {
    sz = 4u;
  }
  *tag = (uint8_t)(b0 & 0xFCu);
  *data = 0;
  for (uint8_t b = 0; b < sz; b++) {
    if (i >= len) {
      return false;
    }
    *data |= (uint32_t)d[i++] << (8 * b);
  }
  *ioff = i;
  return true;
}

/**
 * Probe mouse report layout from the descriptor.
 * Many modern mice use 16-bit X/Y; treating them as boot 8-bit puts the wheel at the wrong byte.
 */
static void desc_probe_mouse_layout(const uint8_t *d, uint16_t len, bool *has_report_id, uint8_t *xy_bytes) {
  *has_report_id = false;
  *xy_bytes = 1;

  if (d == nullptr || len == 0) {
    return;
  }

  uint16_t off = 0;
  uint8_t tag = 0;
  uint32_t data = 0;
  uint16_t usage_page = 0;
  uint8_t report_size = 8;
  uint8_t usages[8];
  uint8_t n_usage = 0;

  while (hid_desc_next(d, len, &off, &tag, &data)) {
    switch (tag) {
      case 0x04: /* USAGE_PAGE */
        usage_page = (uint16_t)data;
        n_usage = 0;
        break;
      case 0x08: /* USAGE */
        if (n_usage < sizeof(usages)) {
          usages[n_usage++] = (uint8_t)data;
        }
        break;
      case 0x18: /* USAGE_MIN — treat as one usage for packing heuristics */
        if (n_usage < sizeof(usages)) {
          usages[n_usage++] = (uint8_t)data;
        }
        break;
      case 0x74: /* REPORT_SIZE */
        report_size = (uint8_t)data;
        break;
      case 0x84: /* REPORT_ID */
        *has_report_id = true;
        break;
      case 0x80: /* INPUT */
        if (usage_page == 0x01u) {
          for (uint8_t u = 0; u < n_usage; u++) {
            if (usages[u] == 0x30u || usages[u] == 0x31u) { /* X or Y */
              *xy_bytes = (report_size >= 16u) ? 2u : 1u;
              break;
            }
          }
        }
        n_usage = 0;
        break;
      case 0xA0: /* COLLECTION */
      case 0xC0: /* END_COLLECTION */
        n_usage = 0;
        break;
      default:
        break;
    }
  }
}

static int16_t rd_axis(const uint8_t *p, uint8_t nbytes) {
  if (nbytes >= 2u) {
    return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
  }
  return (int16_t)(int8_t)p[0];
}

USBHostHIDMouse::USBHostHIDMouse()
  : USBHostHIDDevice(),
    _x(0), _y(0), _buttons(0), _wheel(0), _has_report(false), _strip_report_id(false), _xy_bytes(1),
    _report_cb(nullptr), _report_cb_arg(nullptr) {
}

void USBHostHIDMouse::_ensureRegistered() {
  static bool registered = false;
  if (!registered) {
    registered = true;
    USBHostHID.addDevice(this);
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

  bool has_rid = false;
  uint8_t xy_bytes = 1;
  desc_probe_mouse_layout(report_desc, desc_len, &has_rid, &xy_bytes);
  /*
   * Boot-protocol interfaces often still expose a Report-protocol descriptor (16-bit X/Y),
   * but keep sending short boot reports (buttons + X8 + Y8 [+ wheel]). Prefer the descriptor
   * hint only as a default; onReport() picks 8 vs 16 from the actual payload length.
   */
  _strip_report_id = has_rid;
  _xy_bytes = boot_mouse ? 1u : xy_bytes;

  /*
   * TinyUSB enumerates boot-capable mice in Boot protocol by default. The boot mouse
   * report is fixed at 3 bytes (buttons, X, Y) — no wheel. Switch to Report protocol
   * so the device sends its full descriptor layout (wheel / 16-bit axes / report ID).
   */
  if (tuh_hid_get_protocol(dev_addr, idx) != HID_PROTOCOL_REPORT) {
    if (tuh_hid_set_protocol(dev_addr, idx, HID_PROTOCOL_REPORT)) {
      log_v("[USBHostMouse] set_protocol(REPORT) queued (boot report has no wheel)");
    } else {
      log_w("[USBHostMouse] set_protocol(REPORT) submit failed");
    }
  }

  log_v("[USBHostMouse] claim dev=%u idx=%u boot=%u rid=%u xy_hint=%u", (unsigned)dev_addr, (unsigned)idx,
        (unsigned)(boot_mouse ? 1u : 0u), (unsigned)(_strip_report_id ? 1u : 0u), (unsigned)_xy_bytes);
  return true;
}

void USBHostHIDMouse::onUnmount(uint8_t dev_addr, uint8_t idx) {
  if (dev_addr == _dev_addr && idx == _idx) {
    log_v("[USBHostMouse] unmount dev_addr=%u idx=%u", (unsigned)dev_addr, (unsigned)idx);
    clearHidInterfaceBinding();
    _strip_report_id = false;
    _xy_bytes = 1;
  }
}

void USBHostHIDMouse::onReport(uint8_t dev_addr, uint8_t idx, const uint8_t *report, uint16_t len) {
  (void)dev_addr;
  (void)idx;
  if (report == nullptr || len < 3u) {
    return;
  }

  auto parse_at = [&](const uint8_t *q, uint16_t m) -> bool {
    if (m < 3u) {
      return false;
    }
    /* Length wins over descriptor: short payloads are boot-like 8-bit X/Y. */
    uint8_t xy_bytes;
    if (m >= 6u) {
      xy_bytes = 2u;
    } else if (m == 5u && _xy_bytes >= 2u) {
      xy_bytes = 2u; /* buttons + X16 + Y16 (no wheel) */
    } else {
      xy_bytes = 1u;
    }

    const uint16_t need = (uint16_t)(1u + (uint16_t)xy_bytes * 2u);
    if (m < need) {
      return false;
    }

    _buttons = q[0];
    _x = rd_axis(q + 1, xy_bytes);
    _y = rd_axis(q + 1 + xy_bytes, xy_bytes);
    _wheel = (m > need) ? (int8_t)q[need] : 0;
    return true;
  };

  bool ok = false;
  if (_strip_report_id) {
    ok = parse_at(report + 1, (uint16_t)(len - 1u));
  }
  if (!ok) {
    ok = parse_at(report, len);
  }
  if (!ok && !_strip_report_id && len > 3u) {
    ok = parse_at(report + 1, (uint16_t)(len - 1u));
  }
  if (!ok) {
    return;
  }

  log_v("[USBHostMouse] report len=%u btn=0x%02x dx=%d dy=%d wheel=%d", (unsigned)len, (unsigned)_buttons,
        (int)_x, (int)_y, (int)_wheel);

  _has_report = true;
  if (_report_cb) {
    _report_cb(_x, _y, _buttons, _wheel, _report_cb_arg);
  }
}

bool USBHostHIDMouse::available() {
  _ensureRegistered();
  USBHostHID.serviceReceives();
  return _has_report;
}

void USBHostHIDMouse::clear() {
  _has_report = false;
}

USBHostHIDMouse USBHostMouse;

#endif /* CFG_TUH_HID */
#endif /* SOC_USB_OTG_SUPPORTED */
