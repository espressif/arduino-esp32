/* Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 * Copyright (c) 2010-2011 mbed.org, MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 * and associated documentation files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef USBCLASS_HID_TYPES
#define USBCLASS_HID_TYPES

/**
 * @file HIDTypes.h
 * @brief HID Report Descriptor constants and macros.
 *
 * Provides the standard HID class constants, report types, and short-item
 * macros needed to build HID Report Descriptors for BLE HID devices.
 * Macro names follow the HID specification (Device Class Definition for
 * HID 1.11, Section 6.2.2).
 */

#include <stdint.h>

/**
 * @brief BCD value for HID specification version 1.11.
 */
#define HID_VERSION_1_11 (0x0111)

/* HID Class */
#define BLE_HID_CLASS         (3)
#define BLE_HID_SUBCLASS_NONE (0)
#define BLE_HID_PROTOCOL_NONE (0)

/* HID Report Types (Report Reference descriptor) */
#define HID_REPORT_TYPE_INPUT   (0x01)
#define HID_REPORT_TYPE_OUTPUT  (0x02)
#define HID_REPORT_TYPE_FEATURE (0x03)

/* Descriptors */
#define HID_DESCRIPTOR        (33)
#define HID_DESCRIPTOR_LENGTH (0x09)
#define REPORT_DESCRIPTOR     (34)

/* Class requests */
#define GET_REPORT (0x1)
#define GET_IDLE   (0x2)
#define SET_REPORT (0x9)
#define SET_IDLE   (0xa)

/* HID Class Report Descriptor */
/* Short items: size is 0, 1, 2 or 3 specifying 0, 1, 2 or 4 (four) bytes */
/* of data as per HID Class standard */

/* Main items */
#ifdef ARDUINO_ARCH_ESP32
#define HIDINPUT(size)  (0x80 | size)
#define HIDOUTPUT(size) (0x90 | size)
#else
#define INPUT(size)  (0x80 | size)
#define OUTPUT(size) (0x90 | size)
#endif
#define FEATURE(size)        (0xb0 | size)
#define COLLECTION(size)     (0xa0 | size)
#define END_COLLECTION(size) (0xc0 | size)

/* Global items */
#define USAGE_PAGE(size)       (0x04 | size)
#define LOGICAL_MINIMUM(size)  (0x14 | size)
#define LOGICAL_MAXIMUM(size)  (0x24 | size)
#define PHYSICAL_MINIMUM(size) (0x34 | size)
#define PHYSICAL_MAXIMUM(size) (0x44 | size)
#define UNIT_EXPONENT(size)    (0x54 | size)
#define UNIT(size)             (0x64 | size)
#define REPORT_SIZE(size)      (0x74 | size)  //bits
#define REPORT_ID(size)        (0x84 | size)
#define REPORT_COUNT(size)     (0x94 | size)  //bytes
#define PUSH(size)             (0xa4 | size)
#define POP(size)              (0xb4 | size)

/* Local items */
#define USAGE(size)              (0x08 | size)
#define USAGE_MINIMUM(size)      (0x18 | size)
#define USAGE_MAXIMUM(size)      (0x28 | size)
#define DESIGNATOR_INDEX(size)   (0x38 | size)
#define DESIGNATOR_MINIMUM(size) (0x48 | size)
#define DESIGNATOR_MAXIMUM(size) (0x58 | size)
#define STRING_INDEX(size)       (0x78 | size)
#define STRING_MINIMUM(size)     (0x88 | size)
#define STRING_MAXIMUM(size)     (0x98 | size)
#define DELIMITER(size)          (0xa8 | size)

/* BLE Appearance values — HID category (Bluetooth Assigned Numbers §2.6) */
#define BLE_APPEARANCE_HID_GENERIC         0x03C0
#define BLE_APPEARANCE_HID_KEYBOARD        0x03C1
#define BLE_APPEARANCE_HID_MOUSE           0x03C2
#define BLE_APPEARANCE_HID_JOYSTICK        0x03C3
#define BLE_APPEARANCE_HID_GAMEPAD         0x03C4
#define BLE_APPEARANCE_HID_DIGITIZER       0x03C5
#define BLE_APPEARANCE_HID_CARD_READER     0x03C6
#define BLE_APPEARANCE_HID_DIGITAL_PEN     0x03C7
#define BLE_APPEARANCE_HID_BARCODE_SCANNER 0x03C8

/* HID Report */
/* Where report IDs are used the first byte of 'data' will be the */
/* report ID and 'length' will include this report ID byte. */

/**
 * @brief Max bytes in one HID report buffer (including Report ID if used).
 */
#define MAX_HID_REPORT_SIZE (64)

/**
 * @brief Fixed-size buffer for a single HID report.
 *
 * When Report IDs are in use, the first byte of @c data is the report ID
 * and @c length includes that byte.
 */
typedef struct {
  uint32_t length;                    ///< Number of valid bytes in @c data.
  uint8_t data[MAX_HID_REPORT_SIZE];  ///< Report payload (including report ID if used).
} HID_REPORT;

#endif
