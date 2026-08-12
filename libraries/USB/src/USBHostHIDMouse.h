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

// Boot protocol mouse button bits (same as device USBHIDMouse)
#define MOUSE_LEFT     0x01
#define MOUSE_RIGHT    0x02
#define MOUSE_MIDDLE   0x04
#define MOUSE_BACKWARD 0x08
#define MOUSE_FORWARD  0x10

/** Optional callback when a mouse report is received (called from onReport). */
typedef void (*USBHostHIDMouseReportCb)(int8_t x, int8_t y, uint8_t buttons, int8_t wheel, void *arg);

/**
 * @brief USB Host HID Mouse (Arduino API).
 * Implements USBHostHIDDevice and claims HID interfaces with boot protocol Mouse.
 * Call USBHost.begin() and USBHost.task() in loop(); add USBHostHID.addDevice(&USBHostMouse)
 * in setup() (or the mouse registers itself on first use). Read getX(), getY(), getButtons(),
 * getWheel() when available() is true. getButtons() uses the same MOUSE_LEFT / MOUSE_RIGHT /
 * MOUSE_MIDDLE / … bit masks as device-side USBHIDMouse.
 * Also claims many **report-protocol** mice (bInterfaceProtocol 0) whose descriptor declares
 * Generic Desktop + Mouse; the first report byte is treated as a report ID when applicable.
 * Does not claim descriptors that look like a gamepad/joystick or include a Desktop Hat switch.
 */
class USBHostHIDMouse : public USBHostHIDDevice {
public:
  USBHostHIDMouse();

  // USBHostHIDDevice implementation
  bool claim(uint8_t dev_addr, uint8_t idx, uint8_t protocol,
             const uint8_t *report_desc, uint16_t desc_len) override;
  void onUnmount(uint8_t dev_addr, uint8_t idx) override;
  void onReport(uint8_t dev_addr, uint8_t idx, const uint8_t *report, uint16_t len) override;

  /**
   * @brief Check if a mouse report has been received since last clear().
   * Also calls USBHostHID.serviceReceives() (no-op when the host worker is active).
   */
  bool available();

  /**
   * @brief Relative X movement (signed).
   */
  int8_t getX() const { return _x; }

  /**
   * @brief Relative Y movement (signed).
   */
  int8_t getY() const { return _y; }

  /**
   * @brief Button state bitmask (MOUSE_LEFT, MOUSE_RIGHT, MOUSE_MIDDLE, etc.).
   */
  uint8_t getButtons() const { return _buttons; }

  /**
   * @brief Scroll wheel delta (signed; 0 if no wheel or 3-byte report).
   */
  int8_t getWheel() const { return _wheel; }

  /**
   * @brief Clear the last report (so available() becomes false until next report).
   */
  void clear();

  /**
   * @brief Set optional callback for each mouse report (movement, buttons, wheel).
   * Called from the HID callback context when a report arrives; use for
   * event-driven handling instead of polling available() / getX() / getY().
   * Pass nullptr to disable.
   */
  void setReportCallback(USBHostHIDMouseReportCb cb, void *arg = nullptr) {
    _report_cb = cb;
    _report_cb_arg = arg;
  }

  /** Call in setup() before USBHost.begin() if the mouse may enumerate before loop(). */
  void registerWithHost() { _ensureRegistered(); }

private:
  void _ensureRegistered();

  volatile int8_t _x;
  volatile int8_t _y;
  volatile uint8_t _buttons;
  volatile int8_t _wheel;
  volatile bool _has_report;
  /** When true (report-protocol mouse), skip leading report ID byte before boot-style parsing. */
  bool _strip_report_id;
  USBHostHIDMouseReportCb _report_cb;
  void *_report_cb_arg;
};

extern USBHostHIDMouse USBHostMouse;

#endif /* CFG_TUH_HID */
#endif /* SOC_USB_OTG_SUPPORTED */
