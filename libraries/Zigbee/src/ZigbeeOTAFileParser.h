/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 *
 * Streaming parser for Zigbee OTA upgrade files (header + sub-element + image).
 * Adapted from esp-zigbee-sdk examples/ota_upgrade/ota_client/ota_file_parser.
 */
#pragma once

#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "ezbee/zcl/cluster/ota_file.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ZB_OTA_ELEMENT_HEADER_LEN (sizeof(uint16_t) + sizeof(uint32_t))

typedef enum {
  ZB_OTA_ELEMENT_UPGRADE_IMAGE = 0x0000,
} zb_ota_element_tag_id_t;

typedef struct {
  uint32_t offset;           /*!< Offset within the current header/element section */
  uint32_t total_offset;     /*!< Bytes consumed from the full OTA file */
  uint32_t total_image_size; /*!< Total Image Size from the OTA header / START event */
  uint16_t in_length;        /*!< Remaining bytes in the current input block */
  uint8_t *in;               /*!< Current position in the input block */

  struct {
    ezb_zcl_ota_file_header_t mandatory;
    ezb_zcl_ota_file_header_optional_t optional;
  } __attribute__((packed)) header;

  struct {
    uint16_t type;
    uint32_t total;  /*!< Value length + element header once header is complete */
    uint16_t length; /*!< Bytes of element value available in this process step */
    uint8_t *val;    /*!< Pointer into the input block (zero-copy) */
  } __attribute__((packed)) element;
} zb_ota_file_parser_t;

void zb_ota_file_parser_init(zb_ota_file_parser_t *parser, uint32_t total_image_size);
void zb_ota_file_parser_setup(zb_ota_file_parser_t *parser, uint16_t block_size, uint8_t *block);
bool zb_ota_file_parser_is_element_value(const zb_ota_file_parser_t *parser);
esp_err_t zb_ota_file_parser_process(zb_ota_file_parser_t *parser);
esp_err_t zb_ota_file_parser_check(const zb_ota_file_parser_t *parser);

#ifdef __cplusplus
}
#endif

#endif  // CONFIG_ZB_ENABLED
