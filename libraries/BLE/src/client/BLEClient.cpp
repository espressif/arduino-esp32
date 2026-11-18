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
 * @file BLEClient.cpp
 * @brief Stack-agnostic `BLEClient` implementation: handle lifecycle, callback
 *        registration, GATT convenience shorthands, and feature-disabled stubs.
 *
 * Shared `BLEClient::Impl` helpers (`dispatchIdentityResolved`, `makeHandle`)
 * live in `client/BLEClientImpl.cpp`; stack operations live in
 * `BLEClient.{nimble,bluedroid}.cpp`.
 */

#include "core/BLEGuards.h"
#if BLE_ENABLED

#include "client/BLEClient.h"
#include "remote/BLERemoteService.h"
#include "core/BLEImplHelpers.h"
#include "core/BLEBackend.h"
#include "core/BLEMutex.h"
#include "esp32-hal-log.h"

// ==========================================================================
// 1. Handle construction & connection state queries
// ==========================================================================

BLEClient::BLEClient() : _impl(nullptr) {}

BLEClient::operator bool() const {
  return _impl != nullptr;
}

BTAddress BLEClient::getPeerAddress() const {
  BLE_CHECK_IMPL(BTAddress());
  return impl.peerAddress;
}

bool BLEClient::isConnected() const {
  BLE_CHECK_IMPL(false);
  return impl.connected;
}

String BLEClient::toString() const {
  BLE_CHECK_IMPL("BLEClient(null)");
  String s = "BLEClient(peer=";
  s += impl.peerAddress.toString();
  s += impl.connected ? ", connected)" : ", disconnected)";
  return s;
}

// ==========================================================================
// 2. Service lookup & value shorthands
// ==========================================================================

String BLEClient::getValue(const BLEUUID &serviceUUID, const BLEUUID &charUUID) {
  log_d("Client: getValue svc=%s chr=%s", serviceUUID.toString().c_str(), charUUID.toString().c_str());
  BLERemoteService svc = getService(serviceUUID);
  if (!svc) {
    log_w("Client: getValue - service %s not found", serviceUUID.toString().c_str());
    return "";
  }
  return svc.getValue(charUUID);
}

BTStatus BLEClient::setValue(const BLEUUID &serviceUUID, const BLEUUID &charUUID, const String &value) {
  log_d("Client: setValue svc=%s chr=%s len=%u", serviceUUID.toString().c_str(), charUUID.toString().c_str(), (unsigned)value.length());
  BLERemoteService svc = getService(serviceUUID);
  if (!svc) {
    log_w("Client: setValue - service %s not found", serviceUUID.toString().c_str());
    return BTStatus::NotFound;
  }
  return svc.setValue(charUUID, value);
}

// ==========================================================================
// 3. GATT client callbacks & service discovery (BLE_GATT_CLIENT_SUPPORTED)
// ==========================================================================

#if BLE_GATT_CLIENT_SUPPORTED

void BLEClient::onConnect(ConnectHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onConnectCb = std::move(handler);
}

void BLEClient::onDisconnect(DisconnectHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onDisconnectCb = std::move(handler);
}

void BLEClient::onConnectFail(ConnectFailHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onConnectFailCb = std::move(handler);
}

void BLEClient::onMtuChanged(MtuChangedHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onMtuChangedCb = std::move(handler);
}

void BLEClient::onConnParamsUpdateRequest(ConnParamsReqHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onConnParamsReqCb = std::move(handler);
}

void BLEClient::onIdentity(IdentityHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onIdentityCb = std::move(handler);
}

void BLEClient::resetCallbacks() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onConnectCb = nullptr;
  impl.onDisconnectCb = nullptr;
  impl.onConnectFailCb = nullptr;
  impl.onMtuChangedCb = nullptr;
  impl.onConnParamsReqCb = nullptr;
  impl.onIdentityCb = nullptr;
}

BLERemoteService BLEClient::getService(const BLEUUID &uuid) const {
  BLE_CHECK_IMPL(BLERemoteService());
  bool needDiscover;
  {
    BLELockGuard lock(impl.mtx);
    if (!impl.connected) {
      return BLERemoteService();
    }
    needDiscover = impl.discoveredServices.empty();
  }
  if (needDiscover && !const_cast<BLEClient *>(this)->discoverServices()) {
    return BLERemoteService();
  }

  BLELockGuard lock(impl.mtx);
  for (auto &service : impl.discoveredServices) {
    if (service->uuid == uuid) {
      return BLERemoteService(service);
    }
  }
  log_w("getService: UUID %s not found among %u discovered service(s)", uuid.toString().c_str(), (unsigned)impl.discoveredServices.size());
  return BLERemoteService();
}

std::vector<BLERemoteService> BLEClient::getServices() const {
  std::vector<BLERemoteService> result;
  BLE_CHECK_IMPL(result);
  bool needDiscover;
  {
    BLELockGuard lock(impl.mtx);
    needDiscover = impl.connected && impl.discoveredServices.empty();
  }
  if (needDiscover) {
    auto *self = const_cast<BLEClient *>(this);
    (void)self->discoverServices();
  }

  BLELockGuard lock(impl.mtx);
  result.reserve(impl.discoveredServices.size());
  for (auto &service : impl.discoveredServices) {
    result.push_back(BLERemoteService(service));
  }
  return result;
}

#endif /* BLE_GATT_CLIENT_SUPPORTED */

// ==========================================================================
// 4. Feature-disabled & capability stubs
// ==========================================================================

#if BLE_GATT_CLIENT_SUPPORTED && !BLE5_SUPPORTED

BTStatus BLEClient::setPhy(BLEPhy, BLEPhy) {
  log_w("Client: setPhy not supported (BLE 5.0 unavailable)");
  return BTStatus::NotSupported;
}

BTStatus BLEClient::getPhy(BLEPhy &txPhy, BLEPhy &rxPhy) const {
  txPhy = BLEPhy::PHY_1M;
  rxPhy = BLEPhy::PHY_1M;
  log_w("Client: getPhy not supported (BLE 5.0 unavailable)");
  return BTStatus::NotSupported;
}

#endif /* BLE_GATT_CLIENT_SUPPORTED && !BLE5_SUPPORTED */

#if !BLE_GATT_CLIENT_SUPPORTED

void BLEClient::onConnect(ConnectHandler) {
  log_w("%s not supported (GATT client disabled)", __func__);
}

void BLEClient::onDisconnect(DisconnectHandler) {
  log_w("%s not supported (GATT client disabled)", __func__);
}

void BLEClient::onConnectFail(ConnectFailHandler) {
  log_w("%s not supported (GATT client disabled)", __func__);
}

void BLEClient::onMtuChanged(MtuChangedHandler) {
  log_w("%s not supported (GATT client disabled)", __func__);
}

void BLEClient::onConnParamsUpdateRequest(ConnParamsReqHandler) {
  log_w("%s not supported (GATT client disabled)", __func__);
}

void BLEClient::onIdentity(IdentityHandler) {
  log_w("%s not supported (GATT client disabled)", __func__);
}

void BLEClient::resetCallbacks() {}

BLERemoteService BLEClient::getService(const BLEUUID &) const {
  log_w("GATT client not supported");
  return BLERemoteService();
}

std::vector<BLERemoteService> BLEClient::getServices() const {
  log_w("GATT client not supported");
  return {};
}

BTStatus BLEClient::connect(const BTAddress &, uint32_t) {
  log_w("GATT client not supported");
  return BTStatus::NotSupported;
}

BTStatus BLEClient::connect(const BLEAdvertisedDevice &, uint32_t) {
  log_w("GATT client not supported");
  return BTStatus::NotSupported;
}

BTStatus BLEClient::connect(const BTAddress &, BLEPhy, uint32_t) {
  log_w("GATT client not supported");
  return BTStatus::NotSupported;
}

BTStatus BLEClient::connect(const BLEAdvertisedDevice &, BLEPhy, uint32_t) {
  log_w("GATT client not supported");
  return BTStatus::NotSupported;
}

BTStatus BLEClient::connectAsync(const BTAddress &, BLEPhy) {
  log_w("GATT client not supported");
  return BTStatus::NotSupported;
}

BTStatus BLEClient::connectAsync(const BLEAdvertisedDevice &, BLEPhy) {
  log_w("GATT client not supported");
  return BTStatus::NotSupported;
}

BTStatus BLEClient::cancelConnect() {
  log_w("GATT client not supported");
  return BTStatus::NotSupported;
}

BTStatus BLEClient::disconnect() {
  log_w("GATT client not supported");
  return BTStatus::NotSupported;
}

BTStatus BLEClient::secureConnection() {
  log_w("GATT client not supported");
  return BTStatus::NotSupported;
}

BTStatus BLEClient::discoverServices() {
  log_w("GATT client not supported");
  return BTStatus::NotSupported;
}

void BLEClient::setMTU(uint16_t) {
  log_w("GATT client not supported");
}

uint16_t BLEClient::getMTU() const {
  return 0;
}

int8_t BLEClient::getRSSI() const {
  return -128;
}

uint16_t BLEClient::getHandle() const {
  return 0xFFFF;
}

BLEConnInfo BLEClient::getConnInfo() const {
  return BLEConnInfo();
}

BTStatus BLEClient::updateConnParams(const BLEConnParams &) {
  log_w("GATT client not supported");
  return BTStatus::NotSupported;
}

BTStatus BLEClient::setPhy(BLEPhy, BLEPhy) {
  log_w("GATT client not supported");
  return BTStatus::NotSupported;
}

BTStatus BLEClient::getPhy(BLEPhy &, BLEPhy &) const {
  log_w("GATT client not supported");
  return BTStatus::NotSupported;
}

BTStatus BLEClient::setDataLen(uint16_t, uint16_t) {
  log_w("GATT client not supported");
  return BTStatus::NotSupported;
}

#endif /* !BLE_GATT_CLIENT_SUPPORTED */

#endif /* BLE_ENABLED */
