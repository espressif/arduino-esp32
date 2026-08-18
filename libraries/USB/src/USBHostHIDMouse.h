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
#include <stdint.h>
#include <stdbool.h>

/* Same button bits as device-side USBHIDMouse. */
#define MOUSE_LEFT     0x01
#define MOUSE_RIGHT    0x02
#define MOUSE_MIDDLE   0x04
#define MOUSE_BACKWARD 0x08
#define MOUSE_FORWARD  0x10

typedef void (*USBHostHIDMouseReportCb)(int8_t x, int8_t y, uint8_t buttons, int8_t wheel, void *arg);

/**
 * @brief USB Host HID mouse.
 *
 * Claims boot-protocol mice and many report-protocol mice (Generic Desktop + Mouse).
 * Skips descriptors that look like gamepad/joystick. Call registerWithHost() before
 * USBHost.begin().
 */
class USBHostHIDMouse : public USBHostHIDDevice {
public:
  USBHostHIDMouse();

  bool claim(uint8_t dev_addr, uint8_t idx, uint8_t protocol, const uint8_t *report_desc,
             uint16_t desc_len) override;
  void onUnmount(uint8_t dev_addr, uint8_t idx) override;
  void onReport(uint8_t dev_addr, uint8_t idx, const uint8_t *report, uint16_t len) override;

  bool available();
  int8_t getX() const {
    return _x;
  }
  int8_t getY() const {
    return _y;
  }
  uint8_t getButtons() const {
    return _buttons;
  }
  int8_t getWheel() const {
    return _wheel;
  }
  void clear();

  void setReportCallback(USBHostHIDMouseReportCb cb, void *arg = nullptr) {
    _report_cb = cb;
    _report_cb_arg = arg;
  }

  /** Register with USBHostHID before USBHost.begin(). */
  void registerWithHost() {
    _ensureRegistered();
  }

private:
  void _ensureRegistered();

  volatile int8_t _x;
  volatile int8_t _y;
  volatile uint8_t _buttons;
  volatile int8_t _wheel;
  volatile bool _has_report;
  bool _strip_report_id;  ///< report-protocol mice often prefix a report ID
  USBHostHIDMouseReportCb _report_cb;
  void *_report_cb_arg;
};

extern USBHostHIDMouse USBHostMouse;

#endif /* CFG_TUH_HID */
#endif /* SOC_USB_OTG_SUPPORTED */
