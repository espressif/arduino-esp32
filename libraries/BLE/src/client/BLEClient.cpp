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

#include "client/BLEClient.h"
#include "remote/BLERemoteService.h"
#include "core/BLEImplHelpers.h"
#include "core/BLEBackend.h"

#include "core/BLEMutex.h"
#include "esp32-hal-log.h"

/**
 * @file
 * @brief Common @ref BLEClient implementation: callback registration, shorthands,
 *        and glue to the active GATT client backend.
 */

BLEClient::BLEClient() : _impl(nullptr) {}

BLEClient::operator bool() const {
  return _impl != nullptr;
}

// Shared Impl helpers (dispatchIdentityResolved, makeHandle) live in the implementation
// layer: client/BLEClientImpl.cpp.

// Callback setters store the handler under impl.mtx and no-op with a warning on
// builds where GATT client support is compiled out (BLE_GATT_CLIENT_SUPPORTED).
void BLEClient::onConnect(ConnectHandler handler) {
#if BLE_GATT_CLIENT_SUPPORTED
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onConnectCb = handler;
#else
  (void)handler;
  log_w("%s not supported (GATT client disabled)", __func__);
#endif
}

void BLEClient::onDisconnect(DisconnectHandler handler) {
#if BLE_GATT_CLIENT_SUPPORTED
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onDisconnectCb = handler;
#else
  (void)handler;
  log_w("%s not supported (GATT client disabled)", __func__);
#endif
}

void BLEClient::onConnectFail(ConnectFailHandler handler) {
#if BLE_GATT_CLIENT_SUPPORTED
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onConnectFailCb = handler;
#else
  (void)handler;
  log_w("%s not supported (GATT client disabled)", __func__);
#endif
}

void BLEClient::onMtuChanged(MtuChangedHandler handler) {
#if BLE_GATT_CLIENT_SUPPORTED
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onMtuChangedCb = handler;
#else
  (void)handler;
  log_w("%s not supported (GATT client disabled)", __func__);
#endif
}

void BLEClient::onConnParamsUpdateRequest(ConnParamsReqHandler handler) {
#if BLE_GATT_CLIENT_SUPPORTED
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onConnParamsReqCb = handler;
#else
  (void)handler;
  log_w("%s not supported (GATT client disabled)", __func__);
#endif
}

void BLEClient::onIdentity(IdentityHandler handler) {
#if BLE_GATT_CLIENT_SUPPORTED
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onIdentityCb = handler;
#else
  (void)handler;
  log_w("%s not supported (GATT client disabled)", __func__);
#endif
}

void BLEClient::resetCallbacks() {
#if BLE_GATT_CLIENT_SUPPORTED
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onConnectCb = nullptr;
  impl.onDisconnectCb = nullptr;
  impl.onConnectFailCb = nullptr;
  impl.onMtuChangedCb = nullptr;
  impl.onConnParamsReqCb = nullptr;
  impl.onIdentityCb = nullptr;
#endif
}

BTAddress BLEClient::getPeerAddress() const {
  BLE_CHECK_IMPL(BTAddress());
  return impl.peerAddress;
}

bool BLEClient::isConnected() const {
  BLE_CHECK_IMPL(false);
  return impl.connected;
}

#if BLE_GATT_CLIENT_SUPPORTED

BLERemoteService BLEClient::getService(const BLEUUID &uuid) {
  BLE_CHECK_IMPL(BLERemoteService());
  bool needDiscover;
  {
    BLELockGuard lock(impl.mtx);
    if (!impl.connected) {
      return BLERemoteService();
    }
    needDiscover = impl.discoveredServices.empty();
  }
  if (needDiscover && !discoverServices()) {
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

#else /* !BLE_GATT_CLIENT_SUPPORTED */

BLERemoteService BLEClient::getService(const BLEUUID &) {
  log_w("GATT client not supported");
  return BLERemoteService();
}

std::vector<BLERemoteService> BLEClient::getServices() const {
  log_w("GATT client not supported");
  return {};
}

#endif /* BLE_GATT_CLIENT_SUPPORTED */

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
  log_d("Client: setValue svc=%s chr=%s len=%u", serviceUUID.toString().c_str(), charUUID.toString().c_str(), value.length());
  BLERemoteService svc = getService(serviceUUID);
  if (!svc) {
    log_w("Client: setValue - service %s not found", serviceUUID.toString().c_str());
    return BTStatus::NotFound;
  }
  return svc.setValue(charUUID, value);
}

String BLEClient::toString() const {
  BLE_CHECK_IMPL("BLEClient(null)");
  String s = "BLEClient(peer=";
  s += impl.peerAddress.toString();
  s += impl.connected ? ", connected)" : ", disconnected)";
  return s;
}

#endif /* BLE_ENABLED */
