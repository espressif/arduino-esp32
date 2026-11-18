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

#include "core/BLEGuards.h"
#if BLE_ENABLED

#include "security/BLESecurity.h"
#include "core/BLEBackend.h"  // full BLESecurity::Impl (+ BLESecurityImplCommon)
#include "core/BLEImplHelpers.h"
#include "core/BLEMutex.h"

#include <esp_random.h>
#include "esp32-hal-log.h"

// --------------------------------------------------------------------------
// Constructors / handle validity
// --------------------------------------------------------------------------

BLESecurity::BLESecurity() : _impl(nullptr) {}

BLESecurity::operator bool() const {
  return _impl != nullptr;
}

// --------------------------------------------------------------------------
// Passkey helpers (stack-agnostic)
// --------------------------------------------------------------------------

uint32_t BLESecurity::generateRandomPassKey() {
  // Hardware RNG via esp_random() for entropy.
  return esp_random() % 1000000;
}

void BLESecurity::regenPassKeyOnConnect(bool enable) {
  BLE_CHECK_IMPL();
  // regenOnConnect is read on the host task in resolvePasskeyForDisplay (under
  // mtx), so the writer takes the same lock.
  BLELockGuard lock(impl.mtx);
  impl.regenOnConnect = enable;
}

// --------------------------------------------------------------------------
// Callback registration
// --------------------------------------------------------------------------

void BLESecurity::onPassKeyRequest(PassKeyRequestHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.passKeyRequestCb = std::move(handler);
}

void BLESecurity::onPassKeyDisplay(PassKeyDisplayHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.passKeyDisplayCb = std::move(handler);
}

void BLESecurity::onConfirmPassKey(ConfirmPassKeyHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.confirmPassKeyCb = std::move(handler);
}

void BLESecurity::onSecurityRequest(SecurityRequestHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.securityRequestCb = std::move(handler);
}

void BLESecurity::onAuthorization(AuthorizationHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.authorizationCb = std::move(handler);
}

void BLESecurity::onAuthenticationComplete(AuthCompleteHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.authCompleteCb = std::move(handler);
}

void BLESecurity::onBondStoreOverflow(BondStoreOverflowHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.bondOverflowCb = std::move(handler);
}

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

void BLESecurity::setKeySize(uint8_t size) {
  BLE_CHECK_IMPL();
  impl.keySize = size;
}

void BLESecurity::setForceAuthentication(bool force) {
  BLE_CHECK_IMPL();
  impl.forceAuth = force;
}

bool BLESecurity::getForceAuthentication() const {
  return _impl ? _impl->forceAuth : false;
}

// --------------------------------------------------------------------------
// Authentication synchronization
// --------------------------------------------------------------------------

bool BLESecurity::waitForAuthenticationComplete(uint32_t timeoutMs) {
  BLE_CHECK_IMPL(false);
  // Acquire the auth semaphore then block; notifyAuthComplete() signals it.
  impl.authSync.take();
  BTStatus status = impl.authSync.wait(timeoutMs);
  return status == BTStatus::OK;
}

bool BLESecurity::isConfigured() const {
  if (!_impl) {
    return false;
  }
  BLELockGuard lock(_impl->mtx);
  return _impl->configuredByApp;
}

#if BLE_SMP_SUPPORTED

void BLESecurity::setIOCapability(BLEIOCapability cap) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.ioCap = cap;
  impl.configuredByApp = true;
  impl.applySecurityParams();
}

void BLESecurity::setAuthenticationMode(bool bonding, bool mitm, bool secureConnection) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.bonding = bonding;
  impl.mitm = mitm;
  impl.secureConnection = secureConnection;
  impl.configuredByApp = true;
  impl.applySecurityParams();
}

uint32_t BLESecurity::getPassKey() const {
  if (!_impl) {
    return 0;
  }
  BLELockGuard lock(_impl->mtx);
  return _impl->passKey;
}

void BLESecurity::setInitiatorKeys(KeyDist keys) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.initiatorKeys = keys;
  impl.applySecurityParams();
}

void BLESecurity::setResponderKeys(KeyDist keys) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.responderKeys = keys;
  impl.applySecurityParams();
}

void BLESecurity::resetSecurity() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.ioCap = BLEIOCapability::NoInputNoOutput;
  impl.bonding = true;
  impl.mitm = false;
  impl.secureConnection = true;
  impl.forceAuth = false;
  impl.applySecurityParams();
}

#endif /* BLE_SMP_SUPPORTED */

// ==========================================================================
// Feature-disabled stubs (!BLE_SMP_SUPPORTED)
// ==========================================================================

#if !BLE_SMP_SUPPORTED

void BLESecurity::setIOCapability(BLEIOCapability) {
  log_w("SMP not supported");
}

void BLESecurity::setAuthenticationMode(bool, bool, bool) {
  log_w("SMP not supported");
}

void BLESecurity::setPassKey(bool, uint32_t) {
  log_w("SMP not supported");
}

void BLESecurity::setStaticPassKey(uint32_t) {
  log_w("SMP not supported");
}

void BLESecurity::setRandomPassKey() {
  log_w("SMP not supported");
}

uint32_t BLESecurity::getPassKey() const {
  return 0;
}

void BLESecurity::setInitiatorKeys(KeyDist) {
  log_w("SMP not supported");
}

void BLESecurity::setResponderKeys(KeyDist) {
  log_w("SMP not supported");
}

std::vector<BTAddress> BLESecurity::getBondedDevices() const {
  log_w("SMP not supported");
  return {};
}

BTStatus BLESecurity::deleteBond(const BTAddress &) {
  log_w("SMP not supported");
  return BTStatus::NotSupported;
}

BTStatus BLESecurity::deleteAllBonds() {
  log_w("SMP not supported");
  return BTStatus::NotSupported;
}

BTStatus BLESecurity::startSecurity(uint16_t) {
  log_w("SMP not supported");
  return BTStatus::NotSupported;
}

BTStatus BLESecurity::startSecurity(const BLEConnInfo &) {
  log_w("SMP not supported");
  return BTStatus::NotSupported;
}

void BLESecurity::resetSecurity() {
  log_w("SMP not supported");
}

#endif /* !BLE_SMP_SUPPORTED */

// The shared pairing/event hooks on BLESecurityImplCommon live in the
// implementation layer: security/BLESecurityImpl.cpp.

#endif /* BLE_ENABLED */
