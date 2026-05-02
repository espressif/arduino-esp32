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

#include "impl/BLEGuards.h"
#if BLE_ENABLED

#include "BLEClient.h"
#include "BLERemoteService.h"
#include "impl/BLEImplHelpers.h"
#include "impl/BLEClientBackend.h"

#include "impl/BLEMutex.h"
#include "esp32-hal-log.h"

/**
 * @file
 * @brief Common @ref BLEClient implementation: callback registration, shorthands,
 *        and glue to the active GATT client backend.
 */

/**
 * @brief Default constructor — creates an empty client handle.
 *
 * The handle is not usable until assigned from BLE.createClient().
 */
BLEClient::BLEClient() : _impl(nullptr) {}

/**
 * @brief Boolean conversion — true if this handle references a valid client.
 *
 * @return true if the underlying Impl exists, false for default-constructed handles.
 */
BLEClient::operator bool() const {
  BLE_CHECK_IMPL(false);
  (void)impl;
  return true;
}

/**
 * @brief Set the callback invoked when a connection is established.
 *
 * @note Thread-safe: acquires the impl mutex before storing the handler.
 * @note No-op with a warning on builds where GATT client support is disabled.
 *
 * @param handler  ConnectHandler callback, or nullptr to clear.
 */
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

/**
 * @brief Set the callback invoked when a connection is terminated.
 *
 * @note Thread-safe: acquires the impl mutex before storing the handler.
 * @note No-op with a warning on builds where GATT client support is disabled.
 *
 * @param handler  DisconnectHandler callback, or nullptr to clear.
 */
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

/**
 * @brief Set the callback invoked when a connection attempt fails.
 *
 * @note Thread-safe: acquires the impl mutex before storing the handler.
 * @note No-op with a warning on builds where GATT client support is disabled.
 *
 * @param handler  ConnectFailHandler callback, or nullptr to clear.
 */
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

/**
 * @brief Set the callback invoked when the ATT MTU changes after negotiation.
 *
 * @note Thread-safe: acquires the impl mutex before storing the handler.
 * @note No-op with a warning on builds where GATT client support is disabled.
 *
 * @param handler  MtuChangedHandler callback, or nullptr to clear.
 */
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

/**
 * @brief Set the callback for remote connection-parameter update requests.
 *
 * The handler's boolean return value controls whether the requested parameters
 * are accepted (true) or rejected (false).
 *
 * @note Thread-safe: acquires the impl mutex before storing the handler.
 * @note No-op with a warning on builds where GATT client support is disabled.
 *
 * @param handler  ConnParamsReqHandler callback, or nullptr to clear.
 */
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

/**
 * @brief Set the callback invoked when the peer's identity is resolved.
 *
 * Fired after pairing/bonding resolves an RPA to the peer's identity address.
 *
 * @note Thread-safe: acquires the impl mutex before storing the handler.
 * @note No-op with a warning on builds where GATT client support is disabled.
 *
 * @param handler  IdentityHandler callback, or nullptr to clear.
 */
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

/**
 * @brief Remove all registered callbacks.
 *
 * Sets every callback (connect, disconnect, connect-fail, MTU-changed,
 * connection-params-request, identity) to nullptr.
 *
 * @note Thread-safe: acquires the impl mutex while clearing.
 */
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

#if BLE_GATT_CLIENT_SUPPORTED

/**
 * @brief Create a BLEClient handle backed by the given Impl's shared_ptr.
 * @param impl  Raw pointer to an existing Impl.
 * @return A BLEClient handle backed by the given Impl's shared_ptr.
 */
BLEClient BLEClient::Impl::makeHandle(BLEClient::Impl *impl) {
  return BLEClient(impl->shared_from_this());
}

/**
 * @brief Get the address of the connected (or last connected) peer.
 *
 * @return The peer's BTAddress, or a default-constructed BTAddress if the
 *         handle has no Impl (i.e. was default-constructed).
 */
BTAddress BLEClient::getPeerAddress() const {
  return _impl ? _impl->peerAddress : BTAddress();
}

/**
 * @brief Check whether this client is currently connected.
 *
 * @return true if the Impl exists and the connection flag is set.
 */
bool BLEClient::isConnected() const {
  return _impl && _impl->connected;
}

/**
 * @brief Read a characteristic value by service and characteristic UUID.
 *
 * Convenience method that first looks up the service, then delegates to
 * BLERemoteService::getValue(). Requires prior service discovery via
 * discoverServices().
 *
 * @param serviceUUID  UUID of the containing service.
 * @param charUUID     UUID of the characteristic to read.
 * @return The characteristic value as a String, or an empty String if the
 *         service or characteristic was not found.
 * @note @ref getService may run service discovery if the cache is still empty,
 *       so the first call can block longer than a simple read.
 */
String BLEClient::getValue(const BLEUUID &serviceUUID, const BLEUUID &charUUID) {
  log_d("Client: getValue svc=%s chr=%s", serviceUUID.toString().c_str(), charUUID.toString().c_str());
  BLERemoteService svc = getService(serviceUUID);
  if (!svc) {
    log_w("Client: getValue - service %s not found", serviceUUID.toString().c_str());
    return "";
  }
  return svc.getValue(charUUID);
}

/**
 * @brief Write a characteristic value by service and characteristic UUID.
 *
 * Convenience method that first looks up the service, then delegates to
 * BLERemoteService::setValue(). Requires prior service discovery via
 * discoverServices().
 *
 * @param serviceUUID  UUID of the containing service.
 * @param charUUID     UUID of the characteristic to write.
 * @param value        The value to write.
 * @return BTStatus::Success on success, BTStatus::NotFound if the service
 *         is not in the discovered cache, or a backend-specific error.
 * @note Same lazy-discovery behavior as @ref getValue: @ref getService may
 *       discover services on first use if the cache is empty.
 */
BTStatus BLEClient::setValue(const BLEUUID &serviceUUID, const BLEUUID &charUUID, const String &value) {
  log_d("Client: setValue svc=%s chr=%s len=%u", serviceUUID.toString().c_str(), charUUID.toString().c_str(), value.length());
  BLERemoteService svc = getService(serviceUUID);
  if (!svc) {
    log_w("Client: setValue - service %s not found", serviceUUID.toString().c_str());
    return BTStatus::NotFound;
  }
  return svc.setValue(charUUID, value);
}

/**
 * @brief Get a human-readable description of this client.
 *
 * @return A String of the form "BLEClient(peer=XX:XX:XX:XX:XX:XX, connected)"
 *         or "BLEClient(null)" for a default-constructed handle.
 */
String BLEClient::toString() const {
  if (!_impl) {
    return "BLEClient(null)";
  }
  String s = "BLEClient(peer=";
  s += _impl->peerAddress.toString();
  s += _impl->connected ? ", connected)" : ", disconnected)";
  return s;
}

#endif /* BLE_GATT_CLIENT_SUPPORTED */

#endif /* BLE_ENABLED */
