/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 * Copyright 2020-2025 Ryan Powell <ryan@nable-embedded.io> and
 * esp-nimble-cpp, NimBLE-Arduino contributors.
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

/**
 * @file
 * @brief Shared (stack-agnostic) implementation-layer definitions for
 *        @c BLESecurityImplCommon. Public @c BLESecurity:: methods live in
 *        src/BLESecurity.cpp; this file holds the pairing/event hooks that
 *        backend event handlers invoke on the singleton
 *        (@c BLESecurity::Impl::instance()). @c this is the implementation, so
 *        state is accessed by member name. API contract is documented on the
 *        declarations in security/BLESecurityImpl.h.
 */

#include "core/BLEGuards.h"
#if BLE_ENABLED

#include "security/BLESecurity.h"
#include "core/BLEBackend.h"  // full BLESecurity::Impl (+ BLESecurityImplCommon)
#include "core/BLEMutex.h"
#include "esp32-hal-log.h"

#include <atomic>

// These hooks run on the host/BTC task while the app task may reassign the
// callbacks via BLESecurity setters (which lock mtx). Snapshot the target
// callback under mtx, then invoke the copy with the lock released so a user
// callback never runs while holding the config mutex.

BLESecurity::Impl *BLESecurityImplCommon::s_instance = nullptr;

void BLESecurityImplCommon::notifyAuthComplete(const BLEConnInfo &conn, bool success) {
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_WARN
  if (!success) {
    // Bond keys live in NVS, which survives reflashing, so a leftover or
    // mismatched bond keeps rejecting the same peer long after the sketch
    // changed. The reason codes that surface in that case (DHKey check, confirm
    // value, encryption failure) describe the crypto step that failed and never
    // point at the bond store, which makes this cause easy to chase for a long
    // time. Say it once per boot: failures usually arrive in bursts of retries.
    static std::atomic<bool> hinted{false};
    if (!hinted.exchange(true)) {
      log_w(
        "Security: pairing failed. If it keeps failing with the same peer, suspect a stale bond: bonds persist in NVS across reflashing. Clear them with "
        "BLESecurity::deleteAllBonds(), or erase NVS entirely (esptool erase-flash). See the Troubleshooting section of the BLE API documentation."
      );
    }
  }
#endif

  BLESecurity::AuthCompleteHandler cb;
  {
    BLELockGuard lock(mtx);
    cb = authCompleteCb;
  }
  if (cb) {
    cb(conn, success);
  }
  authSync.give(success ? BTStatus::OK : BTStatus::AuthFailed);
}

bool BLESecurityImplCommon::notifyAuthorization(const BLEConnInfo &conn, uint16_t attrHandle, bool isRead) {
  BLESecurity::AuthorizationHandler cb;
  {
    BLELockGuard lock(mtx);
    cb = authorizationCb;
  }
  if (!cb) {
    // The developer declared ReadAuthorized/WriteAuthorized on this attribute
    // but never installed a handler. That is a misconfiguration; deny access
    // so the security declaration is honored and the bug surfaces immediately
    // (the peer gets ATT_ERR_INSUFFICIENT_AUTHOR rather than silent pass-through).
    log_w("BLESecurity: attribute 0x%04x requires authorization but no handler is installed — denying %s", attrHandle, isRead ? "read" : "write");
    return false;
  }
  return cb(conn, attrHandle, isRead);
}

bool BLESecurityImplCommon::resolveNumericComparison(const BLEConnInfo &conn, uint32_t numcmp) {
  BLESecurity::ConfirmPassKeyHandler cb;
  {
    BLELockGuard lock(mtx);
    cb = confirmPassKeyCb;
  }
  // No handler => "Just Works" accept (no MitM protection); see header note.
  if (!cb) {
    return true;
  }
  return cb(conn, numcmp);
}

bool BLESecurityImplCommon::notifyBondOverflow(const BTAddress &oldest) {
  BLESecurity::BondStoreOverflowHandler cb;
  {
    BLELockGuard lock(mtx);
    cb = bondOverflowCb;
  }
  if (!cb) {
    return false;
  }
  cb(oldest);
  return true;
}

uint32_t BLESecurityImplCommon::resolvePasskeyForInput(const BLEConnInfo &conn) {
  BLESecurity::PassKeyRequestHandler cb;
  uint32_t staticKey;
  {
    BLELockGuard lock(mtx);
    cb = passKeyRequestCb;
    staticKey = passKey;
  }
  if (cb) {
    return cb(conn);
  }
  return staticKey;
}

void BLESecurityImplCommon::notifyPasskeyDisplay(const BLEConnInfo &conn, uint32_t passkey) {
  BLESecurity::PassKeyDisplayHandler cb;
  {
    BLELockGuard lock(mtx);
    cb = passKeyDisplayCb;
  }
  if (cb) {
    cb(conn, passkey);
  }
}

#endif /* BLE_ENABLED */
