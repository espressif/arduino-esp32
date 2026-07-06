/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 * Copyright 2017 Neil Kolban
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

/**
 * @file BluedroidCore.h
 * @brief Bluedroid backend state for BLEClass (BLEClass::Impl).
 */

#include "impl/common/BLEGuards.h"
#if BLE_BLUEDROID

#include "BLE.h"
#include "impl/common/BLESync.h"

#include <esp_gap_ble_api.h>
#if BLE_GATT_SERVER_SUPPORTED
#include <esp_gatts_api.h>
#endif
#if BLE_GATT_CLIENT_SUPPORTED
#include <esp_gattc_api.h>
#endif

/**
 * @brief Map a public @c BLEPhy preference to a Bluedroid preferred-PHY bit mask.
 *
 * Preferred-PHY APIs take masks (@c ESP_BLE_GAP_PHY_*_PREF_MASK), not the single-PHY
 * enum values used by read/update events. Coded is 3 as a @c BLEPhy but bit 2 (4) as a mask.
 */
inline esp_ble_gap_phy_mask_t blePhyToPrefMask(BLEPhy phy) {
  switch (phy) {
    case BLEPhy::PHY_2M:    return ESP_BLE_GAP_PHY_2M_PREF_MASK;
    case BLEPhy::PHY_Coded: return ESP_BLE_GAP_PHY_CODED_PREF_MASK;
    case BLEPhy::PHY_1M:
    default:                return ESP_BLE_GAP_PHY_1M_PREF_MASK;
  }
}

/**
 * @brief Bluedroid backend state for @ref BLEClass (@c BLEClass::Impl).
 *
 * BLEClass is entirely Bluedroid-specific (no cross-backend shared state), so this
 * has no common base. As a nested member of BLEClass, its static callbacks reach
 * BLE._impl directly.
 */
struct BLEClass::Impl {
  uint16_t localMTU = 23;
  uint8_t ownAddrType = BLE_ADDR_TYPE_PUBLIC;
  BLESync privacySync;
  /// Bridges @c esp_ble_gap_update_whitelist (async) to the blocking public
  /// @c whiteListAdd / @c whiteListRemove APIs. Signaled from
  /// @c ESP_GAP_BLE_UPDATE_WHITELIST_COMPLETE_EVT.
  BLESync whitelistSync;
  BLEClass::RawEventHandler customGapHandler = nullptr;
  BLEClass::RawEventHandler customGattcHandler = nullptr;
  BLEClass::RawEventHandler customGattsHandler = nullptr;

  static void gapCallback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
#if BLE_GATT_SERVER_SUPPORTED
  static void gattsCallback(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
#endif
#if BLE_GATT_CLIENT_SUPPORTED
  static void gattcCallback(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
#endif
};

#endif /* BLE_BLUEDROID */
