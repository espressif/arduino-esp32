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

#include "security/BLESecurity.h"
#include "security/BLESecurityImpl.h"

#if BLE_NIMBLE

#include <host/ble_hs.h>

/**
 * @brief NimBLE security implementation (@c BLESecurity::Impl).
 *
 * Defined in a @c .nimble.* file, so everything here is NimBLE-specific; it inherits the
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
  void applySecurityParams() const;

  /// @brief NimBLE-specific: Passkey Entry display role -- NimBLE asks the app to *supply* the
  /// passkey to show, so this computes/regens the shared @c passKey and dispatches it. (The input
  /// role and the display-callback dispatch are shared; see @c BLESecurityImplCommon.)
  uint32_t resolvePasskeyForDisplay(const BLEConnInfo &conn);

  /// @brief NimBLE-specific: report the outcome of @c BLE_GAP_EVENT_PARING_COMPLETE.
  /// @param connHandle Connection the pairing ran on.
  /// @param smReason SMP Pairing Failed reason code, 0 on success.
  /// @note Only this event carries the SMP reason code. @c BLE_GAP_EVENT_ENC_CHANGE reports a
  ///       BLE_HS_* host status instead, which says that encryption failed but not why the peer
  ///       refused it, leaving a rejected passkey indistinguishable from a pairing timeout.
  ///       Static because both the client and the server GAP handlers see the event.
  static void reportPairingComplete(uint16_t connHandle, int smReason);

  /// @brief NimBLE-specific: drop the local half of a bond the peer no longer holds.
  /// @param peerIdAddr Identity address from the connection descriptor.
  /// @note Called when encryption fails with @c BLE_ERR_PINKEY_MISSING. The peer cannot produce
  ///       the LTK, so every attempt on this link fails the same way until the stale bond goes.
  static void deleteStaleBond(const ble_addr_t &peerIdAddr);
};

#endif /* BLE_NIMBLE */
