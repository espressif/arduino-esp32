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

#include "core/BLEGuards.h"
#if BLE_ENABLED

#include "BLE.h"
#include "server/BLEServer.h"
#include "core/BLEImplHelpers.h"
#include "core/BLEBackend.h"
#include "core/BLEMutex.h"
#include "esp32-hal-log.h"

BLEServer::BLEServer() : _impl(nullptr) {}

BLEServer::operator bool() const {
  return _impl != nullptr;
}

// Callback setters and resetCallbacks store handlers under mtx, and no-op with a
// warning when GATT server support is compiled out.

void BLEServer::onConnect(ConnectHandler handler) {
#if BLE_GATT_SERVER_SUPPORTED
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onConnectCb = std::move(handler);
#else
  (void)handler;
  log_w("%s not supported (GATT server disabled)", __func__);
#endif
}

void BLEServer::onDisconnect(DisconnectHandler handler) {
#if BLE_GATT_SERVER_SUPPORTED
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onDisconnectCb = std::move(handler);
#else
  (void)handler;
  log_w("%s not supported (GATT server disabled)", __func__);
#endif
}

void BLEServer::onMtuChanged(MtuChangedHandler handler) {
#if BLE_GATT_SERVER_SUPPORTED
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onMtuChangedCb = std::move(handler);
#else
  (void)handler;
  log_w("%s not supported (GATT server disabled)", __func__);
#endif
}

void BLEServer::onConnParamsUpdate(ConnParamsHandler handler) {
#if BLE_GATT_SERVER_SUPPORTED
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onConnParamsCb = std::move(handler);
#else
  (void)handler;
  log_w("%s not supported (GATT server disabled)", __func__);
#endif
}

void BLEServer::onIdentity(IdentityHandler handler) {
#if BLE_GATT_SERVER_SUPPORTED
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onIdentityCb = std::move(handler);
#else
  (void)handler;
  log_w("%s not supported (GATT server disabled)", __func__);
#endif
}

void BLEServer::resetCallbacks() {
#if BLE_GATT_SERVER_SUPPORTED
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onConnectCb = nullptr;
  impl.onDisconnectCb = nullptr;
  impl.onMtuChangedCb = nullptr;
  impl.onConnParamsCb = nullptr;
  impl.onIdentityCb = nullptr;
#endif
}

// Shared Impl helpers (connection table + callback dispatch) live in the
// implementation layer: server/BLEServerImpl.cpp.

// --------------------------------------------------------------------------
// BLEServer public API (stack-agnostic)
// --------------------------------------------------------------------------

void BLEServer::advertiseOnDisconnect(bool enable) {
  BLE_CHECK_IMPL();
  log_d("Server: advertiseOnDisconnect=%d", enable);
  BLELockGuard lock(impl.mtx);
  impl.advertiseOnDisconnect = enable;
}

BLEService BLEServer::createService(const BLEUUID &uuid, uint32_t numHandles, uint8_t instId) {
#if BLE_GATT_SERVER_SUPPORTED
  BLE_CHECK_IMPL(BLEService());
  BLELockGuard lock(impl.mtx);
  if (!impl.active) {
    log_w("Server: createService called outside an initialized BLE session");
    return BLEService();
  }

  // Return the existing service if this UUID+instId was already created.
  for (auto &svc : impl.services) {
    if (svc->uuid == uuid && svc->instId == instId) {
      log_d("Server: createService %s - returning existing", uuid.toString().c_str());
      return BLEService(svc);
    }
  }

  log_d("Server: createService %s numHandles=%u instId=%u", uuid.toString().c_str(), numHandles, instId);
  auto svc = std::make_shared<BLEService::Impl>();
  svc->uuid = uuid;
  svc->numHandles = numHandles;
  svc->instId = instId;
  svc->server = &impl;
  svc->serverOwner = _impl;
  impl.services.push_back(svc);

  return BLEService(svc);
#else
  (void)uuid;
  (void)numHandles;
  (void)instId;
  return BLEService();
#endif
}

BLEService BLEServer::getService(const BLEUUID &uuid) const {
  BLE_CHECK_IMPL(BLEService());
  BLELockGuard lock(impl.mtx);
  // First match wins when several services share a UUID (different instId).
  for (auto &svc : impl.services) {
    if (svc->uuid == uuid) {
      return BLEService(svc);
    }
  }
  log_d("Server: getService %s - not found", uuid.toString().c_str());
  return BLEService();
}

std::vector<BLEService> BLEServer::getServices() const {
  std::vector<BLEService> result;
  BLE_CHECK_IMPL(result);
  BLELockGuard lock(impl.mtx);
  result.reserve(impl.services.size());
  for (auto &svc : impl.services) {
    result.push_back(BLEService(svc));
  }
  return result;
}

BTStatus BLEServer::removeService(const BLEService &service) {
#if BLE_GATT_SERVER_SUPPORTED
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (!service._impl) {
    return BTStatus::InvalidState;
  }
  {
    BLELockGuard lock(impl.mtx);
    if (!impl.active) {
      return BTStatus::InvalidState;
    }
    bool found = false;
    for (const auto &owner : impl.services) {
      if (owner.get() == service._impl.get()) {
        found = true;
        continue;
      }
      for (const auto &included : owner->includedServices) {
        if (included.get() == service._impl.get()) {
          log_w("Server: cannot remove a service while another registered service includes it");
          return BTStatus::Busy;
        }
      }
    }
    if (!found) {
      return BTStatus::InvalidState;
    }
    if (impl.serviceHasPendingSends(service._impl.get())) {
      log_w("Server: cannot remove a service with pending notifications or indications");
      return BTStatus::Busy;
    }
    service._impl->removing = true;
  }
  // Backend-specific: Bluedroid deletes the service; NimBLE rebuilds the GATT table.
  BTStatus status = bleServerRemoveService(&impl, service._impl);
  if (!status) {
    BLELockGuard lock(impl.mtx);
    if (service._impl->server == &impl) {
      service._impl->removing = false;
    }
  }
  return status;
#else
  (void)service;
  return BTStatus::NotSupported;
#endif
}

bool BLEServer::isStarted() const {
  BLE_CHECK_IMPL(false);
  return impl.started;
}

size_t BLEServer::getConnectedCount() const {
  BLE_CHECK_IMPL(0);
  BLELockGuard lock(impl.mtx);
  return impl.connections.size();
}

std::vector<BLEConnInfo> BLEServer::getConnections() const {
  std::vector<BLEConnInfo> result;
  BLE_CHECK_IMPL(result);
  BLELockGuard lock(impl.mtx);
  // Snapshot under the lock; later connects/disconnects are not reflected.
  for (const auto &entry : impl.connections) {
    result.push_back(entry.second);
  }
  return result;
}

BLEConnInfo BLEServer::getConnInfo(uint16_t connHandle) const {
  BLE_CHECK_IMPL(BLEConnInfo());
  BLELockGuard lock(impl.mtx);
  // Returns the latest snapshot for the handle -- MTU / conn-params / security
  // are refreshed from GAP/GATT events -- or an invalid BLEConnInfo when the
  // handle is gone.
  for (const auto &entry : impl.connections) {
    if (entry.first == connHandle) {
      return entry.second;
    }
  }
  return BLEConnInfo();
}

BLEAdvertising BLEServer::getAdvertising() const {
  return BLE.getAdvertising();
}

BTStatus BLEServer::startAdvertising() {
  return BLE.startAdvertising();
}

BTStatus BLEServer::stopAdvertising() {
  return BLE.stopAdvertising();
}

#endif /* BLE_ENABLED */
