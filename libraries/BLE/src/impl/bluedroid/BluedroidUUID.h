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

#include "impl/common/BLEGuards.h"
#if BLE_BLUEDROID

#include "BLEUUID.h"
#include <esp_bt_defs.h>

/**
 * @brief Fill an ESP-IDF GATT UUID structure from a public @ref BLEUUID.
 * @param uuid  Public UUID; 128-bit values use Bluetooth wire byte order in @p out.
 * @param out   @c esp_bt_uuid_t written for Bluedroid API calls; zeroed first.
 */
void bluedroidUuidFromPublic(const BLEUUID &uuid, esp_bt_uuid_t &out);

/**
 * @brief Convert a Bluedroid @c esp_bt_uuid_t to a public @ref BLEUUID.
 * @param espUuid  UUID as returned from GAP/GATT on Bluedroid.
 * @return         Normalized @ref BLEUUID in application byte order.
 */
BLEUUID bluedroidUuidToPublic(const esp_bt_uuid_t &espUuid);

#endif /* BLE_BLUEDROID */
