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
 * @file BLE.bluedroid.h
 * @brief Bluedroid backend state for BLEClass (BLEClass::Impl).
 */

#include "core/BLEGuards.h"
#if BLE_BLUEDROID

#include "BLEImpl.h"
#include "core/BLESync.h"

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
inline esp_ble_gap_phy_mask_t bluedroidPhyToPrefMask(BLEPhy phy) {
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
 * Inherits @ref BLEClassImplCommon for shared lifecycle state and hook slots;
 * as a nested member of @c BLEClass, its static callbacks reach
 * @c BLE._impl directly.
 */
struct BLEClass::Impl : BLEClassImplCommon {
  using GapHook = void (*)(esp_gap_ble_cb_event_t, esp_ble_gap_cb_param_t *);
#if BLE_GATT_SERVER_SUPPORTED
  using GattsHook = void (*)(esp_gatts_cb_event_t, esp_gatt_if_t, esp_ble_gatts_cb_param_t *);
#endif
#if BLE_GATT_CLIENT_SUPPORTED
  using GattcHook = void (*)(esp_gattc_cb_event_t, esp_gatt_if_t, esp_ble_gattc_cb_param_t *);
#endif

  /// True while this session holds a reference to the shared Bluedroid host.
  /// The first user starts the host, the last one tears it down.
  bool hostUser = false;
  bool releasedHostUser = false;
  bool releasedHostStopped = false;
  /// Controller LL privacy state (resolvable private addresses), toggled by
  /// @c BLEClass::setOwnAddressType for the *ID address types only.
  bool privacyEnabled = false;
  /// Random device address programmed for @c BTAddress::Type::Random
  /// (esp_bd_addr_t, MSB-first). Kept so the advertising backend can mirror it
  /// into the set-level random address the controller requires.
  uint8_t ownRandomAddr[6] = {};
  bool randomAddrSet = false;
  BLESync privacySync{"privacySync"};
  /// Bridges @c esp_ble_gap_set_rand_addr (async) to @c setOwnAddressType.
  /// Signaled from @c ESP_GAP_BLE_SET_STATIC_RAND_ADDR_EVT.
  BLESync randAddrSync{"randAddrSync"};
  /// Bridges @c esp_ble_gap_update_whitelist (async) to the blocking public
  /// @c whiteListAdd / @c whiteListRemove APIs. Signaled from
  /// @c ESP_GAP_BLE_UPDATE_WHITELIST_COMPLETE_EVT.
  BLESync whitelistSync{"whitelistSync"};
  BLEClass::RawEventHandler customGattcHandler = nullptr;
  BLEClass::RawEventHandler customGattsHandler = nullptr;
#if BLE_ADVERTISING_SUPPORTED
  GapHook advGapHook = nullptr;
#endif
#if BLE_SCANNING_SUPPORTED
  GapHook scanGapHook = nullptr;
#endif
#if BLE_AUDIO_SUPPORTED
  GapHook audioGapHook = nullptr;
#endif
#if BLE_GATT_SERVER_SUPPORTED
  GapHook serverGapHook = nullptr;
  GattsHook serverGattsHook = nullptr;
  LifecycleHook serverPreEndHook = nullptr;
#endif
#if BLE_GATT_CLIENT_SUPPORTED
  GapHook clientGapHook = nullptr;
  GattcHook clientGattcHook = nullptr;
#endif
#if BLE_SMP_SUPPORTED
  GapHook securityGapHook = nullptr;
#endif

  BTStatus backendBegin(const String &deviceName);
  void abandonStart();
  void backendAfterHooks();
  void backendFinishBegin();
  void backendPreAdvScanEnd();
  void backendPreSessionClose();
  void backendStopAndDeinitHost();
  void backendFinalizeEnd(bool releaseMemory);

  static void gapCallback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
#if BLE_GATT_SERVER_SUPPORTED
  static void gattsCallback(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
#endif
#if BLE_GATT_CLIENT_SUPPORTED
  static void gattcCallback(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
#endif
};

/**
 * @brief Resolve a public connection handle to its peer address (Bluedroid).
 *
 * Searches the live client and server connection tables. Accepts the handle
 * reported by `BLEClient::getHandle()` / `BLEConnInfo::getHandle()` and the
 * HCI ACL handle as a fallback.
 */
bool bluedroidFindPeerAddress(uint16_t connHandle, BTAddress &peerAddress);

/** @brief Find the public handle for a connected peer address (Bluedroid). */
bool bluedroidFindConnHandle(const BTAddress &peerAddress, uint16_t &connHandle);

/** @brief Collect every live handle for a connected peer address (Bluedroid). */
void bluedroidFindConnHandles(const BTAddress &peerAddress, std::vector<uint16_t> &handles);

/** @brief Collect one BLEConnInfo per role/link for a connected peer (Bluedroid). */
void bluedroidFindConnections(const BTAddress &peerAddress, std::vector<BLEConnInfo> &connections);

/** @brief Snapshot connection info for a connected peer address (Bluedroid). */
bool bluedroidFindConnInfo(const BTAddress &peerAddress, BLEConnInfo &info);

#endif /* BLE_BLUEDROID */
