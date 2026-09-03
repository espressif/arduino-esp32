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
#include <stddef.h>

typedef void (*USBHostHIDGamepadReportCb)(const uint8_t *report, uint16_t len, void *arg);

/**
 * @brief USB Host HID gamepad / joystick.
 *
 * Claims report-protocol pads (Game Pad / Joystick heuristics). Report layout is
 * device-specific — use reportData() / getReport() or the callback for raw bytes.
 * Call registerWithHost() before USBHost.begin(). Register gamepad before mouse
 * when both are used so pads are not claimed as report-protocol mice.
 */
class USBHostHIDGamepad : public USBHostHIDDevice {
public:
  static const size_t REPORT_CAP = 64;

  USBHostHIDGamepad();

  bool claim(uint8_t dev_addr, uint8_t idx, uint8_t protocol, const uint8_t *report_desc, uint16_t desc_len) override;
  void onUnmount(uint8_t dev_addr, uint8_t idx) override;
  void onReport(uint8_t dev_addr, uint8_t idx, const uint8_t *report, uint16_t len) override;

  bool available();
  void clear();

  uint16_t reportLength() const {
    return _report_len;
  }
  const uint8_t *reportData() const {
    return _report;
  }
  uint16_t getReport(uint8_t *dst, uint16_t max_len) const;

  /** First 16 bits as little-endian button mask (if len >= 2). */
  uint16_t getButtons16() const;

  /** Heuristic 8-bit sticks (-128..127 from unsigned center-128); optional leading report ID. */
  void getSticks8(int8_t *lx, int8_t *ly, int8_t *rx, int8_t *ry, bool skip_id_byte = false) const;

  /**
   * Called from USBHost.task() (loop context), not from the TinyUSB worker.
   * Keep the callback reasonably short; Serial is OK here.
   */
  void setReportCallback(USBHostHIDGamepadReportCb cb, void *arg = nullptr) {
    _report_cb = cb;
    _report_cb_arg = arg;
  }

  /** Notify / available() only when report bytes change (default false). */
  void setNotifyOnChangeOnly(bool on) {
    _notify_on_change_only = on;
  }
  bool notifyOnChangeOnly() const {
    return _notify_on_change_only;
  }

  void registerWithHost() {
    _ensureRegistered();
  }

private:
  void _ensureRegistered();
  void dispatchReportCallback() override;

  uint8_t _report[REPORT_CAP];
  uint16_t _report_len;
  volatile bool _has_report;
  USBHostHIDGamepadReportCb _report_cb;
  void *_report_cb_arg;
  bool _notify_on_change_only;
  uint8_t _last_notified[REPORT_CAP];
  uint16_t _last_notified_len;
  volatile bool _cb_pending;
};

extern USBHostHIDGamepad USBHostGamepad;

#endif /* CFG_TUH_HID */
#endif /* SOC_USB_OTG_SUPPORTED */
