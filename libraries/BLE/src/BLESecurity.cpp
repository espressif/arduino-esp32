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

#include "impl/common/BLEGuards.h"
#if BLE_ENABLED

#include "BLESecurity.h"
#include "impl/BLEBackend.h"  // full BLESecurity::Impl (+ BLESecurityImplCommon)
#include "impl/common/BLEImplHelpers.h"
#include "impl/common/BLEMutex.h"

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
  impl.passKeyRequestCb = handler;
}

void BLESecurity::onPassKeyDisplay(PassKeyDisplayHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.passKeyDisplayCb = handler;
}

void BLESecurity::onConfirmPassKey(ConfirmPassKeyHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.confirmPassKeyCb = handler;
}

void BLESecurity::onSecurityRequest(SecurityRequestHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.securityRequestCb = handler;
}

void BLESecurity::onAuthorization(AuthorizationHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.authorizationCb = handler;
}

void BLESecurity::onAuthenticationComplete(AuthCompleteHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.authCompleteCb = handler;
}

void BLESecurity::onBondStoreOverflow(BondStoreOverflowHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.bondOverflowCb = handler;
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

// The shared pairing/event hooks on BLESecurityImplCommon live in the
// implementation layer: impl/common/BLESecurityImpl.cpp.

#endif /* BLE_ENABLED */
