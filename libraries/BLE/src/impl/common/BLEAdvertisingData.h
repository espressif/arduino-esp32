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
#if BLE_ENABLED

#include "BLEAdvertisementData.h"
#include "BLEUUID.h"
#include "WString.h"
#include <vector>

/**
 * @brief Backend-agnostic serialization of the high-level legacy advertising
 *        fields into raw AD byte payloads.
 *
 * The controller-facing raw byte layout is identical for every backend and for
 * both the native-legacy and the extended (legacy-PDU) advertising paths, so it
 * is assembled once here on top of @ref BLEAdvertisementData rather than being
 * duplicated inside each stack's field-struct code. Consumers feed the resulting
 * bytes to their controller API (NimBLE @c ble_gap_ext_adv_set_data /
 * Bluedroid @c esp_ble_gap_config_ext_adv_data_raw, etc.).
 */
struct BLEAdvDataBuilder {
  /**
   * @brief Build the primary advertisement AD payload.
   * @param name Device name (may be empty).
   * @param serviceUUIDs Service UUIDs to advertise (16/32/128-bit).
   * @param appearance GAP appearance value (0 = omit).
   * @param includeTxPower Whether to embed the TX power level AD field.
   * @param txPower TX power value in dBm (only used when @p includeTxPower).
   * @param nameInScanResponse When true, the name is carried in the scan response
   *        (per CSS Part A §1.2) and is therefore omitted here; when false, the
   *        name is placed in the primary AD.
   * @param minPreferred Minimum preferred connection interval in 1.25 ms units
   *        (0 = omit Slave Connection Interval Range AD unless @p maxPreferred is set).
   * @param maxPreferred Maximum preferred connection interval in 1.25 ms units
   *        (0 = omit unless @p minPreferred is set; a zero side copies the other).
   * @return Assembled primary AD payload (Flags + optional name/tx power/appearance/
   *         preferred interval + service UUIDs).
   */
  static BLEAdvertisementData buildLegacyAdvData(
    const String &name, const std::vector<BLEUUID> &serviceUUIDs, uint16_t appearance, bool includeTxPower, int8_t txPower, bool nameInScanResponse,
    uint16_t minPreferred = 0, uint16_t maxPreferred = 0
  );

  /**
   * @brief Build the scan-response AD payload (local name only).
   * @param name Device name to place in the scan response (empty = empty payload).
   * @return Assembled scan-response AD payload.
   */
  static BLEAdvertisementData buildLegacyScanRespData(const String &name);
};

#endif /* BLE_ENABLED */
