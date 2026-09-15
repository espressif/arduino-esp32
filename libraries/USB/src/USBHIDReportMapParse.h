/*
 * HID Report descriptor parser — extracted from ESP-IDF components/esp_hid (esp_hid_common.c).
 * SPDX-FileCopyrightText: 2017-2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 *
 * Use this for USB (or any transport): pass GET_DESCRIPTOR(REPORT) bytes to usbhid_parse_report_map().
 * No dependency on Bluetooth or esp_hid component.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* HID Report Map item tags (short items, bTag in high nibble of first byte) */
#define USBHID_RM_INPUT           0x80
#define USBHID_RM_OUTPUT          0x90
#define USBHID_RM_FEATURE         0xb0
#define USBHID_RM_COLLECTION      0xa0
#define USBHID_RM_END_COLLECTION  0xc0
#define USBHID_RM_USAGE_PAGE      0x04
#define USBHID_RM_LOGICAL_MINIMUM 0x14
#define USBHID_RM_LOGICAL_MAXIMUM 0x24
#define USBHID_RM_PHYSICAL_MINIMUM 0x34
#define USBHID_RM_PHYSICAL_MAXIMUM 0x44
#define USBHID_RM_UNIT_EXPONENT   0x54
#define USBHID_RM_UNIT            0x64
#define USBHID_RM_REPORT_SIZE     0x74
#define USBHID_RM_REPORT_ID       0x84
#define USBHID_RM_REPORT_COUNT    0x94
#define USBHID_RM_PUSH            0xa4
#define USBHID_RM_POP             0xb4
#define USBHID_RM_USAGE           0x08
#define USBHID_RM_USAGE_MINIMUM   0x18
#define USBHID_RM_USAGE_MAXIMUM   0x28

/* Values inside HID Report descriptor (not the same as usbhid_report_usage_t). */
#define USBHID_RD_USAGE_PAGE_GENERIC_DESKTOP 0x01u
#define USBHID_RD_USAGE_KEYBOARD             0x06u
#define USBHID_RD_USAGE_MOUSE                0x02u
#define USBHID_RD_USAGE_JOYSTICK             0x04u
#define USBHID_RD_USAGE_GAMEPAD              0x05u
#define USBHID_RD_USAGE_PAGE_CONSUMER        0x0Cu
#define USBHID_RD_USAGE_CONSUMER_CONTROL     0x01u

#define USBHID_REPORT_TYPE_INPUT   1
#define USBHID_REPORT_TYPE_OUTPUT  2
#define USBHID_REPORT_TYPE_FEATURE 3

#define USBHID_PROTOCOL_MODE_BOOT   0x00u
#define USBHID_PROTOCOL_MODE_REPORT 0x01u

#define USBHID_APPEARANCE_GENERIC  0x03C0u
#define USBHID_APPEARANCE_KEYBOARD 0x03C1u
#define USBHID_APPEARANCE_MOUSE    0x03C2u
#define USBHID_APPEARANCE_JOYSTICK 0x03C3u
#define USBHID_APPEARANCE_GAMEPAD  0x03C4u

typedef enum {
  USBHID_USAGE_GENERIC = 0,
  USBHID_USAGE_KEYBOARD = 1,
  USBHID_USAGE_MOUSE = 2,
  USBHID_USAGE_JOYSTICK = 4,
  USBHID_USAGE_GAMEPAD = 8,
  USBHID_USAGE_TABLET = 16,
  USBHID_USAGE_CCONTROL = 32,
  USBHID_USAGE_VENDOR = 64
} usbhid_report_usage_t;

typedef struct {
  uint8_t map_index;
  uint8_t report_id;
  uint8_t report_type;
  uint8_t protocol_mode;
  usbhid_report_usage_t usage;
  uint16_t value_len; /* report length in bytes */
} usbhid_report_item_t;

typedef struct {
  usbhid_report_usage_t usage;
  uint16_t appearance;
  uint8_t reports_len;
  usbhid_report_item_t *reports;
} usbhid_report_map_t;

/**
 * @brief Parse a raw HID Report descriptor (e.g. from USB GET_DESCRIPTOR).
 * @return Parsed map; caller must usbhid_free_report_map(). NULL on failure.
 *
 * Not reentrant: parse state is file-scope. Call from one task only.
 */
usbhid_report_map_t *usbhid_parse_report_map(const uint8_t *hid_rm, size_t hid_rm_len);

void usbhid_free_report_map(usbhid_report_map_t *map);

const char *usbhid_usage_str(usbhid_report_usage_t usage);
const char *usbhid_protocol_mode_str(uint8_t protocol_mode);
const char *usbhid_report_type_str(uint8_t report_type);

#ifdef __cplusplus
}
#endif
