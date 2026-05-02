/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "impl/BLEGuards.h"
#if BLE_BLUEDROID

#include "BLEUUID.h"
#include <esp_bt_defs.h>
#include <string.h>

/**
 * @brief Fill an ESP-IDF GATT UUID structure from a @ref BLEUUID.
 * @param uuid  Public UUID; 128-bit values use Bluetooth wire byte order in @p out.
 * @param out   @c esp_bt_uuid_t written for Bluedroid API calls; zeroed first.
 */
inline void uuidToEsp(const BLEUUID &uuid, esp_bt_uuid_t &out) {
  memset(&out, 0, sizeof(out));
  if (uuid.bitSize() == 16) {
    out.len = ESP_UUID_LEN_16;
    out.uuid.uuid16 = uuid.toUint16();
  } else if (uuid.bitSize() == 32) {
    out.len = ESP_UUID_LEN_32;
    out.uuid.uuid32 = uuid.toUint32();
  } else {
    out.len = ESP_UUID_LEN_128;
    const uint8_t *data = uuid.data();
    for (int i = 0; i < 16; i++) {
      out.uuid.uuid128[i] = data[15 - i];
    }
  }
}

/**
 * @brief Same as @ref uuidToEsp but return the structure by value.
 * @param uuid  Source @ref BLEUUID.
 * @return      Populated @c esp_bt_uuid_t.
 */
inline esp_bt_uuid_t uuidToEspVal(const BLEUUID &uuid) {
  esp_bt_uuid_t out;
  uuidToEsp(uuid, out);
  return out;
}

/**
 * @brief Convert a Bluedroid @c esp_bt_uuid_t to a public @ref BLEUUID.
 * @param espUuid  UUID as returned from GAP/GATT on Bluedroid.
 * @return         Normalized @ref BLEUUID in application byte order.
 */
inline BLEUUID espToUuid(const esp_bt_uuid_t &espUuid) {
  if (espUuid.len == ESP_UUID_LEN_16) {
    return BLEUUID(espUuid.uuid.uuid16);
  } else if (espUuid.len == ESP_UUID_LEN_32) {
    return BLEUUID(espUuid.uuid.uuid32);
  } else {
    return BLEUUID(espUuid.uuid.uuid128, 16, true);
  }
}

#endif /* BLE_BLUEDROID */
