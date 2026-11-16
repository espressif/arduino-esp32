/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ZigbeeOTAFileParser.h"

#include <string.h>

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

void zb_ota_file_parser_init(zb_ota_file_parser_t *parser, uint32_t total_image_size) {
  memset(parser, 0, sizeof(*parser));
  parser->total_image_size = total_image_size;
}

void zb_ota_file_parser_setup(zb_ota_file_parser_t *parser, uint16_t block_size, uint8_t *block) {
  parser->in = block;
  parser->in_length = block_size;
}

bool zb_ota_file_parser_is_element_value(const zb_ota_file_parser_t *parser) {
  return parser && parser->element.length && parser->element.val;
}

esp_err_t zb_ota_file_parser_check(const zb_ota_file_parser_t *parser) {
  return parser && parser->total_image_size > 0 && parser->total_offset == parser->total_image_size ? ESP_OK : ESP_FAIL;
}

static uint16_t ota_file_stream_format(uint8_t *dst, uint16_t dst_offset, uint16_t dst_total, const uint8_t *src, uint16_t src_total) {
  uint16_t size = 0;
  if (dst_offset < dst_total) {
    size = MIN((uint16_t)(dst_total - dst_offset), src_total);
    memcpy(dst + dst_offset, src, size);
  }
  return size;
}

static void ota_file_parser_refresh(zb_ota_file_parser_t *parser, uint16_t written) {
  parser->offset += written;
  parser->total_offset += written;
  parser->in += written;
  parser->in_length = (uint16_t)(parser->in_length - written);
}

static ezb_zcl_ota_file_header_optional_t ota_file_parser_get_optional(uint16_t fc, const uint8_t *buffer) {
  ezb_zcl_ota_file_header_optional_t optional = {};
  uint16_t offset = 0;
  if (fc & EZB_ZCL_OTA_FILE_HEADER_FC_SECURITY_CREDENTIAL_VERSION_PRESENT) {
    memcpy(&optional.security_credential_version, buffer + offset, sizeof(uint8_t));
    offset += sizeof(uint8_t);
  }
  if (fc & EZB_ZCL_OTA_FILE_HEADER_FC_DEVICE_SPECIFIC) {
    memcpy(&optional.upgrade_file_destination, buffer + offset, sizeof(ezb_extaddr_t));
    offset += sizeof(ezb_extaddr_t);
  }
  if (fc & EZB_ZCL_OTA_FILE_HEADER_FC_HW_VERSION_PRESENT) {
    memcpy(&optional.minimum_hardware_version, buffer + offset, sizeof(uint16_t));
    offset += sizeof(uint16_t);
    memcpy(&optional.maximum_hardware_version, buffer + offset, sizeof(uint16_t));
  }
  return optional;
}

esp_err_t zb_ota_file_parser_process(zb_ota_file_parser_t *parser) {
  if (parser == nullptr || parser->in_length == 0 || parser->in == nullptr) {
    if (parser) {
      parser->element.length = 0;
      parser->element.val = nullptr;
    }
    return ESP_ERR_INVALID_ARG;
  }

  uint16_t written = 0;

  /* Mandatory OTA file header */
  if (sizeof(parser->header.mandatory) > parser->total_offset) {
    written = ota_file_stream_format(
      (uint8_t *)&parser->header.mandatory, (uint16_t)parser->offset, sizeof(parser->header.mandatory), parser->in, parser->in_length
    );
    ota_file_parser_refresh(parser, written);
    parser->offset = parser->total_offset >= sizeof(parser->header.mandatory) ? 0 : parser->offset;
    return parser->in_length ? ESP_ERR_NOT_FINISHED : ESP_OK;
  }

  /* Optional header fields */
  if (parser->header.mandatory.hdr_length > parser->total_offset) {
    written = ota_file_stream_format(
      (uint8_t *)&parser->header.optional, (uint16_t)parser->offset,
      (uint16_t)(parser->header.mandatory.hdr_length - sizeof(parser->header.mandatory)), parser->in, parser->in_length
    );
    ota_file_parser_refresh(parser, written);
    if (parser->total_offset >= parser->header.mandatory.hdr_length) {
      parser->offset = 0;
      parser->header.optional = ota_file_parser_get_optional(parser->header.mandatory.hdr_fc, (const uint8_t *)&parser->header.optional);
    }
    return parser->in_length ? ESP_ERR_NOT_FINISHED : ESP_OK;
  }

  /* Sub-element header (type + length) */
  if (parser->offset < ZB_OTA_ELEMENT_HEADER_LEN) {
    written =
      ota_file_stream_format((uint8_t *)&parser->element, (uint16_t)parser->offset, ZB_OTA_ELEMENT_HEADER_LEN, parser->in, parser->in_length);
    ota_file_parser_refresh(parser, written);
    parser->element.total += ZB_OTA_ELEMENT_HEADER_LEN;
    parser->element.length = 0;
    parser->element.val = nullptr;
    return parser->in_length ? ESP_ERR_NOT_FINISHED : ESP_OK;
  }

  /* Sub-element value (firmware bytes) */
  if (parser->offset < parser->element.total) {
    parser->element.length = MIN((uint16_t)(parser->element.total - parser->offset), parser->in_length);
    parser->element.val = parser->in;
    written = parser->element.length;
    ota_file_parser_refresh(parser, written);
    parser->offset = parser->offset >= parser->element.total ? 0 : parser->offset;
    return parser->in_length ? ESP_ERR_NOT_FINISHED : ESP_OK;
  }

  parser->element.length = 0;
  parser->element.val = nullptr;
  return ESP_OK;
}
