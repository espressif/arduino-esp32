/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 * Copyright 2020-2025 Ryan Powell <ryan@nable-embedded.io> and
 * esp-nimble-cpp, NimBLE-Arduino contributors.
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
#if BLE_NIMBLE

#include "BLE.h"

#include "NimBLEClient.h"
#include "NimBLERemoteTypes.h"
#include "impl/BLEImplHelpers.h"
#include "impl/BLESecurityBackend.h"
#include "esp32-hal-log.h"

/**
 * @file NimBLEClient.cpp
 * @brief NimBLE backend for BLEClient.
 */

#if BLE_GATT_CLIENT_SUPPORTED

#include <host/ble_att.h>

namespace {

/**
 * @brief Notifies the connect-fail callback after copying it under the client lock.
 * @param impl Client implementation; callbacks are read from this object.
 * @param reason Host stack or GAP failure code for the failed connect attempt.
 * @note Callbacks are copied from @p impl under a lock, then invoked outside the lock
 *   (see dispatch* helpers) to avoid deadlocks with user code.
 */
void dispatchConnectFail(BLEClient::Impl *impl, int reason) {
  decltype(impl->onConnectFailCb) cb;
  {
    BLELockGuard lock(impl->mtx);
    cb = impl->onConnectFailCb;
  }
  BLEClient clientHandle = BLEClient::Impl::makeHandle(impl);
  if (cb) {
    cb(clientHandle, reason);
  }
}

/**
 * @brief Notifies the connect callback after copying it under the client lock.
 * @param impl Client implementation; callbacks are read from this object.
 * @param connInfo Snapshot of the new connection.
 * @note Callbacks are copied from @p impl under a lock, then invoked outside the lock
 *   (see dispatch* helpers) to avoid deadlocks with user code.
 */
void dispatchConnect(BLEClient::Impl *impl, const BLEConnInfo &connInfo) {
  decltype(impl->onConnectCb) cb;
  {
    BLELockGuard lock(impl->mtx);
    cb = impl->onConnectCb;
  }
  BLEClient clientHandle = BLEClient::Impl::makeHandle(impl);
  if (cb) {
    cb(clientHandle, connInfo);
  }
}

/**
 * @brief Notifies the disconnect callback after copying it under the client lock.
 * @param impl Client implementation; callbacks are read from this object.
 * @param connInfo Connection details at the time of disconnect.
 * @param reason Bluetooth disconnect reason code.
 * @note Callbacks are copied from @p impl under a lock, then invoked outside the lock
 *   (see dispatch* helpers) to avoid deadlocks with user code.
 */
void dispatchDisconnect(BLEClient::Impl *impl, const BLEConnInfo &connInfo, uint8_t reason) {
  decltype(impl->onDisconnectCb) cb;
  {
    BLELockGuard lock(impl->mtx);
    cb = impl->onDisconnectCb;
  }
  BLEClient clientHandle = BLEClient::Impl::makeHandle(impl);
  if (cb) {
    cb(clientHandle, connInfo, reason);
  }
}

/**
 * @brief Notifies the MTU-changed callback after copying it under the client lock.
 * @param impl Client implementation; callbacks are read from this object.
 * @param connInfo Connection whose ATT MTU changed.
 * @param mtu Negotiated ATT MTU in octets.
 * @note Callbacks are copied from @p impl under a lock, then invoked outside the lock
 *   (see dispatch* helpers) to avoid deadlocks with user code.
 */
void dispatchMtuChanged(BLEClient::Impl *impl, const BLEConnInfo &connInfo, uint16_t mtu) {
  decltype(impl->onMtuChangedCb) cb;
  {
    BLELockGuard lock(impl->mtx);
    cb = impl->onMtuChangedCb;
  }
  BLEClient clientHandle = BLEClient::Impl::makeHandle(impl);
  if (cb) {
    cb(clientHandle, connInfo, mtu);
  }
}

/**
 * @brief Asks the connection-parameter request callback and returns whether to accept peer values.
 * @param impl Client implementation; callbacks are read from this object.
 * @param params Peer-proposed connection parameters.
 * @return @c true to accept the peer's parameters, @c false to reject (returns BLE_ERR_CONN_PARMS to the stack).
 * @note Callbacks are copied from @p impl under a lock, then invoked outside the lock
 *   (see dispatch* helpers) to avoid deadlocks with user code.
 */
bool dispatchConnParamsRequest(BLEClient::Impl *impl, const BLEConnParams &params) {
  decltype(impl->onConnParamsReqCb) cb;
  {
    BLELockGuard lock(impl->mtx);
    cb = impl->onConnParamsReqCb;
  }
  BLEClient clientHandle = BLEClient::Impl::makeHandle(impl);
  bool accept = true;
  if (cb) {
    accept = cb(clientHandle, params);
  }
  return accept;
}

/**
 * @brief Notifies the identity-resolved (or related) callback after copying it under the client lock.
 * @param impl Client implementation; callbacks are read from this object.
 * @param connInfo Current connection information after address resolution or encryption change.
 * @note Callbacks are copied from @p impl under a lock, then invoked outside the lock
 *   (see dispatch* helpers) to avoid deadlocks with user code.
 */
void dispatchIdentity(BLEClient::Impl *impl, const BLEConnInfo &connInfo) {
  decltype(impl->onIdentityCb) cb;
  {
    BLELockGuard lock(impl->mtx);
    cb = impl->onIdentityCb;
  }
  BLEClient clientHandle = BLEClient::Impl::makeHandle(impl);
  if (cb) {
    cb(clientHandle, connInfo);
  }
}

}  // namespace

// --------------------------------------------------------------------------
// BLEClient public API
// --------------------------------------------------------------------------

/**
 * @brief Connects to a peripheral at the given address; blocks until complete or timeout.
 * @param address Target device address.
 * @param timeoutMs GAP connect procedure timeout in milliseconds.
 * @return Outcome: @c OK if connected, or an error/already-connected/invalid state code.
 */
BTStatus BLEClient::connect(const BTAddress &address, uint32_t timeoutMs) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  {
    BLELockGuard lock(impl.mtx);
    if (impl.connected) {
      log_w("Client: already connected");
      return BTStatus::AlreadyConnected;
    }
  }

  log_d("Client: connecting to %s (timeout=%u ms)", address.toString().c_str(), timeoutMs);
  impl.peerAddress = address;
  impl.connectSync.take();

  ble_addr_t addr;
  addr.type = static_cast<uint8_t>(address.type());
  memcpy(addr.val, address.data(), 6);

  impl.nimbleRef = _impl;
  int rc = ble_gap_connect(static_cast<uint8_t>(BLE.getOwnAddressType()), &addr, timeoutMs, NULL, Impl::gapEventHandler, _impl.get());
  if (rc != 0) {
    impl.nimbleRef.reset();
    impl.connectSync.give(BTStatus::Fail);
    log_e("ble_gap_connect: rc=%d", rc);
    return BTStatus::Fail;
  }

  BTStatus status = impl.connectSync.wait(timeoutMs + 500);
  if (status != BTStatus::OK) {
    log_w("Client: connection to %s failed/timed out", address.toString().c_str());
    ble_gap_conn_cancel();
    return status;
  }

  uint16_t handle;
  {
    BLELockGuard lock(impl.mtx);
    handle = impl.connHandle;
  }
  log_i("Client: connected to %s handle=%u", address.toString().c_str(), handle);

  if (impl.preferredMTU > 0 || ble_att_preferred_mtu() > BLE_ATT_MTU_DFLT) {
    ble_gattc_exchange_mtu(handle, Impl::mtuExchangeCb, _impl.get());
  }

  return BTStatus::OK;
}

/**
 * @brief Connects to a device discovered during scanning.
 * @param device Advertised device to connect to; must be valid.
 * @param timeoutMs GAP connect procedure timeout in milliseconds.
 * @return Outcome: @c OK on success, @c InvalidParam if @p device is invalid, or other error codes.
 */
BTStatus BLEClient::connect(const BLEAdvertisedDevice &device, uint32_t timeoutMs) {
  if (!device) {
    log_e("Client: connect called with invalid advertised device");
    return BTStatus::InvalidParam;
  }
  return connect(device.getAddress(), timeoutMs);
}

/**
 * @brief Connects using a preferred LE PHY (BLE 5.0 extended connect) when supported.
 * @param address Target device address.
 * @param phy Mask of preferred PHYs for the connection.
 * @param timeoutMs Synchronous wait timeout in milliseconds.
 * @return @c OK if connected, @c NotSupported if BLE 5.0 is unavailable, or another status on failure.
 * @note If @c BLE5_SUPPORTED is false, this function logs and returns @c NotSupported (no connect attempt).
 */
BTStatus BLEClient::connect(const BTAddress &address, BLEPhy phy, uint32_t timeoutMs) {
#if BLE5_SUPPORTED
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  {
    BLELockGuard lock(impl.mtx);
    if (impl.connected) {
      log_w("Client: already connected");
      return BTStatus::AlreadyConnected;
    }
  }

  impl.peerAddress = address;
  impl.connectSync.take();

  ble_addr_t addr;
  addr.type = static_cast<uint8_t>(address.type());
  memcpy(addr.val, address.data(), 6);

  struct ble_gap_ext_conn_params extParams[3] = {};
  uint8_t phyMask = static_cast<uint8_t>(phy);

  for (int i = 0; i < 3; i++) {
    extParams[i].scan_itvl = BLE_GAP_SCAN_FAST_INTERVAL_MIN;
    extParams[i].scan_window = BLE_GAP_SCAN_FAST_WINDOW;
    extParams[i].itvl_min = BLE_GAP_INITIAL_CONN_ITVL_MIN;
    extParams[i].itvl_max = BLE_GAP_INITIAL_CONN_ITVL_MAX;
    extParams[i].latency = 0;
    extParams[i].supervision_timeout = 0x0100;
    extParams[i].min_ce_len = 0;
    extParams[i].max_ce_len = 0;
  }

  impl.nimbleRef = _impl;
  int rc = ble_gap_ext_connect(
    static_cast<uint8_t>(BLE.getOwnAddressType()), &addr, timeoutMs, phyMask, extParams, extParams + 1, extParams + 2, Impl::gapEventHandler, _impl.get()
  );
  if (rc != 0) {
    impl.nimbleRef.reset();
    impl.connectSync.give(BTStatus::Fail);
    log_e("ble_gap_ext_connect: rc=%d", rc);
    return BTStatus::Fail;
  }

  BTStatus status = impl.connectSync.wait(timeoutMs + 500);
  if (status != BTStatus::OK) {
    log_w("Client: BLE5 connection to %s failed/timed out", address.toString().c_str());
    ble_gap_conn_cancel();
    return status;
  }

  uint16_t handle;
  {
    BLELockGuard lock(impl.mtx);
    handle = impl.connHandle;
  }
  log_i("Client: connected (BLE5) to %s handle=%u", address.toString().c_str(), handle);

  if (impl.preferredMTU > 0 || ble_att_preferred_mtu() > BLE_ATT_MTU_DFLT) {
    ble_gattc_exchange_mtu(handle, Impl::mtuExchangeCb, _impl.get());
  }

  return BTStatus::OK;
#else
  log_w("Client: connect(BLEPhy) not supported (BLE 5.0 unavailable)");
  return BTStatus::NotSupported;
#endif
}

/**
 * @brief Overload for connect-from-advertisement with PHY when extended connect is not built in.
 * @param device Unused; advertised device to connect to (not supported in this build).
 * @param phy Unused; preferred PHY.
 * @param timeoutMs Unused; connect timeout.
 * @return @c NotSupported.
 */
BTStatus BLEClient::connect(const BLEAdvertisedDevice & /*device*/, BLEPhy /*phy*/, uint32_t /*timeoutMs*/) {
  log_w("Client: connect(device, BLEPhy) not supported (BLE 5.0 unavailable)");
  return BTStatus::NotSupported;
}

/**
 * @brief Starts an asynchronous connect using the extended (BLE 5) API when available.
 * @param address Target device address.
 * @param phy Mask of preferred PHYs for the connection.
 * @return @c OK if the connect was started, @c NotSupported if BLE 5.0 is unavailable, or an error.
 * @note If @c BLE5_SUPPORTED is false, this function logs and returns @c NotSupported.
 */
BTStatus BLEClient::connectAsync(const BTAddress &address, BLEPhy phy) {
#if BLE5_SUPPORTED
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  {
    BLELockGuard lock(impl.mtx);
    if (impl.connected) {
      log_w("Client: connectAsync - already connected");
      return BTStatus::AlreadyConnected;
    }
  }

  impl.peerAddress = address;

  ble_addr_t addr;
  addr.type = static_cast<uint8_t>(address.type());
  memcpy(addr.val, address.data(), 6);

  struct ble_gap_ext_conn_params extParams[3] = {};
  uint8_t phyMask = static_cast<uint8_t>(phy);

  for (int i = 0; i < 3; i++) {
    extParams[i].scan_itvl = BLE_GAP_SCAN_FAST_INTERVAL_MIN;
    extParams[i].scan_window = BLE_GAP_SCAN_FAST_WINDOW;
    extParams[i].itvl_min = BLE_GAP_INITIAL_CONN_ITVL_MIN;
    extParams[i].itvl_max = BLE_GAP_INITIAL_CONN_ITVL_MAX;
    extParams[i].latency = 0;
    extParams[i].supervision_timeout = 0x0100;
    extParams[i].min_ce_len = 0;
    extParams[i].max_ce_len = 0;
  }

  impl.nimbleRef = _impl;
  int rc = ble_gap_ext_connect(
    static_cast<uint8_t>(BLE.getOwnAddressType()), &addr, 30000, phyMask, extParams, extParams + 1, extParams + 2, Impl::gapEventHandler, _impl.get()
  );
  if (rc != 0) {
    impl.nimbleRef.reset();
    log_e("connectAsync: rc=%d", rc);
    return BTStatus::Fail;
  }
  return BTStatus::OK;
#else
  log_w("Client: connectAsync(BLEPhy) not supported (BLE 5.0 unavailable)");
  return BTStatus::NotSupported;
#endif
}

// Unavailable when BLE 5.0 / extended advertising is not built in: returns NotSupported.
BTStatus BLEClient::connectAsync(const BLEAdvertisedDevice & /*device*/, BLEPhy /*phy*/) {
  log_w("Client: connectAsync(device, BLEPhy) not supported (BLE 5.0 unavailable)");
  return BTStatus::NotSupported;
}

/**
 * @brief Attempts to cancel an in-progress connection establishment.
 * @return @c OK on success, or @c Fail if the stack rejected cancel.
 */
BTStatus BLEClient::cancelConnect() {
  int rc = ble_gap_conn_cancel();
  if (rc != 0) {
    log_e("cancelConnect: ble_gap_conn_cancel rc=%d", rc);
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

/**
 * @brief Disconnects the current GAP link with remote-user-termination reason.
 * @return @c OK if terminate was issued, @c InvalidState if not connected, or @c Fail on stack error.
 */
BTStatus BLEClient::disconnect() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  uint16_t handle;
  {
    BLELockGuard lock(impl.mtx);
    if (!impl.connected) {
      log_w("Client: disconnect called but not connected");
      return BTStatus::InvalidState;
    }
    handle = impl.connHandle;
  }
  int rc = ble_gap_terminate(handle, BLE_ERR_REM_USER_CONN_TERM);
  if (rc != 0) {
    log_e("Client: ble_gap_terminate rc=%d", rc);
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

/**
 * @brief Initiates pairing/encryption (SMP) on the current connection.
 * @return @c OK if the security procedure was started, @c InvalidState if not connected, or @c Fail.
 */
BTStatus BLEClient::secureConnection() {
  if (!_impl) {
    log_w("Client: secureConnection called but not connected");
    return BTStatus::InvalidState;
  }
  uint16_t handle;
  {
    BLELockGuard lock(_impl->mtx);
    if (!_impl->connected) {
      log_w("Client: secureConnection called but not connected");
      return BTStatus::InvalidState;
    }
    handle = _impl->connHandle;
  }
  int rc = ble_gap_security_initiate(handle);
  if (rc != 0) {
    log_e("Client: ble_gap_security_initiate rc=%d", rc);
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

// --------------------------------------------------------------------------
// Service discovery
// --------------------------------------------------------------------------

/**
 * @brief Discovers all GATT services on the peer and populates the client cache.
 * @return @c OK if discovery finished, @c InvalidState if not connected, or an error/timeout.
 */
BTStatus BLEClient::discoverServices() {
  if (!_impl) {
    return BTStatus::InvalidState;
  }
  uint16_t handle;
  {
    BLELockGuard lock(_impl->mtx);
    if (!_impl->connected) {
      return BTStatus::InvalidState;
    }
    if (_impl->discovering) {
      return BTStatus::Fail;
    }
    _impl->discovering = true;
    _impl->discoveredServices.clear();
    handle = _impl->connHandle;
  }

  log_d("Client: discovering services on handle=%u", handle);
  _impl->discoverSync.take();

  int rc = ble_gattc_disc_all_svcs(handle, Impl::serviceDiscoveryCb, _impl.get());
  if (rc != 0) {
    {
      BLELockGuard lock(_impl->mtx);
      _impl->discovering = false;
    }
    _impl->discoverSync.give(BTStatus::Fail);
    log_e("ble_gattc_disc_all_svcs: rc=%d", rc);
    return BTStatus::Fail;
  }

  BTStatus status = _impl->discoverSync.wait(10000);
  {
    BLELockGuard lock(_impl->mtx);
    _impl->discovering = false;
  }
  if (status == BTStatus::OK) {
    log_i("Client: discovered %u service(s)", (unsigned)_impl->discoveredServices.size());
  } else {
    log_w("Client: service discovery failed/timed out");
  }
  return status;
}

/**
 * @brief Returns a handle for a remote service by UUID, running discovery if needed.
 * @param uuid Service UUID to look up.
 * @return A valid @c BLERemoteService if found, or an empty handle on failure/timeout/not connected.
 */
BLERemoteService BLEClient::getService(const BLEUUID &uuid) {
  if (!_impl) {
    return BLERemoteService();
  }
  bool needDiscover;
  {
    BLELockGuard lock(_impl->mtx);
    if (!_impl->connected) {
      return BLERemoteService();
    }
    needDiscover = _impl->discoveredServices.empty();
  }

  if (needDiscover) {
    if (discoverServices() != BTStatus::OK) {
      return BLERemoteService();
    }
  }

  BLEUUID target = uuid.to128();
  BLELockGuard lock(_impl->mtx);
  uint16_t handle = _impl->connHandle;
  for (auto &entry : _impl->discoveredServices) {
    if (entry.uuid.to128() == target) {
      auto sImpl = std::make_shared<BLERemoteService::Impl>();
      sImpl->uuid = entry.uuid;
      sImpl->startHandle = entry.startHandle;
      sImpl->endHandle = entry.endHandle;
      sImpl->connHandle = handle;
      sImpl->client = _impl.get();

      return BLERemoteService(sImpl);
    }
  }
  return BLERemoteService();
}

/**
 * @brief Returns all discovered (or newly discovered) remote services.
 * @return A vector of service handles; may trigger lazy discovery on first use when the cache is empty.
 */
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
  uint16_t handle = impl.connHandle;
  for (auto &entry : impl.discoveredServices) {
    auto sImpl = std::make_shared<BLERemoteService::Impl>();
    sImpl->uuid = entry.uuid;
    sImpl->startHandle = entry.startHandle;
    sImpl->endHandle = entry.endHandle;
    sImpl->connHandle = handle;
    sImpl->client = _impl.get();

    result.push_back(BLERemoteService(sImpl));
  }
  return result;
}

// --------------------------------------------------------------------------
// Connection info
// --------------------------------------------------------------------------

/**
 * @brief Sets the preferred ATT MTU; exchanges MTU on the link if already connected.
 * @param mtu Preferred MTU in octets.
 */
void BLEClient::setMTU(uint16_t mtu) {
  BLE_CHECK_IMPL();
  impl.preferredMTU = mtu;
  uint16_t handle;
  bool isConn;
  {
    BLELockGuard lock(impl.mtx);
    isConn = impl.connected;
    handle = impl.connHandle;
  }
  if (isConn) {
    ble_gattc_exchange_mtu(handle, Impl::mtuExchangeCb, _impl.get());
  }
}

/**
 * @brief Returns the current effective ATT MTU for the connection.
 * @return Negotiated MTU, or the default if not connected.
 */
uint16_t BLEClient::getMTU() const {
  if (!_impl) {
    return BLE_ATT_MTU_DFLT;
  }
  uint16_t handle;
  {
    BLELockGuard lock(_impl->mtx);
    if (!_impl->connected) {
      return BLE_ATT_MTU_DFLT;
    }
    handle = _impl->connHandle;
  }
  return ble_att_mtu(handle);
}

/**
 * @brief Reads the received signal strength of the current connection.
 * @return RSSI in dBm, or @c -128 if not connected or on read failure.
 */
int8_t BLEClient::getRSSI() const {
  if (!_impl) {
    return -128;
  }
  uint16_t handle;
  {
    BLELockGuard lock(_impl->mtx);
    if (!_impl->connected) {
      return -128;
    }
    handle = _impl->connHandle;
  }
  int8_t rssi;
  int rc = ble_gap_conn_rssi(handle, &rssi);
  if (rc != 0) {
    log_w("Client: getRSSI failed rc=%d", rc);
    return -128;
  }
  return rssi;
}

/**
 * @brief Returns the GAP connection handle.
 * @return The NimBLE connection handle, or @c BLE_HS_CONN_HANDLE_NONE if there is no impl.
 */
uint16_t BLEClient::getHandle() const {
  if (!_impl) {
    return BLE_HS_CONN_HANDLE_NONE;
  }
  BLELockGuard lock(_impl->mtx);
  return _impl->connHandle;
}

/**
 * @brief Snapshot of current GAP link parameters and addressing.
 * @return Populated @c BLEConnInfo on success, or a default/empty value if not connected.
 */
BLEConnInfo BLEClient::getConnection() const {
  if (!_impl) {
    return BLEConnInfo();
  }
  uint16_t handle;
  {
    BLELockGuard lock(_impl->mtx);
    if (!_impl->connected) {
      return BLEConnInfo();
    }
    handle = _impl->connHandle;
  }
  struct ble_gap_conn_desc desc;
  int rc = ble_gap_conn_find(handle, &desc);
  if (rc != 0) {
    log_w("Client: ble_gap_conn_find rc=%d", rc);
    return BLEConnInfo();
  }
  return BLEConnInfoImpl::fromDesc(desc);
}

/**
 * @brief Requests an update to connection interval, latency, and supervision timeout.
 * @param params Desired connection parameters.
 * @return @c OK if the update was requested, or @c InvalidState / @c Fail on error.
 * @note Connection parameter ranges per BT Core Spec v5.x, Vol 6, Part B, §4.5.1:
 *       - Interval: 6–3200 (7.5 ms – 4 s in 1.25 ms units).
 *       - Latency: 0–499.
 *       - Supervision timeout: 10–3200 (100 ms – 32 s in 10 ms units),
 *         must satisfy: timeout > (1 + latency) × maxInterval × 2.
 *       Invalid parameters are rejected locally before sending to the controller.
 */
BTStatus BLEClient::updateConnParams(const BLEConnParams &params) {
  if (!_impl) {
    log_w("Client: updateConnParams called but not connected");
    return BTStatus::InvalidState;
  }

  if (!params.isValid()) {
    log_e("Client: updateConnParams — parameters out of spec");
    return BTStatus::InvalidParam;
  }

  uint16_t handle;
  {
    BLELockGuard lock(_impl->mtx);
    if (!_impl->connected) {
      log_w("Client: updateConnParams called but not connected");
      return BTStatus::InvalidState;
    }
    handle = _impl->connHandle;
  }
  struct ble_gap_upd_params nimParams = {};
  nimParams.itvl_min = params.minInterval;
  nimParams.itvl_max = params.maxInterval;
  nimParams.latency = params.latency;
  nimParams.supervision_timeout = params.timeout;
  int rc = ble_gap_update_params(handle, &nimParams);
  if (rc != 0) {
    log_e("Client: ble_gap_update_params rc=%d", rc);
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

/**
 * @brief Sets the preferred TX and RX LE PHYs when BLE 5 is supported.
 * @param txPhy Preferred transmitter PHY.
 * @param rxPhy Preferred receiver PHY.
 * @return @c OK on success, @c NotSupported without BLE 5, or an error if not connected/failed.
 * @note If @c BLE5_SUPPORTED is false, logs a warning and returns @c NotSupported.
 */
BTStatus BLEClient::setPhy(BLEPhy txPhy, BLEPhy rxPhy) {
#if BLE5_SUPPORTED
  if (!_impl) {
    log_w("Client: setPhy called but not connected");
    return BTStatus::InvalidState;
  }
  uint16_t handle;
  {
    BLELockGuard lock(_impl->mtx);
    if (!_impl->connected) {
      log_w("Client: setPhy called but not connected");
      return BTStatus::InvalidState;
    }
    handle = _impl->connHandle;
  }
  int rc = ble_gap_set_prefered_le_phy(handle, static_cast<uint8_t>(txPhy), static_cast<uint8_t>(rxPhy), 0);
  if (rc != 0) {
    log_e("Client: ble_gap_set_prefered_le_phy rc=%d", rc);
    return BTStatus::Fail;
  }
  return BTStatus::OK;
#else
  log_w("Client: setPhy not supported (BLE 5.0 unavailable)");
  return BTStatus::NotSupported;
#endif
}

/**
 * @brief Reads the current TX and RX LE PHYs when BLE 5 is supported.
 * @param txPhy Filled with the current TX PHY on success.
 * @param rxPhy Filled with the current RX PHY on success.
 * @return @c OK on success, @c NotSupported without BLE 5, or @c InvalidState / @c Fail.
 * @note If @c BLE5_SUPPORTED is false, logs a warning and returns @c NotSupported.
 */
BTStatus BLEClient::getPhy(BLEPhy &txPhy, BLEPhy &rxPhy) const {
#if BLE5_SUPPORTED
  if (!_impl) {
    log_w("Client: getPhy called but not connected");
    return BTStatus::InvalidState;
  }
  uint16_t handle;
  {
    BLELockGuard lock(_impl->mtx);
    if (!_impl->connected) {
      log_w("Client: getPhy called but not connected");
      return BTStatus::InvalidState;
    }
    handle = _impl->connHandle;
  }
  uint8_t tx, rx;
  int rc = ble_gap_read_le_phy(handle, &tx, &rx);
  if (rc == 0) {
    txPhy = static_cast<BLEPhy>(tx);
    rxPhy = static_cast<BLEPhy>(rx);
    return BTStatus::OK;
  }
  log_e("Client: ble_gap_read_le_phy rc=%d", rc);
  return BTStatus::Fail;
#else
  log_w("Client: getPhy not supported (BLE 5.0 unavailable)");
  return BTStatus::NotSupported;
#endif
}

/**
 * @brief Sets the maximum data length (LE Data Length Extension) for the connection.
 * @param txOctets Max TX payload octets.
 * @param txTime Max TX time (as defined by the stack API).
 * @return @c OK on success, or @c InvalidState if not connected, or @c Fail.
 */
BTStatus BLEClient::setDataLen(uint16_t txOctets, uint16_t txTime) {
  if (!_impl) {
    log_w("Client: setDataLen called but not connected");
    return BTStatus::InvalidState;
  }
  uint16_t handle;
  {
    BLELockGuard lock(_impl->mtx);
    if (!_impl->connected) {
      log_w("Client: setDataLen called but not connected");
      return BTStatus::InvalidState;
    }
    handle = _impl->connHandle;
  }
  int rc = ble_gap_set_data_len(handle, txOctets, txTime);
  if (rc != 0) {
    log_e("Client: ble_gap_set_data_len rc=%d", rc);
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

// --------------------------------------------------------------------------
// GAP event handler
// --------------------------------------------------------------------------

/**
 * @brief NimBLE GAP event callback for the GATT client; dispatches user callbacks for connect, disconnect, MTU, and security.
 * @param event GAP event from the stack (type in @c event->type).
 * @param arg Pointer to the owning @c BLEClient::Impl.
 * @return Host status: @c 0 to continue, or @c BLE_ERR_CONN_PARMS to reject a connection-parameter update.
 * @note User callbacks that must not run under the client lock are delivered via the dispatch* helpers: the callback
 *   function pointer is copied from @c impl under a lock, then invoked outside the lock.
 */
int BLEClient::Impl::gapEventHandler(struct ble_gap_event *event, void *arg) {
  auto *impl = static_cast<BLEClient::Impl *>(arg);
  if (!impl) {
    return 0;
  }

  switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
    {
      if (event->connect.status != 0) {
        log_e("Client: connection failed, status=%d", event->connect.status);
        {
          BLELockGuard lock(impl->mtx);
          impl->connected = false;
          impl->connHandle = BLE_HS_CONN_HANDLE_NONE;
        }
        impl->connectSync.give(BTStatus::Fail);
        dispatchConnectFail(impl, event->connect.status);
        impl->nimbleRef.reset();
        return 0;
      }

      {
        BLELockGuard lock(impl->mtx);
        impl->connHandle = event->connect.conn_handle;
        impl->connected = true;
      }
      log_i("Client: connected, handle=%u", event->connect.conn_handle);
      impl->connectSync.give(BTStatus::OK);

      struct ble_gap_conn_desc desc;
      int rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
      if (rc != 0) {
        return 0;
      }

      BLEConnInfo connInfo = BLEConnInfoImpl::fromDesc(desc);
      dispatchConnect(impl, connInfo);
      return 0;
    }

    case BLE_GAP_EVENT_DISCONNECT:
    {
      uint16_t evtHandle = event->disconnect.conn.conn_handle;
      {
        BLELockGuard lock(impl->mtx);
        if (evtHandle != impl->connHandle) {
          return 0;
        }
      }

      BLEConnInfo connInfo = BLEConnInfoImpl::fromDesc(event->disconnect.conn);
      uint8_t reason = event->disconnect.reason;

      log_i("Client: disconnected, handle=%u reason=0x%02x", evtHandle, reason);
      {
        BLELockGuard lock(impl->mtx);
        impl->connected = false;
        impl->connHandle = BLE_HS_CONN_HANDLE_NONE;
      }

      dispatchDisconnect(impl, connInfo, reason);
      impl->nimbleRef.reset();
      return 0;
    }

    case BLE_GAP_EVENT_MTU:
    {
      {
        BLELockGuard lock(impl->mtx);
        if (event->mtu.conn_handle != impl->connHandle) {
          return 0;
        }
      }

      struct ble_gap_conn_desc desc;
      int rc = ble_gap_conn_find(event->mtu.conn_handle, &desc);
      if (rc != 0) {
        return 0;
      }

      BLEConnInfo connInfo = BLEConnInfoImpl::fromDesc(desc);

      log_d("Client: MTU changed, handle=%u mtu=%u", event->mtu.conn_handle, event->mtu.value);
      dispatchMtuChanged(impl, connInfo, event->mtu.value);
      return 0;
    }

    case BLE_GAP_EVENT_CONN_UPDATE_REQ:
    {
      {
        BLELockGuard lock(impl->mtx);
        if (event->conn_update_req.conn_handle != impl->connHandle) {
          return 0;
        }
      }

      const auto *peer = event->conn_update_req.peer_params;
      BLEConnParams params;
      params.minInterval = peer->itvl_min;
      params.maxInterval = peer->itvl_max;
      params.latency = peer->latency;
      params.timeout = peer->supervision_timeout;

      if (!dispatchConnParamsRequest(impl, params)) {
        return BLE_ERR_CONN_PARMS;
      }
      return 0;
    }

    case BLE_GAP_EVENT_ENC_CHANGE:
    case BLE_GAP_EVENT_IDENTITY_RESOLVED:
    {
      uint16_t connHandle = (event->type == BLE_GAP_EVENT_ENC_CHANGE) ? event->enc_change.conn_handle : event->identity_resolved.conn_handle;
      {
        BLELockGuard lock(impl->mtx);
        if (connHandle != impl->connHandle) {
          return 0;
        }
      }

      struct ble_gap_conn_desc desc;
      int rc = ble_gap_conn_find(connHandle, &desc);
      if (rc != 0) {
        return 0;
      }

      BLEConnInfo connInfo = BLEConnInfoImpl::fromDesc(desc);

      dispatchIdentity(impl, connInfo);

#if BLE_SMP_SUPPORTED
      if (event->type == BLE_GAP_EVENT_ENC_CHANGE) {
        BLESecurity sec = BLE.getSecurity();
        if (sec) {
          BLESecurityBackend::notifyAuthComplete(sec, connInfo, event->enc_change.status == 0);
        }
      }
#endif /* BLE_SMP_SUPPORTED */
      return 0;
    }

    case BLE_GAP_EVENT_NOTIFY_RX:
    {
      BLERemoteCharacteristic::Impl::handleNotifyRx(
        event->notify_rx.conn_handle, event->notify_rx.attr_handle, event->notify_rx.om, event->notify_rx.indication == 0
      );
      return 0;
    }

#if BLE_SMP_SUPPORTED
    case BLE_GAP_EVENT_PASSKEY_ACTION:
    {
      struct ble_gap_conn_desc desc;
      int rc = ble_gap_conn_find(event->passkey.conn_handle, &desc);
      if (rc != 0) {
        return 0;
      }

      BLEConnInfo connInfo = BLEConnInfoImpl::fromDesc(desc);
      BLESecurity sec = BLE.getSecurity();
      if (!sec) {
        return 0;
      }

      if (event->passkey.params.action == BLE_SM_IOACT_DISP) {
        struct ble_sm_io pkey = {};
        pkey.action = BLE_SM_IOACT_DISP;
        pkey.passkey = BLESecurityBackend::resolvePasskeyForDisplay(sec, connInfo);
        ble_sm_inject_io(event->passkey.conn_handle, &pkey);
      } else if (event->passkey.params.action == BLE_SM_IOACT_INPUT) {
        struct ble_sm_io pkey = {};
        pkey.action = BLE_SM_IOACT_INPUT;
        pkey.passkey = BLESecurityBackend::resolvePasskeyForInput(sec, connInfo);
        ble_sm_inject_io(event->passkey.conn_handle, &pkey);
      } else if (event->passkey.params.action == BLE_SM_IOACT_NUMCMP) {
        struct ble_sm_io pkey = {};
        pkey.action = BLE_SM_IOACT_NUMCMP;
        pkey.numcmp_accept = BLESecurityBackend::resolveNumericComparison(sec, connInfo, event->passkey.params.numcmp) ? 1 : 0;
        ble_sm_inject_io(event->passkey.conn_handle, &pkey);
      }
      return 0;
    }
#endif /* BLE_SMP_SUPPORTED */

    default: return 0;
  }
}

// --------------------------------------------------------------------------
// Service discovery callback
// --------------------------------------------------------------------------

/**
 * @brief @c ble_gattc_disc_all_svcs callback: accumulates service records then signals sync completion.
 * @param connHandle Connection handle (must match the discovering client).
 * @param error GATT operation status; @c BLE_HS_EDONE indicates discovery completed.
 * @param service One discovered service, or @c nullptr when the iteration is finished.
 * @param arg Pointer to the owning @c BLEClient::Impl.
 * @return @c 0 to continue the discovery procedure.
 */
int BLEClient::Impl::serviceDiscoveryCb(uint16_t connHandle, const struct ble_gatt_error *error, const struct ble_gatt_svc *service, void *arg) {
  auto *impl = static_cast<BLEClient::Impl *>(arg);
  if (!impl) {
    return 0;
  }

  if (error->status == 0 && service != nullptr) {
    RemoteServiceEntry entry;
    if (service->uuid.u.type == BLE_UUID_TYPE_16) {
      entry.uuid = BLEUUID(static_cast<uint16_t>(BLE_UUID16(&service->uuid.u)->value));
    } else if (service->uuid.u.type == BLE_UUID_TYPE_32) {
      entry.uuid = BLEUUID(static_cast<uint32_t>(BLE_UUID32(&service->uuid.u)->value));
    } else {
      entry.uuid = BLEUUID(BLE_UUID128(&service->uuid.u)->value, 16, true);
    }
    entry.startHandle = service->start_handle;
    entry.endHandle = service->end_handle;
    {
      BLELockGuard lock(impl->mtx);
      impl->discoveredServices.push_back(entry);
    }
    return 0;
  }

  if (error->status == BLE_HS_EDONE) {
    impl->discoverSync.give(BTStatus::OK);
  } else {
    impl->discoverSync.give(BTStatus::Fail);
  }
  return 0;
}

// --------------------------------------------------------------------------
// MTU exchange callback
// --------------------------------------------------------------------------

/**
 * @brief @c ble_gattc_exchange_mtu callback; logs the negotiated or failed MTU.
 * @param connHandle Connection handle.
 * @param error MTU procedure status.
 * @param mtu Negotiated ATT MTU when @p error->status is zero.
 * @param arg Opaque user argument (client impl pointer; currently unused in the body).
 * @return @c 0.
 */
int BLEClient::Impl::mtuExchangeCb(uint16_t connHandle, const struct ble_gatt_error *error, uint16_t mtu, void *arg) {
  if (error->status == 0) {
    log_i("MTU exchange complete: %d", mtu);
  } else {
    log_w("MTU exchange failed, status=%d", error->status);
  }
  return 0;
}

// --------------------------------------------------------------------------
// BLEClass::createClient() -- NimBLE factory method
// --------------------------------------------------------------------------

/**
 * @brief Creates a GATT client instance with a new NimBLE-backed @c BLEClient::Impl.
 * @return A connected-capable @c BLEClient, or an empty one if the stack was not initialized.
 */
BLEClient BLEClass::createClient() {
  if (!isInitialized()) {
    log_e("createClient: BLE not initialized");
    return BLEClient();
  }

  return BLEClient(std::make_shared<BLEClient::Impl>());
}

#else /* !BLE_GATT_CLIENT_SUPPORTED -- stubs */

// Stubs for BLE_GATT_CLIENT_SUPPORTED == 0: log on most entry points; return NotSupported, empty, 0,
// -128, 0xFFFF, or no-op. getPhy() does not set out-parameters; initialize or check the return.

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

BLERemoteService BLEClient::getService(const BLEUUID &) {
  log_w("GATT client not supported");
  return BLERemoteService();
}

std::vector<BLERemoteService> BLEClient::getServices() const {
  log_w("GATT client not supported");
  return {};
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

BLEConnInfo BLEClient::getConnection() const {
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

int BLEClient::Impl::gapEventHandler(struct ble_gap_event *, void *) {
  return 0;
}

BLEClient BLEClass::createClient() {
  log_w("GATT client not supported");
  return BLEClient();
}

#endif /* BLE_GATT_CLIENT_SUPPORTED */

#endif /* BLE_NIMBLE */
