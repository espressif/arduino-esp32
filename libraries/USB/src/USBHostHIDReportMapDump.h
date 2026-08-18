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

#include <Arduino.h>

/** Pretty-print a HID report descriptor (uses USBHIDReportMapParse). */
void usbhid_print_parsed_report_map(Print &out, const uint8_t *hid_rm, size_t hid_rm_len);

#if __has_include("tusb_config.h")
#include "tusb_config.h"
#endif
#ifndef CFG_TUH_HID
#define CFG_TUH_HID 0
#endif

#if CFG_TUH_HID

#include "USBHostHID.h"

/**
 * Optional debug handler: prints each interface's report descriptor, never claims.
 * Register before other HID handlers if you want dumps for every iface.
 */
class USBHostHIDReportMapDumper : public USBHostHIDDevice {
public:
  explicit USBHostHIDReportMapDumper(Print *out = nullptr) : _out(out ? out : &Serial) {}

  void setPrint(Print *p) {
    _out = p ? p : &Serial;
  }

  bool claim(uint8_t dev_addr, uint8_t idx, uint8_t protocol, const uint8_t *report_desc,
             uint16_t desc_len) override {
    if (_out != nullptr) {
      _out->printf("\n--- HID report descriptor parse (dev=%u idx=%u boot_protocol=%u, len=%u) ---\n",
                   (unsigned)dev_addr, (unsigned)idx, (unsigned)protocol, (unsigned)desc_len);
      usbhid_print_parsed_report_map(*_out, report_desc, desc_len);
      _out->println("--- end parse ---\n");
    }
    return false;
  }

  void onUnmount(uint8_t, uint8_t) override {}
  void onReport(uint8_t, uint8_t, const uint8_t *, uint16_t) override {}

private:
  Print *_out;
};

#endif /* CFG_TUH_HID */
#endif /* SOC_USB_OTG_SUPPORTED */
