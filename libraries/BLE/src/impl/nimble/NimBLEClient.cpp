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

#include "impl/common/BLEGuards.h"
#if BLE_NIMBLE

#include "BLE.h"

#include "NimBLEClient.h"
#include "NimBLERemoteGatt.h"
#include "NimBLEUUID.h"
#include "impl/common/BLEImplHelpers.h"
#include "impl/BLEBackend.h"
#include "esp32-hal-log.h"

/**
 * @file NimBLEClient.cpp
 * @brief NimBLE backend for BLEClient.
 */

// API contract is documented on the declarations in the public BLE*.h headers; the definitions below carry implementation notes only.

#if BLE_GATT_CLIENT_SUPPORTED

#include <host/ble_att.h>

namespace {

// dispatchConnect / dispatchDisconnect / dispatchConnectFail / dispatchMtuChanged
// are shared and live on BLEClientImplCommon (impl/common/BLEClientImpl.cpp);
// call them as impl->dispatchXxx(...). Only the NimBLE-specific
// connection-parameter *request* hook (which returns accept/reject and has no
// Bluedroid equivalent) stays here.

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

}  // namespace

// --------------------------------------------------------------------------
// BLEClient public API
// --------------------------------------------------------------------------

// Blocks until the connection completes or the timeout elapses.
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

  // Block until the MTU exchange settles so getMTU() is valid on return
  // (ordering contract in DESIGN.md). The negotiated value is read live
  // via ble_att_mtu(), so a Fail/Timeout here just leaves the default MTU in
  // place - not a connection failure.
  if (impl.preferredMTU > 0 || ble_att_preferred_mtu() > BLE_ATT_MTU_DFLT) {
    impl.mtuSync.take();
    if (ble_gattc_exchange_mtu(handle, Impl::mtuExchangeCb, _impl.get()) == 0) {
      impl.mtuSync.wait(timeoutMs);
    } else {
      impl.mtuSync.give(BTStatus::Fail);
    }
  }

  return BTStatus::OK;
}

BTStatus BLEClient::connect(const BLEAdvertisedDevice &device, uint32_t timeoutMs) {
  if (!device) {
    log_e("Client: connect called with invalid advertised device");
    return BTStatus::InvalidParam;
  }
  return connect(device.getAddress(), timeoutMs);
}

// BLE 5 extended connect; blocks until complete or timeout. Returns NotSupported (no connect
// attempt) when BLE5_SUPPORTED is off.
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

  struct ble_gap_conn_params extParams[3] = {};
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

  // See the legacy connect() path: block on the MTU exchange for getMTU() parity.
  if (impl.preferredMTU > 0 || ble_att_preferred_mtu() > BLE_ATT_MTU_DFLT) {
    impl.mtuSync.take();
    if (ble_gattc_exchange_mtu(handle, Impl::mtuExchangeCb, _impl.get()) == 0) {
      impl.mtuSync.wait(timeoutMs);
    } else {
      impl.mtuSync.give(BTStatus::Fail);
    }
  }

  return BTStatus::OK;
#else
  log_w("Client: connect(BLEPhy) not supported (BLE 5.0 unavailable)");
  return BTStatus::NotSupported;
#endif
}

// Device + PHY overload is not supported on this build; always returns NotSupported.
BTStatus BLEClient::connect(const BLEAdvertisedDevice & /*device*/, BLEPhy /*phy*/, uint32_t /*timeoutMs*/) {
  log_w("Client: connect(device, BLEPhy) not supported (BLE 5.0 unavailable)");
  return BTStatus::NotSupported;
}

// BLE 5 extended connect; returns NotSupported when BLE5_SUPPORTED is off.
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

  struct ble_gap_conn_params extParams[3] = {};
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

BTStatus BLEClient::cancelConnect() {
  int rc = ble_gap_conn_cancel();
  if (rc != 0) {
    log_e("cancelConnect: ble_gap_conn_cancel rc=%d", rc);
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

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

  BLELockGuard lock(_impl->mtx);
  uint16_t handle = _impl->connHandle;
  for (auto &entry : _impl->discoveredServices) {
    if (entry.uuid == uuid) {
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

uint16_t BLEClient::getHandle() const {
  if (!_impl) {
    return BLE_HS_CONN_HANDLE_NONE;
  }
  BLELockGuard lock(_impl->mtx);
  return _impl->connHandle;
}

BLEConnInfo BLEClient::getConnInfo() const {
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

// Parameters are validated locally against the BT Core Spec ranges (v5.x, Vol 6, Part B,
// §4.5.1) and rejected before being sent to the controller.
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

// Returns NotSupported when BLE5_SUPPORTED is off.
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

// Returns NotSupported when BLE5_SUPPORTED is off.
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
  txPhy = BLEPhy::PHY_1M;
  rxPhy = BLEPhy::PHY_1M;
  log_w("Client: getPhy not supported (BLE 5.0 unavailable)");
  return BTStatus::NotSupported;
#endif
}

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
        impl->dispatchConnectFail(event->connect.status);
        impl->nimbleRef.reset();
        return 0;
      }

      {
        BLELockGuard lock(impl->mtx);
        impl->connHandle = event->connect.conn_handle;
        impl->connected = true;
        impl->identityDispatched = false;
      }
      log_i("Client: connected, handle=%u", event->connect.conn_handle);

      // Deliver onConnect before unblocking the blocking connect() call, so the
      // callback has run by the time connect() returns (ordering contract in
      // DESIGN.md).
      struct ble_gap_conn_desc desc;
      if (ble_gap_conn_find(event->connect.conn_handle, &desc) == 0) {
        BLEConnInfo connInfo = BLEConnInfoImpl::fromDesc(desc);
        impl->dispatchConnect(connInfo);
      }
      impl->connectSync.give(BTStatus::OK);
      return 0;
    }

    case BLE_GAP_EVENT_DISCONNECT:
    {
      uint16_t evtHandle = event->disconnect.conn.conn_handle;
      bool wasConnected;
      {
        BLELockGuard lock(impl->mtx);
        if (evtHandle != impl->connHandle) {
          return 0;
        }
        wasConnected = impl->connected;
        impl->connected = false;
        impl->connHandle = BLE_HS_CONN_HANDLE_NONE;
      }

      BLEConnInfo connInfo = BLEConnInfoImpl::fromDesc(event->disconnect.conn);
      uint8_t reason = event->disconnect.reason;
      log_i("Client: disconnected, handle=%u reason=0x%02x", evtHandle, reason);

      // Only surface onDisconnect if the link had actually reached connected;
      // suppresses a spurious event when a failed connect attempt tears down.
      if (wasConnected) {
        impl->dispatchDisconnect(connInfo, reason);
      }
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
      impl->dispatchMtuChanged(connInfo, event->mtu.value);
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
      bool alreadyDispatched;
      {
        BLELockGuard lock(impl->mtx);
        if (connHandle != impl->connHandle) {
          return 0;
        }
        // Latch so onIdentity fires at most once per connection even though both
        // IDENTITY_RESOLVED and ENC_CHANGE can arrive for the same pairing.
        alreadyDispatched = impl->identityDispatched;
        impl->identityDispatched = true;
      }

      struct ble_gap_conn_desc desc;
      int rc = ble_gap_conn_find(connHandle, &desc);
      if (rc != 0) {
        return 0;
      }

      BLEConnInfo connInfo = BLEConnInfoImpl::fromDesc(desc);

      if (!alreadyDispatched) {
        impl->dispatchIdentityResolved(connInfo);
      }

#if BLE_SMP_SUPPORTED
      if (event->type == BLE_GAP_EVENT_ENC_CHANGE) {
        auto *sec = BLESecurity::Impl::instance();
        if (sec) {
          sec->notifyAuthComplete(connInfo, event->enc_change.status == 0);
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
      auto *sec = BLESecurity::Impl::instance();
      if (!sec) {
        return 0;
      }

      if (event->passkey.params.action == BLE_SM_IOACT_DISP) {
        struct ble_sm_io pkey = {};
        pkey.action = BLE_SM_IOACT_DISP;
        pkey.passkey = sec->resolvePasskeyForDisplay(connInfo);
        ble_sm_inject_io(event->passkey.conn_handle, &pkey);
      } else if (event->passkey.params.action == BLE_SM_IOACT_INPUT) {
        struct ble_sm_io pkey = {};
        pkey.action = BLE_SM_IOACT_INPUT;
        pkey.passkey = sec->resolvePasskeyForInput(connInfo);
        ble_sm_inject_io(event->passkey.conn_handle, &pkey);
      } else if (event->passkey.params.action == BLE_SM_IOACT_NUMCMP) {
        struct ble_sm_io pkey = {};
        pkey.action = BLE_SM_IOACT_NUMCMP;
        pkey.numcmp_accept = sec->resolveNumericComparison(connInfo, event->passkey.params.numcmp) ? 1 : 0;
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
    entry.uuid = nimbleUuidToPublic(service->uuid);
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
 * @param arg Opaque user argument (client impl pointer used to signal mtuSync).
 * @return @c 0.
 */
int BLEClient::Impl::mtuExchangeCb(uint16_t connHandle, const struct ble_gatt_error *error, uint16_t mtu, void *arg) {
  if (error->status == 0) {
    log_i("MTU exchange complete: %d", mtu);
  } else {
    log_w("MTU exchange failed, status=%d", error->status);
  }
  // Unblock connect() (if it is waiting on the initial exchange). setMTU() fires
  // the same procedure without waiting; the stale signal is drained by the next take().
  if (arg) {
    static_cast<Impl *>(arg)->mtuSync.give(error->status == 0 ? BTStatus::OK : BTStatus::Fail);
  }
  return 0;
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

int BLEClient::Impl::gapEventHandler(struct ble_gap_event *, void *) {
  return 0;
}

#endif /* BLE_GATT_CLIENT_SUPPORTED */

#endif /* BLE_NIMBLE */
