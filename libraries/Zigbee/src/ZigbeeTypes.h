// Copyright 2025 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "esp_zigbee_core.h"

// Foundation Command Types
typedef enum {
  ZB_CMD_READ_ATTRIBUTE = 0x00U,                        /*!< Read attributes command */
  ZB_CMD_READ_ATTRIBUTE_RESPONSE = 0x01U,               /*!< Read attributes response command */
  ZB_CMD_WRITE_ATTRIBUTE = 0x02U,                       /*!< Write attributes foundation command */
  ZB_CMD_WRITE_ATTRIBUTE_UNDIVIDED = 0x03U,             /*!< Write attributes undivided command */
  ZB_CMD_WRITE_ATTRIBUTE_RESPONSE = 0x04U,              /*!< Write attributes response command */
  ZB_CMD_WRITE_ATTRIBUTE_NO_RESPONSE = 0x05U,           /*!< Write attributes no response command */
  ZB_CMD_CONFIGURE_REPORTING = 0x06U,                   /*!< Configure reporting command */
  ZB_CMD_CONFIGURE_REPORTING_RESPONSE = 0x07U,          /*!< Configure reporting response command */
  ZB_CMD_READ_REPORTING_CONFIG = 0x08U,                 /*!< Read reporting config command */
  ZB_CMD_READ_REPORTING_CONFIG_RESPONSE = 0x09U,        /*!< Read reporting config response command */
  ZB_CMD_REPORT_ATTRIBUTE = 0x0aU,                      /*!< Report attribute command */
  ZB_CMD_DEFAULT_RESPONSE = 0x0bU,                      /*!< Default response command */
  ZB_CMD_DISCOVER_ATTRIBUTES = 0x0cU,                   /*!< Discover attributes command */
  ZB_CMD_DISCOVER_ATTRIBUTES_RESPONSE = 0x0dU,          /*!< Discover attributes response command */
  ZB_CMD_READ_ATTRIBUTE_STRUCTURED = 0x0eU,             /*!< Read attributes structured */
  ZB_CMD_WRITE_ATTRIBUTE_STRUCTURED = 0x0fU,            /*!< Write attributes structured */
  ZB_CMD_WRITE_ATTRIBUTE_STRUCTURED_RESPONSE = 0x10U,   /*!< Write attributes structured response */
  ZB_CMD_DISCOVER_COMMANDS_RECEIVED = 0x11U,            /*!< Discover Commands Received command */
  ZB_CMD_DISCOVER_COMMANDS_RECEIVED_RESPONSE = 0x12U,   /*!< Discover Commands Received response command */
  ZB_CMD_DISCOVER_COMMANDS_GENERATED = 0x13U,           /*!< Discover Commands Generated command */
  ZB_CMD_DISCOVER_COMMANDS_GENERATED_RESPONSE = 0x14U,  /*!< Discover Commands Generated response command */
  ZB_CMD_DISCOVER_ATTRIBUTES_EXTENDED = 0x15U,          /*!< Discover attributes extended command */
  ZB_CMD_DISCOVER_ATTRIBUTES_EXTENDED_RESPONSE = 0x16U, /*!< Discover attributes extended response command */
} zb_cmd_type_t;
