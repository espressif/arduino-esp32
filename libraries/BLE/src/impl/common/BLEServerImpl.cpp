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
 *        @c BLEServerImplCommon. Public @c BLEServer:: methods live in
 *        src/BLEServer.cpp; this file holds the shared Impl helpers (connection
 *        table + callback dispatch) that backend event handlers call.
 *        API contract is documented on the declarations in
 *        impl/common/BLEServerImpl.h.
 */

#include "impl/common/BLEGuards.h"
#if BLE_ENABLED

#include "BLEServer.h"
#include "impl/BLEBackend.h"
#include "impl/common/BLEMutex.h"
#include "esp32-hal-log.h"

#if BLE_GATT_SERVER_SUPPORTED

// --------------------------------------------------------------------------
// Connection helpers (shared between backends)
// --------------------------------------------------------------------------

void BLEServerImplCommon::connSet(uint16_t connHandle, const BLEConnInfo &connInfo) {
  for (auto &entry : connections) {
    if (entry.first == connHandle) {
      entry.second = connInfo;
      return;
    }
  }
  connections.emplace_back(connHandle, connInfo);
}

void BLEServerImplCommon::connErase(uint16_t connHandle) {
  for (auto it = connections.begin(); it != connections.end(); ++it) {
    if (it->first == connHandle) {
      connections.erase(it);
      return;
    }
  }
}

BLEConnInfo *BLEServerImplCommon::connFind(uint16_t connHandle) {
  for (auto &entry : connections) {
    if (entry.first == connHandle) {
      return &entry.second;
    }
  }
  return nullptr;
}

BLEServer BLEServerImplCommon::makeHandle(BLEServer::Impl *impl) {
  return BLEServer(impl->shared_from_this());
}

// --------------------------------------------------------------------------
// Callback dispatch (shared between backends)
//
// Each dispatcher copies the handler under mtx and invokes it outside the lock
// so user code can safely re-enter the BLE API without deadlocking.
// --------------------------------------------------------------------------

void BLEServerImplCommon::dispatchConnect(const BLEConnInfo &connInfo) {
  log_i("Server: client connected, handle=%u addr=%s", connInfo.getHandle(), connInfo.getAddress().toString().c_str());
  decltype(onConnectCb) cb;
  {
    BLELockGuard lock(mtx);
    cb = onConnectCb;
  }
  BLEServer serverHandle(shared_from_this());
  if (cb) {
    cb(serverHandle, connInfo);
  }
}

void BLEServerImplCommon::dispatchDisconnect(const BLEConnInfo &connInfo, uint8_t reason) {
  log_i("Server: client disconnected, handle=%u addr=%s reason=0x%02x", connInfo.getHandle(), connInfo.getAddress().toString().c_str(), reason);
  decltype(onDisconnectCb) cb;
  {
    BLELockGuard lock(mtx);
    cb = onDisconnectCb;
  }
  BLEServer serverHandle(shared_from_this());
  if (cb) {
    cb(serverHandle, connInfo, reason);
  }
}

void BLEServerImplCommon::dispatchMtuChanged(const BLEConnInfo &connInfo, uint16_t mtu) {
  log_d("Server: MTU changed, handle=%u mtu=%u", connInfo.getHandle(), mtu);
  decltype(onMtuChangedCb) cb;
  {
    BLELockGuard lock(mtx);
    cb = onMtuChangedCb;
  }
  BLEServer serverHandle(shared_from_this());
  if (cb) {
    cb(serverHandle, connInfo, mtu);
  }
}

void BLEServerImplCommon::dispatchConnParamsUpdate(const BLEConnInfo &connInfo) {
  log_d(
    "Server: conn params updated, handle=%u interval=%u latency=%u timeout=%u", connInfo.getHandle(), connInfo.getInterval(), connInfo.getLatency(),
    connInfo.getSupervisionTimeout()
  );
  decltype(onConnParamsCb) cb;
  {
    BLELockGuard lock(mtx);
    cb = onConnParamsCb;
  }
  BLEServer serverHandle(shared_from_this());
  if (cb) {
    cb(serverHandle, connInfo);
  }
}

void BLEServerImplCommon::dispatchIdentityResolved(const BLEConnInfo &connInfo) {
  decltype(onIdentityCb) cb;
  {
    BLELockGuard lock(mtx);
    cb = onIdentityCb;
  }
  BLEServer serverHandle(shared_from_this());
  if (cb) {
    cb(serverHandle, connInfo);
  }
}

#endif /* BLE_GATT_SERVER_SUPPORTED */

#endif /* BLE_ENABLED */
