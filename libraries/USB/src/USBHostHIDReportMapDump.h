/*
 * HID report descriptor: pretty-print usbhid_parse_report_map() to a Print stream, and an
 * optional USBHostHIDDevice that dumps each interface’s descriptor (register it before other
 * handlers; claim() always returns false).
 *
 * Core parser: USBHIDReportMapParse.h / USBHIDReportMapParse.c
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "soc/soc_caps.h"
#if SOC_USB_OTG_SUPPORTED

#include <Arduino.h>

/** Pretty-print usbhid_parse_report_map() to @p out; frees the map. */
void usbhid_print_parsed_report_map(Print &out, const uint8_t *hid_rm, size_t hid_rm_len);

#if __has_include("tusb_config.h")
#include "tusb_config.h"
#endif
#ifndef CFG_TUH_HID
#define CFG_TUH_HID 0
#endif

#if CFG_TUH_HID

#include "USBHostHID.h"

class USBHostHIDReportMapDumper : public USBHostHIDDevice {
public:
  explicit USBHostHIDReportMapDumper(Print *out = nullptr) : _out(out ? out : &Serial) {}

  void setPrint(Print *p) { _out = p ? p : &Serial; }

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
