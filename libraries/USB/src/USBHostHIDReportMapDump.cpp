/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "USBHostHIDReportMapDump.h"

#if SOC_USB_OTG_SUPPORTED

#include "USBHIDReportMapParse.h"

void usbhid_print_parsed_report_map(Print &out, const uint8_t *hid_rm, size_t hid_rm_len) {
  if (hid_rm == nullptr || hid_rm_len == 0) {
    out.println("usbhid_print_parsed_report_map: empty descriptor");
    return;
  }

  usbhid_report_map_t *map = usbhid_parse_report_map(hid_rm, hid_rm_len);
  if (map == nullptr) {
    out.println("usbhid_parse_report_map failed");
    return;
  }

  out.printf("Parsed HID report map: usage=%s appearance=0x%04x report_items=%u\n",
             usbhid_usage_str(map->usage), (unsigned)map->appearance, (unsigned)map->reports_len);

  for (unsigned i = 0; i < (unsigned)map->reports_len; i++) {
    const usbhid_report_item_t *r = &map->reports[i];
    out.printf("  [%u] report_id=%u type=%s protocol=%s len=%u usage=%s\n", i, (unsigned)r->report_id,
               usbhid_report_type_str(r->report_type), usbhid_protocol_mode_str(r->protocol_mode),
               (unsigned)r->value_len, usbhid_usage_str(r->usage));
  }

  usbhid_free_report_map(map);
}

#endif /* SOC_USB_OTG_SUPPORTED */
