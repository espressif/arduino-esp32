/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_bit_defs.h"
#include "sdkconfig.h"
#include "soc/soc_caps.h"
#ifdef CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL
#include "esp_ds.h"
#endif

/*
 * Plase note that no two TLV structures of the same type
 * can be stored in the esp_secure_cert partition at one time.
 */
typedef enum esp_secure_cert_tlv_type {
    ESP_SECURE_CERT_CA_CERT_TLV = 0,
    ESP_SECURE_CERT_DEV_CERT_TLV,
    ESP_SECURE_CERT_PRIV_KEY_TLV,
    ESP_SECURE_CERT_DS_DATA_TLV,
    ESP_SECURE_CERT_DS_CONTEXT_TLV,
    ESP_SECURE_CERT_HMAC_ECDSA_KEY_SALT,
    ESP_SECURE_CERT_TLV_SEC_CFG,
    // Any new tlv types should be added above this
    ESP_SECURE_CERT_TLV_END = 50,
    //Custom data types
    //that can be defined by the user
    ESP_SECURE_CERT_USER_DATA_1 = 51,
    ESP_SECURE_CERT_USER_DATA_2 = 52,
    ESP_SECURE_CERT_USER_DATA_3 = 53,
    ESP_SECURE_CERT_USER_DATA_4 = 54,
    ESP_SECURE_CERT_USER_DATA_5 = 54,
    ESP_SECURE_CERT_TLV_MAX = 254, /* Max TLV entry identifier (should not be assigned to a TLV entry) */
    ESP_SECURE_CERT_TLV_INVALID = 255, /* Invalid TLV type */
} esp_secure_cert_tlv_type_t;

typedef enum esp_secure_cert_tlv_subtype {
    ESP_SECURE_CERT_SUBTYPE_0 = 0,
    ESP_SECURE_CERT_SUBTYPE_1 = 1,
    ESP_SECURE_CERT_SUBTYPE_2 = 2,
    ESP_SECURE_CERT_SUBTYPE_3 = 3,
    ESP_SECURE_CERT_SUBTYPE_4 = 4,
    ESP_SECURE_CERT_SUBTYPE_5 = 5,
    ESP_SECURE_CERT_SUBTYPE_6 = 6,
    ESP_SECURE_CERT_SUBTYPE_7 = 7,
    ESP_SECURE_CERT_SUBTYPE_8 = 8,
    ESP_SECURE_CERT_SUBTYPE_9 = 9,
    ESP_SECURE_CERT_SUBTYPE_10 = 10,
    ESP_SECURE_CERT_SUBTYPE_11 = 11,
    ESP_SECURE_CERT_SUBTYPE_12 = 12,
    ESP_SECURE_CERT_SUBTYPE_13 = 13,
    ESP_SECURE_CERT_SUBTYPE_14 = 14,
    ESP_SECURE_CERT_SUBTYPE_15 = 15,
    ESP_SECURE_CERT_SUBTYPE_16 = 16,
    ESP_SECURE_CERT_SUBTYPE_MAX = 254, /* Max Subtype entry identifier (should not be assigned to a TLV entry) */
    ESP_SECURE_CERT_SUBTYPE_INVALID = 255, /* Invalid TLV subtype */
} esp_secure_cert_tlv_subtype_t;
