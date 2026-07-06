/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 * Copyright 2020-2025 Ryan Powell <ryan@nable-embedded.io> and
 * esp-nimble-cpp, NimBLE-Arduino contributors.
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

#if BLE_NIMBLE

#include <host/ble_hs.h>

/**
 * @brief NimBLE security implementation (@c BLESecurity::Impl).
 *
 * Defined in @c impl/nimble/, so everything here is NimBLE-specific; it inherits the
 * stack-agnostic @c BLESecurityImplCommon (shared config + pairing hooks), giving one uniform
 * @c impl.member type. Layer is disclosed by file/type: members on @c BLESecurity::Impl are
 * NimBLE, members on @c BLESecurityImplCommon are shared.
 */
struct BLESecurity::Impl : BLESecurityImplCommon {
  bool bonding = true;
  bool sc = true;
  // Distribute both the LTK (ENC) and the IRK (ID) so a bond supports identity
  // resolution / RPA.
  uint8_t initKeyDist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
  uint8_t respKeyDist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

  /// @brief NimBLE-specific: push the current SMP settings into the NimBLE host (@c ble_hs_cfg).
  void applyToHost() const;

  /// @brief NimBLE-specific: Passkey Entry display role -- NimBLE asks the app to *supply* the
  /// passkey to show, so this computes/regens the shared @c passKey and dispatches it. (The input
  /// role and the display-callback dispatch are shared; see @c BLESecurityImplCommon.)
  uint32_t resolvePasskeyForDisplay(const BLEConnInfo &conn);
};

#endif /* BLE_NIMBLE */
