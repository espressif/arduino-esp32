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
 * @file BLEServer.cpp
 * @brief Stack-agnostic `BLEServer` implementation: handle lifecycle, service
 *        table management, connection queries, advertising shorthands, and
 *        feature-disabled stubs.
 *
 * Shared `BLEServer::Impl` helpers (connection table + callback dispatch) live
 * in `server/BLEServerImpl.cpp`; stack operations live in
 * `BLEServer.{nimble,bluedroid}.cpp`.
 */

#include "core/BLEGuards.h"
#if BLE_ENABLED

#include "BLE.h"
#include "server/BLEServer.h"
#include "core/BLEImplHelpers.h"
#include "core/BLEBackend.h"
#include "core/BLEMutex.h"
#include "esp32-hal-log.h"

// ==========================================================================
// 1. Handle construction, connection queries & advertising shorthands
// ==========================================================================

BLEServer::BLEServer() : _impl(nullptr) {}

BLEServer::operator bool() const {
  return _impl != nullptr;
}

void BLEServer::advertiseOnDisconnect(bool enable) {
  BLE_CHECK_IMPL();
  log_d("Server: advertiseOnDisconnect=%d", enable);
  BLELockGuard lock(impl.mtx);
  impl.advertiseOnDisconnect = enable;
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

// ==========================================================================
// 2. Callbacks & GATT service lifecycle (BLE_GATT_SERVER_SUPPORTED)
// ==========================================================================

#if BLE_GATT_SERVER_SUPPORTED

void BLEServer::onConnect(ConnectHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onConnectCb = std::move(handler);
}

void BLEServer::onDisconnect(DisconnectHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onDisconnectCb = std::move(handler);
}

void BLEServer::onMtuChanged(MtuChangedHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onMtuChangedCb = std::move(handler);
}

void BLEServer::onConnParamsUpdate(ConnParamsHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onConnParamsCb = std::move(handler);
}

void BLEServer::onIdentity(IdentityHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onIdentityCb = std::move(handler);
}

void BLEServer::resetCallbacks() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onConnectCb = nullptr;
  impl.onDisconnectCb = nullptr;
  impl.onMtuChangedCb = nullptr;
  impl.onConnParamsCb = nullptr;
  impl.onIdentityCb = nullptr;
}

BLEService BLEServer::createService(const BLEUUID &uuid, uint32_t numHandles, uint8_t instId) {
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
}

BTStatus BLEServer::removeService(const BLEService &service) {
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
  // Backend-specific: Bluedroid deletes the service; NimBLE rebuilds or hides the GATT range.
  BTStatus status = BLEServerInternal::removeService(&impl, service._impl);
  if (!status) {
    BLELockGuard lock(impl.mtx);
    if (service._impl->server == &impl) {
      service._impl->removing = false;
    }
  }
  return status;
}

#endif /* BLE_GATT_SERVER_SUPPORTED */

// ==========================================================================
// 3. Feature-disabled & capability stubs
// ==========================================================================

#if BLE_GATT_SERVER_SUPPORTED && !BLE5_SUPPORTED

BTStatus BLEServer::setPhy(uint16_t, BLEPhy, BLEPhy) {
  log_w("Server: setPhy not supported (BLE 5.0 unavailable)");
  return BTStatus::NotSupported;
}

BTStatus BLEServer::getPhy(uint16_t, BLEPhy &txPhy, BLEPhy &rxPhy) const {
  txPhy = BLEPhy::PHY_1M;
  rxPhy = BLEPhy::PHY_1M;
  log_w("Server: getPhy not supported (BLE 5.0 unavailable)");
  return BTStatus::NotSupported;
}

#endif /* BLE_GATT_SERVER_SUPPORTED && !BLE5_SUPPORTED */

#if !BLE_GATT_SERVER_SUPPORTED

void BLEServer::onConnect(ConnectHandler) {
  log_w("%s not supported (GATT server disabled)", __func__);
}

void BLEServer::onDisconnect(DisconnectHandler) {
  log_w("%s not supported (GATT server disabled)", __func__);
}

void BLEServer::onMtuChanged(MtuChangedHandler) {
  log_w("%s not supported (GATT server disabled)", __func__);
}

void BLEServer::onConnParamsUpdate(ConnParamsHandler) {
  log_w("%s not supported (GATT server disabled)", __func__);
}

void BLEServer::onIdentity(IdentityHandler) {
  log_w("%s not supported (GATT server disabled)", __func__);
}

void BLEServer::resetCallbacks() {}

BLEService BLEServer::createService(const BLEUUID &, uint32_t, uint8_t) {
  return BLEService();
}

BTStatus BLEServer::removeService(const BLEService &) {
  return BTStatus::NotSupported;
}

BTStatus BLEServer::start() {
  log_w("GATT server not supported");
  return BTStatus::NotSupported;
}

BTStatus BLEServer::disconnect(uint16_t, uint8_t) {
  log_w("GATT server not supported");
  return BTStatus::NotSupported;
}

uint16_t BLEServer::getPeerMTU(uint16_t) const {
  return 0;
}

BTStatus BLEServer::updateConnParams(uint16_t, const BLEConnParams &) {
  log_w("GATT server not supported");
  return BTStatus::NotSupported;
}

BTStatus BLEServer::setPhy(uint16_t, BLEPhy, BLEPhy) {
  log_w("GATT server not supported");
  return BTStatus::NotSupported;
}

BTStatus BLEServer::getPhy(uint16_t, BLEPhy &, BLEPhy &) const {
  log_w("GATT server not supported");
  return BTStatus::NotSupported;
}

BTStatus BLEServer::setDataLen(uint16_t, uint16_t, uint16_t) {
  log_w("GATT server not supported");
  return BTStatus::NotSupported;
}

int BLEServer::handleGapEvent(void *) {
  return 0;
}

#endif /* !BLE_GATT_SERVER_SUPPORTED */

#endif /* BLE_ENABLED */
