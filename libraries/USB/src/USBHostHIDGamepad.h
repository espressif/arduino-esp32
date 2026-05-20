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
#include <stddef.h>

/** Optional callback when a gamepad report is received. */
typedef void (*USBHostHIDGamepadReportCb)(const uint8_t *report, uint16_t len, void *arg);

/**
 * @brief USB Host HID Gamepad / Joystick (Arduino API).
 *
 * Claims HID interfaces whose report descriptor indicates Generic Desktop
 * Game Pad (usage 0x05) or Joystick (0x04). Most USB gamepads use report
 * protocol (not boot mouse), so they are handled here rather than USBHostMouse.
 *
 * Report layout is device-specific. This class stores the raw report and
 * offers helpers for common 8-bit gamepad layouts; use reportData() /
 * reportLength() or the callback for full dumps.
 */
class USBHostHIDGamepad : public USBHostHIDDevice {
public:
  static const size_t REPORT_CAP = 64;

  USBHostHIDGamepad();

  bool claim(uint8_t dev_addr, uint8_t idx, uint8_t protocol,
             const uint8_t *report_desc, uint16_t desc_len) override;
  void onUnmount(uint8_t dev_addr, uint8_t idx) override;
  void onReport(uint8_t dev_addr, uint8_t idx, const uint8_t *report, uint16_t len) override;

  bool available();
  void clear();

  /** Length of last INPUT report (0 if none). */
  uint16_t reportLength() const { return _report_len; }

  /** Pointer to last report (valid until next report or unmount). */
  const uint8_t *reportData() const { return _report; }

  /** Copy last report into dst; returns bytes copied. */
  uint16_t getReport(uint8_t *dst, uint16_t max_len) const;

  /**
   * @brief Heuristic: first 16 bits as button bitmask (little-endian), if len >= 2.
   */
  uint16_t getButtons16() const;

  /**
   * @brief Heuristic for simple 8-bit pads: axes as signed -128..127 from unsigned 0..255 (center 128).
   * Uses bytes at offsets 1..4 if report length allows (after optional 1-byte report ID).
   * If report[0] is often 0..15 as ID, set skip_id_byte true when your pad sends report ID first.
   */
  void getSticks8(int8_t *lx, int8_t *ly, int8_t *rx, int8_t *ry, bool skip_id_byte = false) const;

  void setReportCallback(USBHostHIDGamepadReportCb cb, void *arg = nullptr) {
    _report_cb = cb;
    _report_cb_arg = arg;
  }

  /**
   * @brief If true, the report callback runs and available() becomes true only when
   * the INPUT report differs from the last notified report (memcmp). Use this to
   * avoid polling/spam when the device repeats identical reports. First report after
   * mount always counts as a change. Default is false (every report notifies).
   */
  void setNotifyOnChangeOnly(bool on) { _notify_on_change_only = on; }
  bool notifyOnChangeOnly() const { return _notify_on_change_only; }

  /**
   * @brief Register this handler with the USB HID host. Call once in setup()
   * **before** USBHost.begin(). If the gamepad is already powered when begin()
   * runs, enumeration can finish before loop() — then handlers registered only
   * from available() never see mount, and nothing will claim the device.
   */
  void registerWithHost() { _ensureRegistered(); }

private:
  void _ensureRegistered();

  uint8_t _report[REPORT_CAP];
  uint16_t _report_len;
  volatile bool _has_report;
  USBHostHIDGamepadReportCb _report_cb;
  void *_report_cb_arg;
  bool _notify_on_change_only;
  uint8_t _last_notified[REPORT_CAP];
  uint16_t _last_notified_len;
};

extern USBHostHIDGamepad USBHostGamepad;

#endif /* CFG_TUH_HID */
#endif /* SOC_USB_OTG_SUPPORTED */
