/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 * Copyright 2017 Neil Kolban
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
#if BLE_BLUEDROID

#if BLE_GATT_CLIENT_SUPPORTED

#include "BLE.h"
#include "BLEAdvertisedDevice.h"

#include "BluedroidClient.h"
#include "BluedroidRemoteTypes.h"
#include "BluedroidUUID.h"
#include "impl/BLEImplHelpers.h"
#include "impl/BLEConnInfoData.h"
#include "esp32-hal-log.h"

#include <esp_gap_ble_api.h>
#include <esp_gattc_api.h>
#include <esp_gatt_common_api.h>
#include <string.h>

/**
 * @file BluedroidClient.cpp
 * @brief Bluedroid implementation of the Arduino `BLEClient`: GATTC app registration,
 *        GAP/GATT event handling, synchronous connect and discovery, and related APIs.
 */

// --------------------------------------------------------------------------
// BLEConnInfoImpl -- Bluedroid bridge (client-side)
// --------------------------------------------------------------------------

struct BLEConnInfoImpl {
  /**
   * @brief Build a `BLEConnInfo` snapshot for a Bluedroid GATTC link (central role).
   * @param connId GATTC connection identifier.
   * @param bda Six-byte little-endian peer Bluetooth device address.
   * @param mtu Negotiated ATT MTU (default 23 until exchanged).
   * @return A valid `BLEConnInfo` describing the link.
   */
  static BLEConnInfo make(uint16_t connId, const uint8_t bda[6], uint16_t mtu = 23) {
    BLEConnInfo info;
    info._valid = true;
    auto *d = info.data();
    d->handle = connId;
    d->address = BTAddress(bda, BTAddress::Type::Public);
    d->mtu = mtu;
    d->central = true;  // Client is central
    d->encrypted = false;
    d->authenticated = false;
    d->bonded = false;
    d->keySize = 0;
    d->interval = 0;
    d->latency = 0;
    d->supervisionTimeout = 0;
    d->txPhy = 1;
    d->rxPhy = 1;
    d->rssi = 0;
    return info;
  }

  /**
   * @brief Overwrite the MTU field on an existing `BLEConnInfo` if it is valid.
   * @param info Connection info object; no-op if invalid.
   * @param mtu New MTU value to store.
   */
  static void setMTU(BLEConnInfo &info, uint16_t mtu) {
    if (info) {
      info.data()->mtu = mtu;
    }
  }
};

// --------------------------------------------------------------------------
// Static data
// --------------------------------------------------------------------------

uint16_t BLEClient::Impl::s_nextAppId = 0x10;  // Start from 0x10 to avoid collision with server
std::vector<BLEClient::Impl *> BLEClient::Impl::s_clients;

/**
 * @brief Tear down the client: remove from the global list, unregister GATTC, delete mutex.
 * @note GATT unregistration and mutex deletion run here; not safe to use after return.
 */
BLEClient::Impl::~Impl() {
  // Remove from static list
  for (auto it = s_clients.begin(); it != s_clients.end(); ++it) {
    if (*it == this) {
      s_clients.erase(it);
      break;
    }
  }
  // Unregister GATTC app if registered
  if (gattcIf != ESP_GATT_IF_NONE) {
    esp_ble_gattc_app_unregister(gattcIf);
    gattcIf = ESP_GATT_IF_NONE;
  }
  if (mtx) {
    vSemaphoreDelete(mtx);
  }
}

// --------------------------------------------------------------------------
// Callback dispatch helpers
// --------------------------------------------------------------------------

/**
 * @brief Copy the on-connect callback under the client mutex and invoke it on the BLE thread.
 * @param impl Backing implementation pointer (must be valid for this client).
 * @param conn Connection information passed to the user callback.
 * @note Called from Bluedroid GATTC; holds lock only to copy the handler pointer.
 */
static void dispatchConnect(BLEClient::Impl *impl, const BLEConnInfo &conn) {
  BLEClient::ConnectHandler connectCb;
  {
    BLELockGuard lock(impl->mtx);
    connectCb = impl->onConnectCb;
  }
  BLEClient handle = BLEClient::Impl::makeHandle(impl);
  if (connectCb) {
    connectCb(handle, conn);
  }
}

/**
 * @brief Copy the on-disconnect callback and invoke it with the disconnect reason.
 * @param impl Backing implementation pointer.
 * @param conn Last known connection info for the event.
 * @param reason Bluedroid disconnect reason code.
 */
static void dispatchDisconnect(BLEClient::Impl *impl, const BLEConnInfo &conn, uint8_t reason) {
  BLEClient::DisconnectHandler disconnectCb;
  {
    BLELockGuard lock(impl->mtx);
    disconnectCb = impl->onDisconnectCb;
  }
  BLEClient handle = BLEClient::Impl::makeHandle(impl);
  if (disconnectCb) {
    disconnectCb(handle, conn, reason);
  }
}

/**
 * @brief Copy the connect-failure callback and invoke it when `OPEN_EVT` reports an error.
 * @param impl Backing implementation pointer.
 * @param reason GATT/Bluedroid status or error code from the stack.
 */
static void dispatchConnectFail(BLEClient::Impl *impl, int reason) {
  BLEClient::ConnectFailHandler failCb;
  {
    BLELockGuard lock(impl->mtx);
    failCb = impl->onConnectFailCb;
  }
  BLEClient handle = BLEClient::Impl::makeHandle(impl);
  if (failCb) {
    failCb(handle, reason);
  }
}

/**
 * @brief Copy the MTU-changed callback and invoke it after a successful `CFG_MTU` event.
 * @param impl Backing implementation pointer.
 * @param conn Connection info (includes updated MTU in stack state).
 * @param mtu New negotiated MTU value.
 */
static void dispatchMtuChanged(BLEClient::Impl *impl, const BLEConnInfo &conn, uint16_t mtu) {
  BLEClient::MtuChangedHandler mtuCb;
  {
    BLELockGuard lock(impl->mtx);
    mtuCb = impl->onMtuChangedCb;
  }
  BLEClient handle = BLEClient::Impl::makeHandle(impl);
  if (mtuCb) {
    mtuCb(handle, conn, mtu);
  }
}

// --------------------------------------------------------------------------
// createClient
// --------------------------------------------------------------------------

/**
 * @brief Allocate a Bluedroid GATTC client, register it with the stack, and return a public handle.
 * @return A valid `BLEClient` handle on success, or an empty client if BLE is not initialized,
 *         registration fails, or the registration wait times out.
 * @note Registration is asynchronous; this function **blocks** up to 5 s for `REG_EVT` via a sync
 *       object. Fails and removes the impl from the global list on any error.
 */
BLEClient BLEClass::createClient() {
  if (!isInitialized()) {
    log_e("createClient: BLE not initialized");
    return BLEClient();
  }

  auto impl = std::make_shared<BLEClient::Impl>();
  impl->appId = BLEClient::Impl::s_nextAppId++;
  BLEClient::Impl::s_clients.push_back(impl.get());

  // Register GATTC application
  impl->regSync.take();
  esp_err_t err = esp_ble_gattc_app_register(impl->appId);
  if (err != ESP_OK) {
    log_e("esp_ble_gattc_app_register: %s", esp_err_to_name(err));
    // Remove from s_clients
    auto &clients = BLEClient::Impl::s_clients;
    for (auto it = clients.begin(); it != clients.end(); ++it) {
      if (*it == impl.get()) {
        clients.erase(it);
        break;
      }
    }
    return BLEClient();
  }

  BTStatus st = impl->regSync.wait(5000);
  if (!st) {
    log_e("GATTC app registration failed");
    auto &clients = BLEClient::Impl::s_clients;
    for (auto it = clients.begin(); it != clients.end(); ++it) {
      if (*it == impl.get()) {
        clients.erase(it);
        break;
      }
    }
    return BLEClient();
  }

  log_i("GATTC registered (gattc_if=%u, appId=%u)", impl->gattcIf, impl->appId);
  return BLEClient(impl);
}

// --------------------------------------------------------------------------
// connect / disconnect
// --------------------------------------------------------------------------

/**
 * @brief Connect to a peripheral by address (1M PHY) with a bounded wait.
 * @param address Target device address.
 * @param timeoutMs Upper bound in milliseconds to wait for connection completion.
 * @return `BTStatus::OK` on success, or a failure/timeout/invalid state code from the connect path.
 */
BTStatus BLEClient::connect(const BTAddress &address, uint32_t timeoutMs) {
  return connect(address, BLEPhy::PHY_1M, timeoutMs);
}

/**
 * @brief Connect using the address from a scanned `BLEAdvertisedDevice` (1M PHY, bounded wait).
 * @param device Advertised device providing the peer address and type.
 * @param timeoutMs Upper bound in milliseconds to wait for connection completion.
 * @return Result of the underlying `connect` call to that address.
 */
BTStatus BLEClient::connect(const BLEAdvertisedDevice &device, uint32_t timeoutMs) {
  return connect(device.getAddress(), BLEPhy::PHY_1M, timeoutMs);
}

/**
 * @brief Synchronously open a GATTC link to a peripheral and wait for completion or timeout.
 * @param address Target device address (type used for GATTC `open` call).
 * @param phy Requested PHY (Bluedroid path may not apply all PHY options; value passed for API parity).
 * @param timeoutMs Maximum time in milliseconds to wait for `OPEN_EVT` after a successful `gattc_open`.
 * @return `BTStatus::OK` when connected, or error on duplicate connect, unregistered GATTC, GAP
 *         disconnect on timeout, or `esp_ble_gattc_open` failure.
 * @note **Blocking**: waits on an internal sync for up to @p timeoutMs. GATT operations are
 *        asynchronous; completion is signaled from `handleGATTC` on `OPEN_EVT` or `DISCONNECT_EVT`.
 *        `phy` is currently unused in the Bluedroid `open` request (1M used by the stack as applicable).
 */
BTStatus BLEClient::connect(const BTAddress &address, BLEPhy /*phy*/, uint32_t timeoutMs) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);

  if (impl.connected) {
    log_e("Client: already connected");
    return BTStatus::AlreadyConnected;
  }

  if (impl.gattcIf == ESP_GATT_IF_NONE) {
    log_e("Client: GATTC not registered");
    return BTStatus::InvalidState;
  }

  log_d("Client: connecting to %s (timeout=%u ms)", address.toString().c_str(), timeoutMs);
  impl.peerAddress = address;

  esp_bd_addr_t bda;
  memcpy(bda, address.data(), 6);

  impl.connectSync.take();
  esp_err_t err = esp_ble_gattc_open(
    impl.gattcIf, bda, static_cast<esp_ble_addr_type_t>(address.type()),
    true  // direct connection
  );

  if (err != ESP_OK) {
    log_e("esp_ble_gattc_open: %s", esp_err_to_name(err));
    impl.connectSync.give(BTStatus::Fail);
    return BTStatus::Fail;
  }

  BTStatus st = impl.connectSync.wait(timeoutMs);
  if (!st) {
    log_e("Connection failed or timed out: %d", static_cast<int>(st.code()));
    // Cancel pending connection
    esp_ble_gap_disconnect(bda);
    return st;
  }

  return BTStatus::OK;
}

/**
 * @brief Connect to a device discovered during scanning, using a specific PHY and timeout.
 * @param device Advertised device to connect to.
 * @param phy PHY selection for API parity (Bluedroid `open` path may not honor all values).
 * @param timeoutMs Maximum time in milliseconds to wait for the connection.
 * @return Result of `connect(device.getAddress(), phy, timeoutMs)`.
 */
BTStatus BLEClient::connect(const BLEAdvertisedDevice &device, BLEPhy phy, uint32_t timeoutMs) {
  return connect(device.getAddress(), phy, timeoutMs);
}

/**
 * @brief Start a connection without waiting for `OPEN_EVT` (non-blocking from the caller's perspective).
 * @param address Target device address.
 * @param phy PHY selection for API parity; not applied to the Bluedroid `gattc_open` call in this file.
 * @return `BTStatus::OK` if `esp_ble_gattc_open` was accepted, or an error if already connected, GATTC
 *         is not registered, or the open call failed.
 * @note Connection result is delivered **asynchronously** via GATTC events and user callbacks; use
 *       `connect` with a timeout if you need to block until the link is up.
 */
BTStatus BLEClient::connectAsync(const BTAddress &address, BLEPhy /*phy*/) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);

  if (impl.connected) {
    log_w("Client: connectAsync - already connected");
    return BTStatus::AlreadyConnected;
  }
  if (impl.gattcIf == ESP_GATT_IF_NONE) {
    log_e("Client: connectAsync - GATTC not registered");
    return BTStatus::InvalidState;
  }

  impl.peerAddress = address;
  esp_bd_addr_t bda;
  memcpy(bda, address.data(), 6);

  esp_err_t err = esp_ble_gattc_open(impl.gattcIf, bda, static_cast<esp_ble_addr_type_t>(address.type()), true);
  if (err != ESP_OK) {
    log_e("Client: esp_ble_gattc_open: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

/**
 * @brief `connectAsync` using the address from a `BLEAdvertisedDevice`.
 * @param device Source of the peer address and type.
 * @param phy PHY argument forwarded to `connectAsync`.
 * @return Result of `connectAsync(device.getAddress(), phy)`.
 */
BTStatus BLEClient::connectAsync(const BLEAdvertisedDevice &device, BLEPhy phy) {
  return connectAsync(device.getAddress(), phy);
}

/**
 * @brief Abort an in-progress connection by issuing GAP disconnect to the last peer address.
 * @return `BTStatus::OK` if the disconnect request was sent, `AlreadyConnected` if already linked,
 *         or `Fail` on `esp_ble_gap_disconnect` error.
 * @note If the client is already connected, this does **not** tear down the active link; it is
 *      intended to cancel a pending `gattc_open` when possible.
 */
BTStatus BLEClient::cancelConnect() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (impl.connected) {
    log_w("Client: cancelConnect - already connected, not canceling");
    return BTStatus::AlreadyConnected;
  }

  esp_bd_addr_t bda;
  memcpy(bda, impl.peerAddress.data(), 6);
  esp_err_t err = esp_ble_gap_disconnect(bda);
  if (err != ESP_OK) {
    log_e("Client: cancelConnect esp_ble_gap_disconnect: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

/**
 * @brief Request closure of the active GATTC connection for this client.
 * @return `BTStatus::OK` on success, `NotConnected` if no link, or `Fail` if `gattc_close` fails.
 * @note The actual disconnection and cleanup propagate **asynchronously** through GATTC events
 *        (`DISCONNECT_EVT` clears state and may invoke user disconnect callbacks).
 */
BTStatus BLEClient::disconnect() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);

  if (!impl.connected) {
    log_w("Client: disconnect called but not connected");
    return BTStatus::NotConnected;
  }

  esp_err_t err = esp_ble_gattc_close(impl.gattcIf, impl.connId);
  if (err != ESP_OK) {
    log_e("esp_ble_gattc_close: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

// --------------------------------------------------------------------------
// Service discovery
// --------------------------------------------------------------------------

/**
 * @brief Request primary service search on the current connection and block until it completes.
 * @return `BTStatus::OK` with filled `discoveredServices` on success, or `NotConnected` / `Fail` /
 *         timeout failure.
 * @note **Blocking** up to 10 s on internal `discoverSync` waiting for `SEARCH_CMPL_EVT`. Clears
 *        `discoveredServices` before the search. On failure, returns the sync status.
 */
BTStatus BLEClient::discoverServices() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (!impl.connected) {
    log_w("Client: discoverServices called but not connected");
    return BTStatus::NotConnected;
  }

  {
    BLELockGuard lock(impl.mtx);
    impl.discoveredServices.clear();
  }

  impl.discoverSync.take();
  esp_err_t err = esp_ble_gattc_search_service(impl.gattcIf, impl.connId, NULL);
  if (err != ESP_OK) {
    log_e("esp_ble_gattc_search_service: %s", esp_err_to_name(err));
    impl.discoverSync.give(BTStatus::Fail);
    return BTStatus::Fail;
  }

  BTStatus st = impl.discoverSync.wait(10000);
  if (!st) {
    log_e("Service discovery failed or timed out");
    return st;
  }

  log_i("Discovered %u services", (unsigned)impl.discoveredServices.size());
  return BTStatus::OK;
}

/**
 * @brief Return a `BLERemoteService` for the given UUID, running discovery on demand.
 * @param uuid 128- or 16-bit service UUID to look up in the last discovery pass.
 * @return A non-empty handle if found; otherwise an empty `BLERemoteService` after a failed/empty
 *         `discoverServices` or when the UUID is absent.
 * @note If services were not yet discovered and the client is connected, calls `discoverServices()`
 *       (blocking) once before matching.
 */
BLERemoteService BLEClient::getService(const BLEUUID &uuid) {
  BLE_CHECK_IMPL(BLERemoteService());

  // Auto-discover if not done
  if (impl.discoveredServices.empty() && impl.connected) {
    if (!discoverServices()) {
      return BLERemoteService();
    }
  }

  BLELockGuard lock(impl.mtx);
  for (auto &svc : impl.discoveredServices) {
    log_d("getService: comparing %s == %s", svc->uuid.toString().c_str(), uuid.toString().c_str());
    if (svc->uuid == uuid) {
      return BLERemoteService(std::shared_ptr<BLERemoteService::Impl>(svc));
    }
  }
  log_w("getService: UUID %s not found among %u discovered service(s)", uuid.toString().c_str(), (unsigned)impl.discoveredServices.size());
  return BLERemoteService();
}

/**
 * @brief Return a copy of all services discovered (or known) for this connection.
 * @return A vector of `BLERemoteService` objects; empty if not connected or on early exit.
 * @note Const overload triggers lazy discovery: if connected and the service list is empty, calls
 *       the non-const `discoverServices()` **synchronously** (up to 10 s) to populate the list.
 */
std::vector<BLERemoteService> BLEClient::getServices() const {
  std::vector<BLERemoteService> result;
  if (!_impl) {
    return result;
  }

  // Lazy discovery: mirror the const_cast pattern already used by
  // BLERemoteService::getCharacteristics() / BLERemoteCharacteristic::getDescriptors()
  // so a user calling only getServices() still gets back the actual remote services.
  if (_impl->connected) {
    bool empty;
    {
      BLELockGuard lock(_impl->mtx);
      empty = _impl->discoveredServices.empty();
    }
    if (empty) {
      auto *self = const_cast<BLEClient *>(this);
      (void)self->discoverServices();
    }
  }

  BLELockGuard lock(_impl->mtx);
  for (auto &svc : _impl->discoveredServices) {
    result.push_back(BLERemoteService(std::shared_ptr<BLERemoteService::Impl>(svc)));
  }
  return result;
}

// --------------------------------------------------------------------------
// Security
// --------------------------------------------------------------------------

/**
 * @brief Request an encrypted (MITM-capable) link using SMP with the current peer.
 * @return `BTStatus::OK` on success, `NotConnected` or `NotSupported` (SMP off), or `Fail` on stack error.
 * @note The encryption upgrade completes **asynchronously** in the stack; user code may observe
 *        bonding state via GAP and connection info updates elsewhere, not in this return value alone.
 */
BTStatus BLEClient::secureConnection() {
#if BLE_SMP_SUPPORTED
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (!impl.connected) {
    log_w("Client: secureConnection called but not connected");
    return BTStatus::NotConnected;
  }

  esp_bd_addr_t bda;
  memcpy(bda, impl.peerAddress.data(), 6);
  esp_err_t err = esp_ble_set_encryption(bda, ESP_BLE_SEC_ENCRYPT_MITM);
  if (err != ESP_OK) {
    log_e("Client: esp_ble_set_encryption: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }
  return BTStatus::OK;
#else
  log_w("SMP not supported");
  return BTStatus::NotSupported;
#endif
}

// --------------------------------------------------------------------------
// MTU
// --------------------------------------------------------------------------

/**
 * @brief Set local GATT MTU and, if connected, request a peer MTU exchange.
 * @param mtu Desired ATT MTU (subject to Bluedroid limits).
 * @note When a link is active, sends `esp_ble_gattc_send_mtu_req` and may **block** up to 3 s
 *        waiting for `CFG_MTU_EVT` on the internal `mtuSync` object.
 */
void BLEClient::setMTU(uint16_t mtu) {
  if (!_impl) {
    return;
  }

  esp_ble_gatt_set_local_mtu(mtu);

  if (_impl->connected) {
    _impl->mtuSync.take();
    esp_err_t err = esp_ble_gattc_send_mtu_req(_impl->gattcIf, _impl->connId);
    if (err == ESP_OK) {
      _impl->mtuSync.wait(3000);
    } else {
      _impl->mtuSync.give(BTStatus::Fail);
    }
  }
}

/**
 * @brief Return the last negotiated (or default) MTU for this client.
 * @return Current MTU from the implementation, or 23 if the client is null.
 */
uint16_t BLEClient::getMTU() const {
  return _impl ? _impl->mtu : 23;
}

// --------------------------------------------------------------------------
// RSSI
// --------------------------------------------------------------------------

/**
 * @brief Read the RSSI of the connected peer via GAP.
 * @return Measured dBm on success, or -128 if not connected, on read error, or after a 3 s wait timeout.
 * @note **Blocking** up to 3 s: posts `esp_ble_gap_read_rssi` and waits for `READ_RSSI_COMPLETE_EVT` on
 *        `rssiSync`. RSSI is delivered in `handleGAP`.
 */
int8_t BLEClient::getRSSI() const {
  if (!_impl || !_impl->connected) {
    return -128;
  }

  esp_bd_addr_t bda;
  memcpy(bda, _impl->peerAddress.data(), 6);

  _impl->rssiSync.take();
  esp_err_t err = esp_ble_gap_read_rssi(bda);
  if (err != ESP_OK) {
    _impl->rssiSync.give(BTStatus::Fail);
    log_w("Client: getRSSI esp_ble_gap_read_rssi: %s", esp_err_to_name(err));
    return -128;
  }

  BTStatus st = _impl->rssiSync.wait(3000);
  if (!st) {
    log_w("Client: getRSSI timed out");
    return -128;
  }
  return _impl->lastRssi;
}

// --------------------------------------------------------------------------
// Connection info
// --------------------------------------------------------------------------

/**
 * @brief GATTC connection id used as a handle in this Bluedroid implementation.
 * @return The active `connId`, or 0xFFFF if the public `BLEClient` has no implementation.
 */
uint16_t BLEClient::getHandle() const {
  return _impl ? _impl->connId : 0xFFFF;
}

/**
 * @brief Snapshot the current link as a `BLEConnInfo` (invalid if not connected).
 * @return Populated `BLEConnInfo` from `BLEConnInfoImpl::make` when connected; otherwise an invalid
 *         default-constructed `BLEConnInfo`.
 */
BLEConnInfo BLEClient::getConnection() const {
  if (!_impl || !_impl->connected) {
    return BLEConnInfo();
  }
  return BLEConnInfoImpl::make(_impl->connId, _impl->peerAddress.data(), _impl->mtu);
}

// --------------------------------------------------------------------------
// Connection parameters
// --------------------------------------------------------------------------

/**
 * @brief Propose new connection interval, latency, and supervision timeout to the link layer.
 * @param params `minInterval`/`maxInterval` in 1.25 ms units, `latency` events, and `timeout` in 10 ms
 *               units as in `esp_ble_conn_update_params_t`.
 * @return `OK` on successful submission to GAP, or `NotConnected` / `InvalidParam` / `Fail`.
 * @note The update is **asynchronous**; LL acceptance is reported via the stack, not in this return.
 *       Connection parameter constraints per BT Core Spec v5.x, Vol 6, Part B, §4.5.1:
 *       interval 6–3200 (1.25 ms units), latency 0–499, timeout 10–3200 (10 ms units),
 *       timeout > (1 + latency) × maxInterval × 2.
 */
BTStatus BLEClient::updateConnParams(const BLEConnParams &params) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (!impl.connected) {
    log_w("Client: updateConnParams called but not connected");
    return BTStatus::NotConnected;
  }

  if (!params.isValid()) {
    log_e("Client: updateConnParams — parameters out of spec");
    return BTStatus::InvalidParam;
  }

  esp_ble_conn_update_params_t cp;
  memcpy(cp.bda, impl.peerAddress.data(), 6);
  cp.min_int = params.minInterval;
  cp.max_int = params.maxInterval;
  cp.latency = params.latency;
  cp.timeout = params.timeout;
  esp_err_t err = esp_ble_gap_update_conn_params(&cp);
  if (err != ESP_OK) {
    log_e("Client: esp_ble_gap_update_conn_params: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

// --------------------------------------------------------------------------
// PHY (BLE 5.0 -- limited support on Bluedroid)
// --------------------------------------------------------------------------

/**
 * @brief PHY override hook (not implemented for Bluedroid in this file).
 * @param txPhy Ignored; API parity.
 * @param rxPhy Ignored; API parity.
 * @return Always `BTStatus::NotSupported` with a warning log.
 */
BTStatus BLEClient::setPhy(BLEPhy /*txPhy*/, BLEPhy /*rxPhy*/) {
  log_w("%s not supported on Bluedroid", __func__);
  return BTStatus::NotSupported;
}

/**
 * @brief Report the assumed RX/TX PHYs for a Bluedroid build without extended PHY control.
 * @param txPhy Filled with `BLEPhy::PHY_1M`.
 * @param rxPhy Filled with `BLEPhy::PHY_1M`.
 * @return `BTStatus::OK` to indicate a fixed default 1M assumption.
 * @note Does not read controller state; this is a stub to satisfy the API.
 */
BTStatus BLEClient::getPhy(BLEPhy &txPhy, BLEPhy &rxPhy) const {
  txPhy = BLEPhy::PHY_1M;
  rxPhy = BLEPhy::PHY_1M;
  return BTStatus::OK;
}

/**
 * @brief L2CAP data length extension (not supported on this Bluedroid path).
 * @param txOctets Ignored.
 * @param txTime Ignored.
 * @return `BTStatus::NotSupported` with a warning log.
 */
BTStatus BLEClient::setDataLen(uint16_t /*txOctets*/, uint16_t /*txTime*/) {
  log_w("%s not supported on Bluedroid", __func__);
  return BTStatus::NotSupported;
}

// --------------------------------------------------------------------------
// handleGATTC -- static event handler
// --------------------------------------------------------------------------

/**
 * @brief Central GATTC callback: registration, open/close, discovery, read/write, notify, MTU.
 * @param event Bluedroid GATTC event id.
 * @param gattc_if Interface handle associated with a registered GATTC app, or the one in the event.
 * @param param Event-specific parameters from the Bluedroid stack; must be valid for @p event.
 * @note Runs on the **Bluedroid task**; unblocks `regSync`/`connectSync`/`discoverSync`/`readSync`/
 *       `writeSync`/`mtuSync` as events arrive. Drives `dispatchConnect`/`dispatchDisconnect`/
 *       `dispatchMtuChanged` and characteristic notify callbacks. Uses `s_clients` to find the
 *       matching `Impl` (by `app_id` on `REG_EVT`, by `gattc_if` otherwise, with a BDA fallback for
 *       `CONNECT_EVT`).
 */
void BLEClient::Impl::handleGATTC(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) {
  // REG_EVT: match by app_id
  if (event == ESP_GATTC_REG_EVT) {
    for (auto *c : s_clients) {
      if (param->reg.app_id == c->appId) {
        if (param->reg.status == ESP_GATT_OK) {
          c->gattcIf = gattc_if;
          c->regSync.give(BTStatus::OK);
        } else {
          log_e("GATTC REG failed: status=%d", param->reg.status);
          c->regSync.give(BTStatus::Fail);
        }
        return;
      }
    }
    return;
  }

  // Find the client instance by gattc_if
  Impl *client = nullptr;
  for (auto *c : s_clients) {
    if (c->gattcIf == gattc_if) {
      client = c;
      break;
    }
  }

  // For CONNECT_EVT, also try matching by BDA if gattc_if didn't match
  if (!client && event == ESP_GATTC_CONNECT_EVT) {
    for (auto *c : s_clients) {
      if (memcmp(c->peerAddress.data(), param->connect.remote_bda, 6) == 0) {
        client = c;
        break;
      }
    }
  }

  if (!client) {
    return;
  }

  switch (event) {
    case ESP_GATTC_CONNECT_EVT:
    {
      client->connId = param->connect.conn_id;
      client->connected = true;
      // Request MTU exchange after connection
      esp_ble_gattc_send_mtu_req(gattc_if, param->connect.conn_id);
      break;
    }

    case ESP_GATTC_OPEN_EVT:
    {
      if (param->open.status == ESP_GATT_OK) {
        client->connId = param->open.conn_id;
        client->connected = true;
        log_i("Client: connected, connId=%u", client->connId);
        BLEConnInfo conn = BLEConnInfoImpl::make(client->connId, param->open.remote_bda, client->mtu);
        dispatchConnect(client, conn);
        client->connectSync.give(BTStatus::OK);
      } else {
        log_e("Client: GATTC open failed: status=%d", param->open.status);
        dispatchConnectFail(client, param->open.status);
        client->connectSync.give(BTStatus::Fail);
      }
      break;
    }

    case ESP_GATTC_DISCONNECT_EVT:
    {
      if (param->disconnect.conn_id != client->connId) {
        break;
      }

      bool wasConnected = client->connected;
      client->connected = false;
      uint8_t reason = param->disconnect.reason;
      log_i("Client: disconnected, connId=%u reason=0x%02x", client->connId, reason);

      // Release any waiting syncs
      client->connectSync.give(BTStatus::Fail);
      client->discoverSync.give(BTStatus::Fail);
      client->readSync.give(BTStatus::Fail);
      client->writeSync.give(BTStatus::Fail);
      client->mtuSync.give(BTStatus::Fail);
      client->rssiSync.give(BTStatus::Fail);

      if (wasConnected) {
        BLEConnInfo conn = BLEConnInfoImpl::make(client->connId, param->disconnect.remote_bda, client->mtu);
        dispatchDisconnect(client, conn, reason);
      }

      client->connId = 0xFFFF;
      {
        BLELockGuard lock(client->mtx);
        client->discoveredServices.clear();
      }
      break;
    }

    case ESP_GATTC_CLOSE_EVT:
    {
      break;
    }

    case ESP_GATTC_SEARCH_RES_EVT:
    {
      auto svc = std::make_shared<BLERemoteService::Impl>();
      svc->uuid = espToUuid(param->search_res.srvc_id.uuid);
      svc->startHandle = param->search_res.start_handle;
      svc->endHandle = param->search_res.end_handle;
      svc->client = client;
      BLELockGuard lock(client->mtx);
      client->discoveredServices.push_back(svc);
      break;
    }

    case ESP_GATTC_SEARCH_CMPL_EVT:
    {
      client->discoverSync.give(param->search_cmpl.status == ESP_GATT_OK ? BTStatus::OK : BTStatus::Fail);
      break;
    }

    case ESP_GATTC_READ_CHAR_EVT:
    case ESP_GATTC_READ_DESCR_EVT:
    {
      client->lastReadStatus = param->read.status;
      if (param->read.status == ESP_GATT_OK) {
        client->readBuf.assign(param->read.value, param->read.value + param->read.value_len);
      } else {
        client->readBuf.clear();
      }
      client->readSync.give(param->read.status == ESP_GATT_OK ? BTStatus::OK : BTStatus::Fail);
      break;
    }

    case ESP_GATTC_WRITE_CHAR_EVT:
    case ESP_GATTC_WRITE_DESCR_EVT:
    {
      client->lastWriteStatus = param->write.status;
      client->writeSync.give(param->write.status == ESP_GATT_OK ? BTStatus::OK : BTStatus::Fail);
      break;
    }

    case ESP_GATTC_REG_FOR_NOTIFY_EVT:
    {
      // Notification registration complete -- no sync needed
      break;
    }

    case ESP_GATTC_NOTIFY_EVT:
    {
      uint16_t handle = param->notify.handle;
      BLERemoteCharacteristic::NotifyCallback notifyCb;
      std::shared_ptr<BLERemoteCharacteristic::Impl> chrShared;
      {
        BLELockGuard lock(client->mtx);
        for (auto &svc : client->discoveredServices) {
          for (auto &chr : svc->characteristics) {
            if (chr->handle == handle && chr->notifyCb) {
              notifyCb = chr->notifyCb;
              chrShared = chr->shared_from_this();
              break;
            }
          }
          if (notifyCb) {
            break;
          }
        }
      }
      if (notifyCb) {
        BLERemoteCharacteristic chrHandle(chrShared);
        notifyCb(chrHandle, param->notify.value, param->notify.value_len, param->notify.is_notify);
      }
      break;
    }

    case ESP_GATTC_CFG_MTU_EVT:
    {
      if (param->cfg_mtu.status == ESP_GATT_OK) {
        client->mtu = param->cfg_mtu.mtu;
        log_i("Client: MTU exchanged, mtu=%u connId=%u", client->mtu, client->connId);
      } else {
        log_w("Client: MTU exchange failed, status=%d", param->cfg_mtu.status);
      }
      client->mtuSync.give(param->cfg_mtu.status == ESP_GATT_OK ? BTStatus::OK : BTStatus::Fail);

      BLEConnInfo conn = BLEConnInfoImpl::make(client->connId, client->peerAddress.data(), client->mtu);
      dispatchMtuChanged(client, conn, client->mtu);
      break;
    }

    case ESP_GATTC_SRVC_CHG_EVT:
    {
      log_i("Client: service changed indication received");
      break;
    }

    default: break;
  }
}

// --------------------------------------------------------------------------
// handleGAP -- static GAP event handler (for RSSI etc.)
// --------------------------------------------------------------------------

/**
 * @brief GAP callback used to complete an RSSI read and unblock `getRSSI`.
 * @param event GAP event; only `READ_RSSI_COMPLETE` is handled here.
 * @param param Event data, including BDA, status, and measured RSSI when successful.
 * @note On GAP read failure, all clients' `rssiSync` are signaled to avoid a stuck waiter;
 *       on success, only the matching peer is released.
 */
void BLEClient::Impl::handleGAP(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  if (event == ESP_GAP_BLE_READ_RSSI_COMPLETE_EVT) {
    if (param->read_rssi_cmpl.status == ESP_BT_STATUS_SUCCESS) {
      for (auto *c : s_clients) {
        if (memcmp(c->peerAddress.data(), param->read_rssi_cmpl.remote_addr, 6) == 0) {
          c->lastRssi = param->read_rssi_cmpl.rssi;
          c->rssiSync.give(BTStatus::OK);
          return;
        }
      }
    } else {
      // GAP RSSI failure does not include BDA — give(Fail) to all clients to
      // avoid leaving any waiter stuck.  This may unblock a client that did not
      // request the read; callers already treat Fail as -128, so the impact is
      // limited to a spurious early return.
      for (auto *c : s_clients) {
        c->rssiSync.give(BTStatus::Fail);
      }
    }
  }
}

#else /* !BLE_GATT_CLIENT_SUPPORTED -- stubs */

#include "BLE.h"
#include "impl/BLEClientBackend.h"
#include "esp32-hal-log.h"

// Stubs for BLE_GATT_CLIENT_SUPPORTED == 0: log on most entry points; return NotSupported, empty,
// 0, -128, 0xFFFF, or no-op. getPhy() does not write txPhy/rxPhy; initialize or check the return.

BLEClient BLEClass::createClient() {
  log_w("GATT client not supported");
  return BLEClient();
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

BTStatus BLEClient::discoverServices() {
  log_w("GATT client not supported");
  return BTStatus::NotSupported;
}

BLERemoteService BLEClient::getService(const BLEUUID &) {
  return BLERemoteService();
}

std::vector<BLERemoteService> BLEClient::getServices() const {
  return {};
}

BTStatus BLEClient::secureConnection() {
  log_w("GATT client not supported");
  return BTStatus::NotSupported;
}

void BLEClient::setMTU(uint16_t) {}

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

#endif /* BLE_GATT_CLIENT_SUPPORTED */

#endif /* BLE_BLUEDROID */
