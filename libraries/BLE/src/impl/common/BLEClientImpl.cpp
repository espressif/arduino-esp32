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

/**
 * @file
 * @brief Shared (stack-agnostic) implementation-layer definitions for
 *        @c BLEClientImplCommon. Public @c BLEClient:: methods live in
 *        src/BLEClient.cpp; this file holds the shared Impl helpers that backend
 *        client code calls. API contract is documented on the declarations in
 *        impl/common/BLEClientImpl.h.
 */

#include "impl/common/BLEGuards.h"
#if BLE_ENABLED

#include "BLEClient.h"
#include "impl/BLEBackend.h"
#include "impl/common/BLEMutex.h"

void BLEClientImplCommon::dispatchConnect(const BLEConnInfo &connInfo) {
  decltype(onConnectCb) cb;
  {
    BLELockGuard lock(mtx);
    cb = onConnectCb;
  }
  BLEClient clientHandle(shared_from_this());
  if (cb) {
    cb(clientHandle, connInfo);
  }
}

void BLEClientImplCommon::dispatchDisconnect(const BLEConnInfo &connInfo, uint8_t reason) {
  decltype(onDisconnectCb) cb;
  {
    BLELockGuard lock(mtx);
    cb = onDisconnectCb;
  }
  BLEClient clientHandle(shared_from_this());
  if (cb) {
    cb(clientHandle, connInfo, reason);
  }
}

void BLEClientImplCommon::dispatchConnectFail(int reason) {
  decltype(onConnectFailCb) cb;
  {
    BLELockGuard lock(mtx);
    cb = onConnectFailCb;
  }
  BLEClient clientHandle(shared_from_this());
  if (cb) {
    cb(clientHandle, reason);
  }
}

void BLEClientImplCommon::dispatchMtuChanged(const BLEConnInfo &connInfo, uint16_t mtu) {
  decltype(onMtuChangedCb) cb;
  {
    BLELockGuard lock(mtx);
    cb = onMtuChangedCb;
  }
  BLEClient clientHandle(shared_from_this());
  if (cb) {
    cb(clientHandle, connInfo, mtu);
  }
}

void BLEClientImplCommon::dispatchIdentityResolved(const BLEConnInfo &connInfo) {
  decltype(onIdentityCb) cb;
  {
    BLELockGuard lock(mtx);
    cb = onIdentityCb;
  }
  BLEClient clientHandle(shared_from_this());
  if (cb) {
    cb(clientHandle, connInfo);
  }
}

#if BLE_GATT_CLIENT_SUPPORTED

BLEClient BLEClientImplCommon::makeHandle(BLEClient::Impl *impl) {
  return BLEClient(impl->shared_from_this());
}

#endif /* BLE_GATT_CLIENT_SUPPORTED */

#endif /* BLE_ENABLED */
