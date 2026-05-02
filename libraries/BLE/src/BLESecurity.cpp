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

#include "impl/BLEGuards.h"
#if BLE_ENABLED

#include "BLESecurity.h"
#include "impl/BLESecurityBackend.h"
#include "impl/BLEImplHelpers.h"
#include "impl/BLEMutex.h"

#include <esp_random.h>
#include "esp32-hal-log.h"

// --------------------------------------------------------------------------
// Constructors / handle validity
// --------------------------------------------------------------------------

/**
 * @brief Construct a BLESecurity handle with no backend.
 *
 * @note The handle is invalid until initialized by BLEClass. Use operator bool() to check.
 */
BLESecurity::BLESecurity() : _impl(nullptr) {}

/**
 * @brief Check whether this handle references a valid security backend.
 * @return true if the implementation pointer is non-null, false otherwise.
 */
BLESecurity::operator bool() const {
  return _impl != nullptr;
}

// --------------------------------------------------------------------------
// Passkey helpers (stack-agnostic)
// --------------------------------------------------------------------------

/**
 * @brief Generate a cryptographically random 6-digit passkey.
 * @return A random value in the range 0-999999.
 * @note Uses the hardware RNG via esp_random() for entropy.
 */
uint32_t BLESecurity::generateRandomPassKey() {
  return esp_random() % 1000000;
}

/**
 * @brief Enable or disable automatic passkey regeneration on each new connection.
 * @param enable If true, a new random passkey is generated before every pairing.
 */
void BLESecurity::regenPassKeyOnConnect(bool enable) {
  BLE_CHECK_IMPL();
  impl.regenOnConnect = enable;
}

// --------------------------------------------------------------------------
// Callback registration
// --------------------------------------------------------------------------

/**
 * @brief Register a handler invoked when the stack needs a passkey from the application.
 * @param handler The handler to invoke, or nullptr to clear.
 */
void BLESecurity::onPassKeyRequest(PassKeyRequestHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.passKeyRequestCb = handler;
}

/**
 * @brief Register a handler invoked when the stack needs the application to display a passkey.
 * @param handler The handler to invoke, or nullptr to clear.
 */
void BLESecurity::onPassKeyDisplay(PassKeyDisplayHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.passKeyDisplayCb = handler;
}

/**
 * @brief Register a handler for numeric comparison confirmation.
 * @param handler The handler to invoke, or nullptr to clear.
 */
void BLESecurity::onConfirmPassKey(ConfirmPassKeyHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.confirmPassKeyCb = handler;
}

/**
 * @brief Register a handler for incoming security requests from a peer.
 * @param handler The handler to invoke, or nullptr to clear.
 */
void BLESecurity::onSecurityRequest(SecurityRequestHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.securityRequestCb = handler;
}

/**
 * @brief Register a handler for runtime ATT authorization decisions.
 * @param handler The handler to invoke, or nullptr to clear.
 */
void BLESecurity::onAuthorization(AuthorizationHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.authorizationCb = handler;
}

/**
 * @brief Register a handler called when authentication completes.
 * @param handler The handler to invoke, or nullptr to clear.
 */
void BLESecurity::onAuthenticationComplete(AuthCompleteHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.authCompleteCb = handler;
}

/**
 * @brief Register a handler for bond store overflow events.
 * @param handler The handler to invoke, or nullptr to clear.
 */
void BLESecurity::onBondStoreOverflow(BondStoreOverflowHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.bondOverflowCb = handler;
}

/**
 * @brief Remove all registered security callbacks.
 * @note Clears passKeyRequest, passKeyDisplay, confirmPassKey, securityRequest,
 *       authorization, authComplete, and bondOverflow handlers.
 */
void BLESecurity::resetCallbacks() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.passKeyRequestCb = nullptr;
  impl.passKeyDisplayCb = nullptr;
  impl.confirmPassKeyCb = nullptr;
  impl.securityRequestCb = nullptr;
  impl.authorizationCb = nullptr;
  impl.authCompleteCb = nullptr;
  impl.bondOverflowCb = nullptr;
}

// --------------------------------------------------------------------------
// Key size / force-authentication
// --------------------------------------------------------------------------

/**
 * @brief Set the maximum encryption key size.
 * @param size Key size in bytes (7-16).
 */
void BLESecurity::setKeySize(uint8_t size) {
  BLE_CHECK_IMPL();
  impl.keySize = size;
}

/**
 * @brief Control strict checking of authentication on links that use encryption.
 * @param force If true, unauthenticated encrypted links are rejected.
 * @note When bonding or encryption is in flight at the same time as GATT
 *       procedures (service discovery, first read), the order of
 *       authentication-complete vs attribute-access events is
 *       stack-dependent. Use @ref waitForAuthenticationComplete or the
 *       @ref onAuthenticationComplete callback if you need a defined
 *       post-pairing sequence.
 */
void BLESecurity::setForceAuthentication(bool force) {
  BLE_CHECK_IMPL();
  impl.forceAuth = force;
}

/**
 * @brief Query whether strict authentication checking is enabled.
 * @return true if forced authentication is active, false otherwise or if the handle is invalid.
 * @note Null-safe: returns false when @c _impl is nullptr.
 */
bool BLESecurity::getForceAuthentication() const {
  return _impl ? _impl->forceAuth : false;
}

// --------------------------------------------------------------------------
// Authentication synchronization
// --------------------------------------------------------------------------

/**
 * @brief Block until authentication completes or the timeout elapses.
 * @param timeoutMs Maximum time to wait in milliseconds.
 * @return true if authentication completed successfully, false on timeout or auth failure.
 * @note Acquires the auth semaphore then blocks on it. The semaphore is signaled by
 *       notifyAuthComplete() with BTStatus::OK on success or BTStatus::AuthFailed on failure.
 */
bool BLESecurity::waitForAuthenticationComplete(uint32_t timeoutMs) {
  BLE_CHECK_IMPL(false);
  impl.authSync.take();
  BTStatus status = impl.authSync.wait(timeoutMs);
  return status == BTStatus::OK;
}

// --------------------------------------------------------------------------
// Stack event dispatch (shared between backends)
// --------------------------------------------------------------------------

/**
 * @brief Notify that authentication completed for a connection.
 * @param conn Connection info for the peer.
 * @param success true if pairing succeeded, false on failure.
 * @note Dispatches to the registered AuthCompleteHandler (if any), then signals the
 *       auth semaphore so waitForAuthenticationComplete() unblocks.
 */
void BLESecurity::notifyAuthComplete(const BLEConnInfo &conn, bool success) {
  BLE_CHECK_IMPL();
  if (impl.authCompleteCb) {
    impl.authCompleteCb(conn, success);
  }
  impl.authSync.give(success ? BTStatus::OK : BTStatus::AuthFailed);
}

/**
 * @brief Runtime authorization hook for ATT read/write on protected attributes.
 * @param conn Connection info for the requesting peer.
 * @param attrHandle The attribute handle being accessed.
 * @param isRead true for a read operation, false for a write.
 * @return true to allow access, false to reject with an ATT error.
 * @note Fail-closed when no AuthorizationHandler is installed: returns false.
 */
bool BLESecurity::notifyAuthorization(const BLEConnInfo &conn, uint16_t attrHandle, bool isRead) {
  BLE_CHECK_IMPL(false);
  if (!impl.authorizationCb) {
    // The developer declared ReadAuthorized/WriteAuthorized on this attribute
    // but never installed a handler. That is a misconfiguration; deny access
    // so the security declaration is honoured and the bug surfaces immediately
    // (the peer gets ATT_ERR_INSUFFICIENT_AUTHOR rather than silent pass-through).
    log_w(
      "BLESecurity: attribute 0x%04x requires authorization but no handler is installed — denying %s",
      attrHandle, isRead ? "read" : "write"
    );
    return false;
  }
  return impl.authorizationCb(conn, attrHandle, isRead);
}

/**
 * @brief Resolve a numeric comparison confirmation via the registered handler.
 * @param conn Connection info for the peer.
 * @param numcmp The numeric comparison value to confirm.
 * @return true if the user confirmed the match, false to reject.
 * @note When no ConfirmPassKeyHandler is registered, this function defaults
 *       to accepting the pairing.  This is equivalent to the "Just Works"
 *       association model: it provides no MitM protection because no
 *       out-of-band value comparison takes place.
 *       Per BT Core Spec v5.x, Vol 3, Part H, §2.3.5.6 (Numeric Comparison):
 *       "Both devices shall display the 6-digit value and the user shall
 *       confirm that the values are equal on both devices."
 *       Install a ConfirmPassKeyHandler via BLESecurity::onConfirmPassKey()
 *       to enforce proper user confirmation and achieve MITM-resistant pairing.
 */
bool BLESecurity::resolveNumericComparison(const BLEConnInfo &conn, uint32_t numcmp) {
  if (!_impl || !_impl->confirmPassKeyCb) {
    return true;
  }
  return _impl->confirmPassKeyCb(conn, numcmp);
}

/**
 * @brief Notify the application that the bond store has overflowed.
 * @param oldest Address of the oldest bond that would be evicted.
 * @return true if the handler was invoked, false if no handler is registered or handle is invalid.
 */
bool BLESecurity::notifyBondOverflow(const BTAddress &oldest) {
  if (!_impl || !_impl->bondOverflowCb) {
    return false;
  }
  _impl->bondOverflowCb(oldest);
  return true;
}

#endif /* BLE_ENABLED */
