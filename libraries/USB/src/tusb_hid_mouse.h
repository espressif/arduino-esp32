#pragma once

#include "class/hid/hid_device.h"

#if !defined TUD_HID_REPORT_DESC_ABSMOUSE
// This version of arduino-esp32 does not handle absolute mouse natively.
// Let's throw a minimalistic implementation of absmouse driver.
// See: https://github.com/hathach/tinyusb/pull/1363
// Also see: https://github.com/espressif/arduino-esp32/pull/6331

extern "C" {

  // Absolute Mouse data struct is a copy of the relative mouse struct
  // with int16_t instead of int8_t for X and Y coordinates.
  typedef struct TU_ATTR_PACKED {
    uint8_t buttons = 0;
    int16_t x = 0;
    int16_t y = 0;
    int8_t wheel = 0;
    int8_t pan = 0;
  } hid_abs_mouse_report_t;


// Absolute Mouse Report Descriptor Template applies those datatype changes too
#define TUD_HID_REPORT_DESC_ABSMOUSE(...) \
  HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP), \
    HID_USAGE(HID_USAGE_DESKTOP_MOUSE), \
    HID_COLLECTION(HID_COLLECTION_APPLICATION), /* Report ID if any */ \
    __VA_ARGS__ \
    HID_USAGE(HID_USAGE_DESKTOP_POINTER), \
    HID_COLLECTION(HID_COLLECTION_PHYSICAL), \
    HID_USAGE_PAGE(HID_USAGE_PAGE_BUTTON), \
    HID_USAGE_MIN(1), \
    HID_USAGE_MAX(5), \
    HID_LOGICAL_MIN(0), \
    HID_LOGICAL_MAX(1), /* Left, Right, Middle, Backward, Forward buttons */ \
    HID_REPORT_COUNT(5), \
    HID_REPORT_SIZE(1), \
    HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE), /* 3 bit padding */ \
    HID_REPORT_COUNT(1), \
    HID_REPORT_SIZE(3), \
    HID_INPUT(HID_CONSTANT), \
    HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP), /* X, Y absolute position [0, 32767] */ \
    HID_USAGE(HID_USAGE_DESKTOP_X), \
    HID_USAGE(HID_USAGE_DESKTOP_Y), \
    HID_LOGICAL_MIN(0x00), \
    HID_LOGICAL_MAX_N(0x7FFF, 2), \
    HID_REPORT_SIZE(16), \
    HID_REPORT_COUNT(2), \
    HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE), /* Vertical wheel scroll [-127, 127] */ \
    HID_USAGE(HID_USAGE_DESKTOP_WHEEL), \
    HID_LOGICAL_MIN(0x81), \
    HID_LOGICAL_MAX(0x7f), \
    HID_REPORT_COUNT(1), \
    HID_REPORT_SIZE(8), \
    HID_INPUT(HID_DATA | HID_VARIABLE | HID_RELATIVE), \
    HID_USAGE_PAGE(HID_USAGE_PAGE_CONSUMER), /* Horizontal wheel scroll [-127, 127] */ \
    HID_USAGE_N(HID_USAGE_CONSUMER_AC_PAN, 2), \
    HID_LOGICAL_MIN(0x81), \
    HID_LOGICAL_MAX(0x7f), \
    HID_REPORT_COUNT(1), \
    HID_REPORT_SIZE(8), \
    HID_INPUT(HID_DATA | HID_VARIABLE | HID_RELATIVE), \
    HID_COLLECTION_END, \
    HID_COLLECTION_END

  static inline bool tud_hid_n_abs_mouse_report(uint8_t instance, uint8_t report_id, uint8_t buttons, int16_t x, int16_t y, int8_t vertical, int8_t horizontal) {
    hid_abs_mouse_report_t report = {
      .buttons = buttons,
      .x = x,
      .y = y,
      .wheel = vertical,
      .pan = horizontal
    };
    return tud_hid_n_report(instance, report_id, &report, sizeof(report));
  }

  static inline bool tud_hid_abs_mouse_report(uint8_t report_id, uint8_t buttons, int16_t x, int16_t y, int8_t vertical, int8_t horizontal) {
    return tud_hid_n_abs_mouse_report(0, report_id, buttons, x, y, vertical, horizontal);
  }


}  // end extern "C"

#else
#pragma message "This file is now safe to delete along with its include from USBHIDMouse.h"
#endif
