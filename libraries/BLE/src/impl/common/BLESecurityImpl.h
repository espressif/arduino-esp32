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

/**
 * @file
 * @brief Shared, stack-agnostic base for @c BLESecurity::Impl.
 *
 * Holds the state and behavior that are identical on every backend: the pairing
 * callbacks, the auth-completion sync object, the config mutex, and the internal
 * notification hooks the stack fires during pairing. The active backend defines
 * @c BLESecurity::Impl in @c impl/<stack>/ inheriting this base, so the whole impl is used
 * uniformly as @c impl.member. What layer a member belongs to is disclosed by its defining type:
 * @c BLESecurityImplCommon:: (common) vs @c BLESecurity::Impl:: (stack-specific, in impl/<stack>/).
 *
 * The hooks live here (not on the public @c BLESecurity) so backend event handlers
 * can invoke them directly on the process-wide singleton via @c instance() without
 * a @c friend bridge on the public class.
 */

#include "impl/common/BLEGuards.h"

#if BLE_ENABLED

#include "BLESecurity.h"
#include "impl/common/BLESync.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

struct BLESecurityImplCommon {
  // Single process-wide sync for waitForAuthenticationComplete(). Because
  // BLESecurity is a singleton, only ONE pairing can be awaited at a time: if
  // two links pair concurrently, notifyAuthComplete() for either wakes the one
  // waiter, so the app cannot distinguish which peer completed. This matches the
  // typical single-central/single-pairing usage; concurrent multi-link pairing
  // with per-link wait semantics is out of scope (would need a per-connection
  // sync map). Documented limitation, not a bug.
  BLESync authSync;
  // Guards the security config fields, which the user task writes (setIOCapability,
  // setAuthenticationMode, ...) while the host task reads them during pairing.
  SemaphoreHandle_t mtx = xSemaphoreCreateRecursiveMutex();

  BLESecurity::IOCapability ioCap = BLESecurity::NoInputNoOutput;
  bool mitm = false;
  bool forceAuth = false;
  bool regenOnConnect = false;
  uint8_t keySize = 16;

  // Passkey Entry configuration (shared; the name `staticPassKey`/`passKeySet` was
  // stack-inconsistent before). `passKey` is the 6-digit value; `passKeyIsStatic`
  // marks it as a fixed value the app pins (vs. left to per-connection generation).
  uint32_t passKey = 0;
  bool passKeyIsStatic = false;

  BLESecurity::PassKeyRequestHandler passKeyRequestCb = nullptr;
  BLESecurity::PassKeyDisplayHandler passKeyDisplayCb = nullptr;
  BLESecurity::ConfirmPassKeyHandler confirmPassKeyCb = nullptr;
  BLESecurity::SecurityRequestHandler securityRequestCb = nullptr;
  BLESecurity::AuthorizationHandler authorizationCb = nullptr;
  BLESecurity::AuthCompleteHandler authCompleteCb = nullptr;
  BLESecurity::BondStoreOverflowHandler bondOverflowCb = nullptr;

  // Process-wide singleton (all BLESecurity handles share one implementation).
  // Set by BLEClass::getSecurity() on first use; backend event handlers reach the
  // hooks through instance() instead of cracking open a public BLESecurity handle.
  static BLESecurity::Impl *s_instance;
  static BLESecurity::Impl *instance() {
    return s_instance;
  }

  // Stack event dispatch hooks (shared across backends); defined in BLESecurity.cpp.

  /// Dispatch the AuthCompleteHandler (if any), then signal authSync so
  /// waitForAuthenticationComplete() unblocks.
  void notifyAuthComplete(const BLEConnInfo &conn, bool success);

  /// Runtime ATT read/write authorization hook. Fail-closed: with no
  /// AuthorizationHandler installed this returns false, so a ReadAuthorized/
  /// WriteAuthorized declaration without a handler denies access.
  bool notifyAuthorization(const BLEConnInfo &conn, uint16_t attrHandle, bool isRead);

  /// Numeric-comparison confirmation. With no ConfirmPassKeyHandler installed this
  /// defaults to accepting ("Just Works", no MitM protection); per BT Core Spec v5.x
  /// Vol 3, Part H 2.3.5.6, install a handler via onConfirmPassKey() for MitM-resistant
  /// Numeric Comparison.
  bool resolveNumericComparison(const BLEConnInfo &conn, uint32_t numcmp);

  /// Notify the application that the bond store overflowed; returns true if a handler ran.
  bool notifyBondOverflow(const BTAddress &oldest);

  /// Passkey Entry, input role (this device supplies the passkey the peer displays):
  /// returns the PassKeyRequestHandler result, else the configured passKey. Identical
  /// on both stacks. (Display role stays backend-specific: NimBLE supplies the shown
  /// value, Bluedroid's stack generates it -- see BLESecurity::Impl::resolvePasskeyForDisplay
  /// in impl/nimble/.)
  uint32_t resolvePasskeyForInput(const BLEConnInfo &conn);

  /// Passkey Entry, display role callback dispatch: fire the PassKeyDisplayHandler with
  /// the value to show. The value's *source* differs per stack, but the dispatch is shared.
  void notifyPasskeyDisplay(const BLEConnInfo &conn, uint32_t passkey);

  ~BLESecurityImplCommon() {
    if (mtx) {
      vSemaphoreDelete(mtx);
    }
  }
};

#endif /* BLE_ENABLED */
