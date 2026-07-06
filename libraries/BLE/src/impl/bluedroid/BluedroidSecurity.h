/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 * Copyright 2017 chegewara
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

#include "BLESecurity.h"
#include "impl/common/BLESecurityImpl.h"

#if BLE_BLUEDROID

#include <esp_gap_ble_api.h>

/**
 * @brief Bluedroid security implementation (@c BLESecurity::Impl).
 *
 * Defined in @c impl/bluedroid/, so everything here is Bluedroid-specific; it inherits the
 * stack-agnostic @c BLESecurityImplCommon (shared config + pairing hooks), giving one uniform
 * @c impl.member type. Layer is disclosed by file/type: members on @c BLESecurity::Impl are
 * Bluedroid, members on @c BLESecurityImplCommon are shared.
 *
 * Passkey Entry (both roles) uses the shared BLESecurityImplCommon helpers
 * (resolvePasskeyForInput / notifyPasskeyDisplay); the Bluedroid stack supplies the
 * displayed value, so there is no backend-specific display resolver here.
 */
struct BLESecurity::Impl : BLESecurityImplCommon {
  using KeyDist = BLESecurity::KeyDist;

  bool bonding = true;
  bool secureConnection = true;
  KeyDist initiatorKeys = KeyDist::EncKey | KeyDist::IdKey;
  KeyDist responderKeys = KeyDist::EncKey | KeyDist::IdKey;

  /// Bridges @c esp_ble_remove_bond_device (async) to the blocking public
  /// @c deleteBond / @c deleteAllBonds APIs. Signaled from
  /// @c ESP_GAP_BLE_REMOVE_BOND_DEV_COMPLETE_EVT.
  BLESync bondSync;

  /// @brief Bluedroid-specific: GAP security event dispatch (routed from BluedroidCore via
  /// @c bluedroidSecurityHandleGAP). Static because the stack calls it by function pointer.
  static void handleGAP(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

  /// @brief Bluedroid-specific: push the current config into the Bluedroid stack
  /// (@c esp_ble_gap_set_security_param).
  void applySecurityParams();
};

#endif /* BLE_BLUEDROID */
