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

#include "BLE.h"
#include "BLEServer.h"
#include "impl/BLEImplHelpers.h"
#include "impl/BLEServerBackend.h"
#if BLE_GATT_SERVER_SUPPORTED
#include "impl/BLEServiceBackend.h"
#endif

#include "impl/BLEMutex.h"
#include "esp32-hal-log.h"

/**
 * @brief Construct a default (invalid) BLEServer handle.
 * @note The handle must be initialized via BLE.createServer() before use.
 */
BLEServer::BLEServer() : _impl(nullptr) {}

/**
 * @brief Check whether this handle references a valid server implementation.
 * @return true if the internal Impl pointer is non-null; false otherwise.
 * @note Uses BLE_CHECK_IMPL which logs an error and returns early if the
 *       implementation pointer has been released.
 */
BLEServer::operator bool() const {
  BLE_CHECK_IMPL(false);
  (void)impl;
  return true;
}

/**
 * @brief Register a handler called when a central connects.
 * @param handler Callback to invoke on connection, or nullptr to clear.
 * @note Thread-safe: the callback is stored under the Impl mutex.
 *       When GATT server support is compiled out, logs a warning and no-ops.
 */
void BLEServer::onConnect(ConnectHandler handler) {
#if BLE_GATT_SERVER_SUPPORTED
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onConnectCb = handler;
#else
  (void)handler;
  log_w("%s not supported (GATT server disabled)", __func__);
#endif
}

/**
 * @brief Register a handler called when a central disconnects.
 * @param handler Callback to invoke on disconnection, or nullptr to clear.
 * @note Thread-safe: the callback is stored under the Impl mutex.
 *       When GATT server support is compiled out, logs a warning and no-ops.
 */
void BLEServer::onDisconnect(DisconnectHandler handler) {
#if BLE_GATT_SERVER_SUPPORTED
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onDisconnectCb = handler;
#else
  (void)handler;
  log_w("%s not supported (GATT server disabled)", __func__);
#endif
}

/**
 * @brief Register a handler called when the ATT MTU changes.
 * @param handler Callback to invoke on MTU change, or nullptr to clear.
 * @note Thread-safe: the callback is stored under the Impl mutex.
 *       When GATT server support is compiled out, logs a warning and no-ops.
 */
void BLEServer::onMtuChanged(MtuChangedHandler handler) {
#if BLE_GATT_SERVER_SUPPORTED
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onMtuChangedCb = handler;
#else
  (void)handler;
  log_w("%s not supported (GATT server disabled)", __func__);
#endif
}

/**
 * @brief Register a handler called when connection parameters are updated.
 * @param handler Callback to invoke on parameter update, or nullptr to clear.
 * @note Thread-safe: the callback is stored under the Impl mutex.
 *       When GATT server support is compiled out, logs a warning and no-ops.
 */
void BLEServer::onConnParamsUpdate(ConnParamsHandler handler) {
#if BLE_GATT_SERVER_SUPPORTED
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onConnParamsCb = handler;
#else
  (void)handler;
  log_w("%s not supported (GATT server disabled)", __func__);
#endif
}

/**
 * @brief Register a handler called when peer identity is resolved.
 * @param handler Callback to invoke on identity resolution, or nullptr to clear.
 * @note Thread-safe: the callback is stored under the Impl mutex.
 *       When GATT server support is compiled out, logs a warning and no-ops.
 */
void BLEServer::onIdentity(IdentityHandler handler) {
#if BLE_GATT_SERVER_SUPPORTED
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onIdentityCb = handler;
#else
  (void)handler;
  log_w("%s not supported (GATT server disabled)", __func__);
#endif
}

/**
 * @brief Clear all registered callback handlers.
 * @note Thread-safe: all callbacks are cleared under the Impl mutex in a
 *       single critical section.
 */
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

#if BLE_GATT_SERVER_SUPPORTED

// --------------------------------------------------------------------------
// Connection helpers (shared between backends)
// --------------------------------------------------------------------------

/**
 * @brief Insert or update a connection entry in the connection list.
 * @param connHandle Connection handle used as the lookup key.
 * @param connInfo   Connection information to store.
 * @note If an entry with the same connHandle already exists it is updated
 *       in-place; otherwise a new entry is appended. Caller must hold the
 *       Impl mutex.
 */
void BLEServer::Impl::connSet(uint16_t connHandle, const BLEConnInfo &connInfo) {
  for (auto &entry : connections) {
    if (entry.first == connHandle) {
      entry.second = connInfo;
      return;
    }
  }
  connections.emplace_back(connHandle, connInfo);
}

/**
 * @brief Remove a connection entry by handle.
 * @param connHandle Connection handle to remove.
 * @note No-ops silently if connHandle is not found. Caller must hold the
 *       Impl mutex.
 */
void BLEServer::Impl::connErase(uint16_t connHandle) {
  for (auto it = connections.begin(); it != connections.end(); ++it) {
    if (it->first == connHandle) {
      connections.erase(it);
      return;
    }
  }
}

/**
 * @brief Find a connection entry by handle.
 * @param connHandle Connection handle to look up.
 * @return Pointer to the matching BLEConnInfo, or nullptr if not found.
 * @note The returned pointer is invalidated if the connection list is
 *       modified. Caller must hold the Impl mutex.
 */
BLEConnInfo *BLEServer::Impl::connFind(uint16_t connHandle) {
  for (auto &entry : connections) {
    if (entry.first == connHandle) {
      return &entry.second;
    }
  }
  return nullptr;
}

/**
 * @brief Create a BLEServer handle backed by the given Impl's shared_ptr.
 * @param impl Raw pointer to the server implementation.
 * @return A BLEServer handle backed by the given Impl's shared_ptr.
 */
BLEServer BLEServer::Impl::makeHandle(BLEServer::Impl *impl) {
  return BLEServer(impl->shared_from_this());
}

namespace ble_server_dispatch {

/**
 * @brief Dispatch the connect callback to user code.
 * @param impl     Raw pointer to the server implementation.
 * @param connInfo Connection information for the new link.
 * @note The callback is copied under the mutex and invoked outside the lock
 *       to avoid deadlocks in user code.
 */
void dispatchConnect(BLEServer::Impl *impl, const BLEConnInfo &connInfo) {
  log_i("Server: client connected, handle=%u addr=%s", connInfo.getHandle(), connInfo.getAddress().toString().c_str());
  decltype(impl->onConnectCb) cb;
  {
    BLELockGuard lock(impl->mtx);
    cb = impl->onConnectCb;
  }
  BLEServer serverHandle = BLEServer::Impl::makeHandle(impl);
  if (cb) {
    cb(serverHandle, connInfo);
  }
}

/**
 * @brief Dispatch the disconnect callback to user code.
 * @param impl     Raw pointer to the server implementation.
 * @param connInfo Connection information for the terminated link.
 * @param reason   HCI disconnect reason code.
 * @note The callback is copied under the mutex and invoked outside the lock
 *       to avoid deadlocks in user code.
 */
void dispatchDisconnect(BLEServer::Impl *impl, const BLEConnInfo &connInfo, uint8_t reason) {
  log_i("Server: client disconnected, handle=%u addr=%s reason=0x%02x", connInfo.getHandle(), connInfo.getAddress().toString().c_str(), reason);
  decltype(impl->onDisconnectCb) cb;
  {
    BLELockGuard lock(impl->mtx);
    cb = impl->onDisconnectCb;
  }
  BLEServer serverHandle = BLEServer::Impl::makeHandle(impl);
  if (cb) {
    cb(serverHandle, connInfo, reason);
  }
}

/**
 * @brief Dispatch the MTU-changed callback to user code.
 * @param impl     Raw pointer to the server implementation.
 * @param connInfo Connection information for the link.
 * @param mtu      The newly agreed ATT MTU size in bytes.
 * @note The callback is copied under the mutex and invoked outside the lock
 *       to avoid deadlocks in user code.
 */
void dispatchMtuChanged(BLEServer::Impl *impl, const BLEConnInfo &connInfo, uint16_t mtu) {
  log_d("Server: MTU changed, handle=%u mtu=%u", connInfo.getHandle(), mtu);
  decltype(impl->onMtuChangedCb) cb;
  {
    BLELockGuard lock(impl->mtx);
    cb = impl->onMtuChangedCb;
  }
  BLEServer serverHandle = BLEServer::Impl::makeHandle(impl);
  if (cb) {
    cb(serverHandle, connInfo, mtu);
  }
}

/**
 * @brief Dispatch the connection-parameters-updated callback to user code.
 * @param impl     Raw pointer to the server implementation.
 * @param connInfo Connection information reflecting the updated parameters.
 * @note The callback is copied under the mutex and invoked outside the lock
 *       to avoid deadlocks in user code.
 */
void dispatchConnParamsUpdate(BLEServer::Impl *impl, const BLEConnInfo &connInfo) {
  log_d(
    "Server: conn params updated, handle=%u interval=%u latency=%u timeout=%u", connInfo.getHandle(), connInfo.getInterval(), connInfo.getLatency(),
    connInfo.getSupervisionTimeout()
  );
  decltype(impl->onConnParamsCb) cb;
  {
    BLELockGuard lock(impl->mtx);
    cb = impl->onConnParamsCb;
  }
  BLEServer serverHandle = BLEServer::Impl::makeHandle(impl);
  if (cb) {
    cb(serverHandle, connInfo);
  }
}

}  // namespace ble_server_dispatch

// --------------------------------------------------------------------------
// BLEServer public API (stack-agnostic)
// --------------------------------------------------------------------------

/**
 * @brief Enable or disable automatic advertising restart after a disconnection.
 * @param enable true to restart advertising on disconnect; false to remain idle.
 */
void BLEServer::advertiseOnDisconnect(bool enable) {
  BLE_CHECK_IMPL();
  log_d("Server: advertiseOnDisconnect=%d", enable);
  impl.advertiseOnDisconnect = enable;
}

/**
 * @brief Create a new GATT service and register it on this server.
 * @param uuid       UUID of the service (16-bit, 32-bit, or 128-bit).
 * @param numHandles Maximum number of attribute handles reserved for this service.
 * @param instId     Instance ID to distinguish multiple services with the same UUID.
 * @return A BLEService handle, or an invalid handle if the Impl is null.
 * @note If a service with the same UUID and instId already exists, the
 *       existing handle is returned instead of creating a duplicate.
 *       Thread-safe: the service list is accessed under the Impl mutex.
 */
BLEService BLEServer::createService(const BLEUUID &uuid, uint32_t numHandles, uint8_t instId) {
  BLE_CHECK_IMPL(BLEService());
  BLELockGuard lock(impl.mtx);

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
  svc->server = _impl.get();
  impl.services.push_back(svc);

  return BLEService(svc);
}

/**
 * @brief Look up an existing service by UUID.
 * @param uuid UUID of the service to find.
 * @return The first matching BLEService handle, or an invalid handle if not found.
 * @note Performs a linear search; returns the first match when multiple
 *       services share the same UUID (different instId). Thread-safe.
 */
BLEService BLEServer::getService(const BLEUUID &uuid) {
  BLE_CHECK_IMPL(BLEService());
  BLELockGuard lock(impl.mtx);
  for (auto &svc : impl.services) {
    if (svc->uuid == uuid) {
      return BLEService(svc);
    }
  }
  log_d("Server: getService %s - not found", uuid.toString().c_str());
  return BLEService();
}

/**
 * @brief Get all services registered on this server.
 * @return A vector of BLEService handles (shared-ownership copies).
 * @note Thread-safe: the service list is read under the Impl mutex.
 */
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

/**
 * @brief Remove a service from this server and from the BLE stack.
 * @param service The service to remove.
 * @return BTStatus::Success on success, BTStatus::InvalidState if either handle
 *         is null, or BTStatus::NotSupported when GATT server is compiled out.
 * @note Delegates to the backend-specific bleServerRemoveService(). On NimBLE
 *       this rebuilds the entire GATT table.
 */
BTStatus BLEServer::removeService(const BLEService &service) {
#if BLE_GATT_SERVER_SUPPORTED
  if (!_impl || !service._impl) {
    return BTStatus::InvalidState;
  }
  return bleServerRemoveService(_impl.get(), service._impl);
#else
  (void)service;
  return BTStatus::NotSupported;
#endif
}

/**
 * @brief Check whether the server has been started.
 * @return true if start() completed successfully; false otherwise.
 */
bool BLEServer::isStarted() const {
  return _impl && _impl->started;
}

/**
 * @brief Get the number of currently connected centrals.
 * @return The active connection count, or 0 if the Impl is null.
 * @note Thread-safe: the connection list is read under the Impl mutex.
 */
size_t BLEServer::getConnectedCount() const {
  BLE_CHECK_IMPL(0);
  BLELockGuard lock(impl.mtx);
  return impl.connections.size();
}

/**
 * @brief Get connection information for every active link.
 * @return A vector of BLEConnInfo snapshots for each connected peer.
 * @note Thread-safe: the connection list is read under the Impl mutex.
 *       The returned vector is a snapshot; subsequent connects/disconnects
 *       are not reflected.
 */
std::vector<BLEConnInfo> BLEServer::getConnections() const {
  std::vector<BLEConnInfo> result;
  BLE_CHECK_IMPL(result);
  BLELockGuard lock(impl.mtx);
  for (const auto &entry : impl.connections) {
    result.push_back(entry.second);
  }
  return result;
}

/**
 * @brief Get the advertising handle associated with this server.
 * @return A BLEAdvertising handle obtained from the global BLE singleton.
 */
BLEAdvertising BLEServer::getAdvertising() {
  return BLE.getAdvertising();
}

/**
 * @brief Start advertising using the current advertising configuration.
 * @return BTStatus indicating success or the error encountered.
 */
BTStatus BLEServer::startAdvertising() {
  return BLE.startAdvertising();
}

/**
 * @brief Stop any active advertising.
 * @return BTStatus indicating success or the error encountered.
 */
BTStatus BLEServer::stopAdvertising() {
  return BLE.stopAdvertising();
}

#endif /* BLE_GATT_SERVER_SUPPORTED */

#endif /* BLE_ENABLED */
