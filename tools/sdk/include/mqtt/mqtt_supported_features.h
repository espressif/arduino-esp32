// Copyright 2015-2019 Espressif Systems (Shanghai) PTE LTD
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

#ifndef _MQTT_SUPPORTED_FEATURES_H_
#define _MQTT_SUPPORTED_FEATURES_H_

#if __has_include("esp_idf_version.h")
#include "esp_idf_version.h"
#endif

/**
 * @brief This header defines supported features of IDF which mqtt module
 *        could use depending on specific version of ESP-IDF.
 *        In case "esp_idf_version.h" were not found, all additional
 *        features would be disabled
 */

#ifdef ESP_IDF_VERSION

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(3, 3, 0)
// Features supported from 3.3
#define MQTT_SUPPORTED_FEATURE_EVENT_LOOP
#define MQTT_SUPPORTED_FEATURE_SKIP_CRT_CMN_NAME_CHECK
#endif

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
// Features supported in 4.0
#define MQTT_SUPPORTED_FEATURE_WS_SUBPROTOCOL
#define MQTT_SUPPORTED_FEATURE_TRANSPORT_ERR_REPORTING
#endif

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0)
// Features supported in 4.1
#define MQTT_SUPPORTED_FEATURE_PSK_AUTHENTICATION
#define MQTT_SUPPORTED_FEATURE_DER_CERTIFICATES
#define MQTT_SUPPORTED_FEATURE_ALPN
#define MQTT_SUPPORTED_FEATURE_CLIENT_KEY_PASSWORD
#endif

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 2, 0)
// Features supported in 4.2
#define MQTT_SUPPORTED_FEATURE_SECURE_ELEMENT
#endif

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 0)
// Features supported in 4.3
#define MQTT_SUPPORTED_FEATURE_DIGITAL_SIGNATURE
#endif

#endif /* ESP_IDF_VERSION */
#endif // _MQTT_SUPPORTED_FEATURES_H_
