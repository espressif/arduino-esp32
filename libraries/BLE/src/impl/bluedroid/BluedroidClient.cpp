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

#include "impl/common/BLEGuards.h"
#if BLE_BLUEDROID

#if BLE_GATT_CLIENT_SUPPORTED

#include "BLE.h"
#include "BLEAdvertisedDevice.h"

#include "BluedroidClient.h"
#include "BluedroidCore.h"
#include "BluedroidRemoteGatt.h"
#include "BluedroidUUID.h"
#include "impl/common/BLEImplHelpers.h"
#include "impl/common/BLEConnInfoData.h"
#include "BluedroidConnInfo.h"
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

// API contract is documented on the declarations in the public BLE*.h headers; the definitions below carry implementation notes only.

// --------------------------------------------------------------------------
// Static data
// --------------------------------------------------------------------------

static constexpr uint16_t APP_ID_INVALID = UINT16_MAX;

// Sentinel for esp_ble_gatt_creat_conn_params_t::own_addr_type. 0xFF is not a valid
// esp_ble_addr_type_t enum value (PUBLIC/RANDOM/RPA_* are 0x00–0x03); ESP-IDF documents
// it as "address type unknown" and lets Bluedroid pick the locally configured type.
// esp_ble_gattc_open() and esp_ble_gattc_aux_open() set the same value before enh_open.
// Scan/advertise pass BLE.getOwnAddressType() because GAP transmit APIs require an
// explicit identity; outbound connect does not.
static constexpr uint8_t GATTC_OWN_ADDR_TYPE_LOCAL_DEFAULT = 0xff;
uint16_t BLEClient::Impl::s_nextAppId = 0;
std::vector<BLEClient::Impl *> BLEClient::Impl::s_clients;

// --------------------------------------------------------------------------
// Impl destructor
// --------------------------------------------------------------------------

/**
 * @brief Tear down the client: remove from the global list, unregister GATTC, delete mutex.
 * @note GATT unregistration and mutex deletion run here; not safe to use after return.
 */
BLEClient::Impl::~Impl() {
  // Remove from static list
  for (auto it = s_clients.begin(); it != s_clients.end(); ++it) {
    if (*it == static_cast<BLEClient::Impl *>(this)) {
      s_clients.erase(it);
      break;
    }
  }
  // Unregister GATTC app if registered
  if (gattcIf != ESP_GATT_IF_NONE) {
    esp_ble_gattc_app_unregister(gattcIf);
    gattcIf = ESP_GATT_IF_NONE;
  }
  // Note: mtx is owned and destroyed by BLEClientImplCommon.
}

// --------------------------------------------------------------------------
// Callback dispatch helpers
// --------------------------------------------------------------------------

// dispatchConnect / dispatchDisconnect / dispatchConnectFail / dispatchMtuChanged
// are shared and live on BLEClientImplCommon (impl/common/BLEClientImpl.cpp);
// call them as client->dispatchXxx(...).

// --------------------------------------------------------------------------
// App ID allocation helper
// --------------------------------------------------------------------------

/**
 * @brief Allocate the next free GATT application ID.
 *
 * Wraps at ESP_APP_ID_MAX (0x7FFF) back to 0 and checks for collisions with
 * still-alive clients so that wrap-around is safe.
 *
 * @return A valid app ID, or APP_ID_INVALID if none are available.
 */
static uint16_t allocateAppId() {
  uint16_t candidate = BLEClient::Impl::s_nextAppId;
  uint16_t startedAt = candidate;

  while (true) {
    BLEClient::Impl::s_nextAppId = candidate + 1;
    if (BLEClient::Impl::s_nextAppId > ESP_APP_ID_MAX) {
      BLEClient::Impl::s_nextAppId = 0;
    }

    bool inUse = false;
    for (auto *c : BLEClient::Impl::s_clients) {
      if (c->appId == candidate) {
        inUse = true;
        break;
      }
    }
    if (!inUse) {
      return candidate;
    }

    candidate = BLEClient::Impl::s_nextAppId;
    if (candidate == startedAt) return APP_ID_INVALID;
  }
}

// --------------------------------------------------------------------------
// GATTC app registration (used by BLEClass::createClient in BluedroidCore.cpp)
// --------------------------------------------------------------------------

// Registration is asynchronous: blocks up to 5 s for REG_EVT via a sync object, and removes the
// impl from the global client list on any error.
bool bluedroidRegisterClient(const std::shared_ptr<BLEClient::Impl> &impl) {
  impl->appId = allocateAppId();
  if (impl->appId == APP_ID_INVALID) {
    log_e("createClient: no free app IDs");
    return false;
  }

  BLEClient::Impl::s_clients.push_back(impl.get());

  impl->regSync.take();
  esp_err_t err = esp_ble_gattc_app_register(impl->appId);
  if (err != ESP_OK) {
    log_e("esp_ble_gattc_app_register: %s", esp_err_to_name(err));
    auto &clients = BLEClient::Impl::s_clients;
    for (auto it = clients.begin(); it != clients.end(); ++it) {
      if (*it == impl.get()) {
        clients.erase(it);
        break;
      }
    }
    return false;
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
    return false;
  }

  log_i("GATTC registered (gattc_if=%u, appId=%u)", impl->gattcIf, impl->appId);
  return true;
}

// --------------------------------------------------------------------------
// connect / disconnect
// --------------------------------------------------------------------------

BTStatus BLEClient::connect(const BTAddress &address, uint32_t timeoutMs) {
  return connect(address, BLEPhy::PHY_1M, timeoutMs);
}

BTStatus BLEClient::connect(const BLEAdvertisedDevice &device, uint32_t timeoutMs) {
  return connect(device.getAddress(), BLEPhy::PHY_1M, timeoutMs);
}

// Blocking: waits on an internal sync for up to timeoutMs. GATT operations are asynchronous;
// completion is signaled from handleGATTC on OPEN_EVT or DISCONNECT_EVT.
// When BLE5 is available, prefer_ext_connect_params_set applies the requested PHY before open.
BTStatus BLEClient::connect(const BTAddress &address, BLEPhy phy, uint32_t timeoutMs) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);

  if (impl.connected) {
    log_e("Client: already connected");
    return BTStatus::AlreadyConnected;
  }

  if (impl.gattcIf == ESP_GATT_IF_NONE) {
    log_e("Client: GATTC not registered");
    return BTStatus::InvalidState;
  }

  log_d("Client: connecting to %s phy=%u (timeout=%u ms)", address.toString().c_str(), static_cast<unsigned>(phy), timeoutMs);
  impl.peerAddress = address;
#if BLE5_SUPPORTED
  impl.cachedTxPhy = 1;
  impl.cachedRxPhy = 1;
#endif

  esp_bd_addr_t bda;
  address.toEspBdAddr(bda);

#if BLE5_SUPPORTED
  {
    esp_ble_gap_conn_params_t connParams = {};
    connParams.scan_interval = 0x50;
    connParams.scan_window = 0x30;
    connParams.interval_min = 0x18;
    connParams.interval_max = 0x28;
    connParams.latency = 0;
    connParams.supervision_timeout = 0x1F4;
    connParams.min_ce_len = 0;
    connParams.max_ce_len = 0;
    esp_ble_gap_phy_mask_t phyMask = blePhyToPrefMask(phy);
    const esp_ble_gap_conn_params_t *p1m = (phyMask & ESP_BLE_GAP_PHY_1M_PREF_MASK) ? &connParams : nullptr;
    const esp_ble_gap_conn_params_t *p2m = (phyMask & ESP_BLE_GAP_PHY_2M_PREF_MASK) ? &connParams : nullptr;
    const esp_ble_gap_conn_params_t *pcoded = (phyMask & ESP_BLE_GAP_PHY_CODED_PREF_MASK) ? &connParams : nullptr;
    esp_err_t prefErr = esp_ble_gap_prefer_ext_connect_params_set(bda, phyMask, p1m, p2m, pcoded);
    if (prefErr != ESP_OK) {
      log_w("Client: prefer_ext_connect_params_set: %s (continuing with gattc_enh_open)", esp_err_to_name(prefErr));
    }
  }
#else
  (void)phy;
#endif

  // esp_ble_gattc_open is compiled only when BLE 4.2 features are enabled.
  // BLE5-only Bluedroid builds (e.g. esp32s31) expose aux_open / enh_open instead.
  // Use the unified enh_open API with is_aux matching the active feature set.
  esp_ble_gatt_creat_conn_params_t conn = {};
  memcpy(conn.remote_bda, bda, ESP_BD_ADDR_LEN);
  conn.remote_addr_type = static_cast<esp_ble_addr_type_t>(address.type());
  conn.is_direct = true;
#if BLE5_SUPPORTED
  conn.is_aux = true;
#else
  conn.is_aux = false;
#endif
  conn.own_addr_type = static_cast<esp_ble_addr_type_t>(GATTC_OWN_ADDR_TYPE_LOCAL_DEFAULT);
  conn.phy_mask = 0;

  impl.connectSync.take();
  esp_err_t err = esp_ble_gattc_enh_open(impl.gattcIf, &conn);

  if (err != ESP_OK) {
    log_e("esp_ble_gattc_enh_open: %s", esp_err_to_name(err));
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

  // Initial ATT MTU exchange, performed synchronously from the caller's thread
  // before connect() returns (as NimBLE does via ble_gattc_exchange_mtu). We
  // MUST block until CFG_MTU_EVT: the MTU exchange is an ATT procedure, and if
  // the caller issues another GATT/GAP operation (e.g. updateConnParams, a read,
  // or discovery) while it is still in flight, Bluedroid drops the pending MTU
  // procedure and no CFG_MTU_EVT is delivered. Completing it here guarantees the
  // link's MTU is settled before any user operation can race it. onConnect
  // already fired during OPEN_EVT, so onMtuChanged still lands after onConnect
  // (DESIGN.md ordering contract).
  if (impl.connected) {
    impl.mtuSync.take();
    if (esp_ble_gattc_send_mtu_req(impl.gattcIf, impl.connId) == ESP_OK) {
      impl.mtuSync.wait(2000);
    } else {
      impl.mtuSync.give(BTStatus::Fail);
    }
  }

  return BTStatus::OK;
}

BTStatus BLEClient::connect(const BLEAdvertisedDevice &device, BLEPhy phy, uint32_t timeoutMs) {
  return connect(device.getAddress(), phy, timeoutMs);
}

// Connection result is delivered asynchronously via GATTC events and user callbacks; use connect()
// with a timeout to block until the link is up. When BLE5 is available, prefer_ext_connect_params_set
// applies the requested PHY before open.
BTStatus BLEClient::connectAsync(const BTAddress &address, BLEPhy phy) {
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
#if BLE5_SUPPORTED
  impl.cachedTxPhy = 1;
  impl.cachedRxPhy = 1;
#endif
  esp_bd_addr_t bda;
  address.toEspBdAddr(bda);

#if BLE5_SUPPORTED
  {
    esp_ble_gap_conn_params_t connParams = {};
    connParams.scan_interval = 0x50;
    connParams.scan_window = 0x30;
    connParams.interval_min = 0x18;
    connParams.interval_max = 0x28;
    connParams.latency = 0;
    connParams.supervision_timeout = 0x1F4;
    esp_ble_gap_phy_mask_t phyMask = blePhyToPrefMask(phy);
    const esp_ble_gap_conn_params_t *p1m = (phyMask & ESP_BLE_GAP_PHY_1M_PREF_MASK) ? &connParams : nullptr;
    const esp_ble_gap_conn_params_t *p2m = (phyMask & ESP_BLE_GAP_PHY_2M_PREF_MASK) ? &connParams : nullptr;
    const esp_ble_gap_conn_params_t *pcoded = (phyMask & ESP_BLE_GAP_PHY_CODED_PREF_MASK) ? &connParams : nullptr;
    esp_err_t prefErr = esp_ble_gap_prefer_ext_connect_params_set(bda, phyMask, p1m, p2m, pcoded);
    if (prefErr != ESP_OK) {
      log_w("Client: prefer_ext_connect_params_set: %s (continuing with gattc_enh_open)", esp_err_to_name(prefErr));
    }
  }
#else
  (void)phy;
#endif

  esp_ble_gatt_creat_conn_params_t conn = {};
  memcpy(conn.remote_bda, bda, ESP_BD_ADDR_LEN);
  conn.remote_addr_type = static_cast<esp_ble_addr_type_t>(address.type());
  conn.is_direct = true;
#if BLE5_SUPPORTED
  conn.is_aux = true;
#else
  conn.is_aux = false;
#endif
  conn.own_addr_type = static_cast<esp_ble_addr_type_t>(GATTC_OWN_ADDR_TYPE_LOCAL_DEFAULT);
  conn.phy_mask = 0;

  esp_err_t err = esp_ble_gattc_enh_open(impl.gattcIf, &conn);
  if (err != ESP_OK) {
    log_e("Client: esp_ble_gattc_enh_open: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

BTStatus BLEClient::connectAsync(const BLEAdvertisedDevice &device, BLEPhy phy) {
  return connectAsync(device.getAddress(), phy);
}

// Cancels a pending gattc_open when possible; if already connected, does not tear down the active link.
BTStatus BLEClient::cancelConnect() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (impl.connected) {
    log_w("Client: cancelConnect - already connected, not canceling");
    return BTStatus::AlreadyConnected;
  }

  esp_bd_addr_t bda;
  impl.peerAddress.toEspBdAddr(bda);
  esp_err_t err = esp_ble_gap_disconnect(bda);
  if (err != ESP_OK) {
    log_e("Client: cancelConnect esp_ble_gap_disconnect: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

// Disconnection and cleanup propagate asynchronously through GATTC events (DISCONNECT_EVT clears
// state and may invoke user disconnect callbacks).
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

// Blocking up to 10 s on discoverSync waiting for SEARCH_CMPL_EVT. Clears discoveredServices
// before the search.
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

// If services were not yet discovered and the client is connected, calls discoverServices()
// (blocking) once before matching.
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

// Const overload triggers lazy discovery: if connected and the service list is empty, calls the
// non-const discoverServices() synchronously (up to 10 s) to populate the list.
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

// The encryption upgrade completes asynchronously in the stack; bonding state is observed via GAP
// and connection info updates elsewhere, not in this return value alone.
BTStatus BLEClient::secureConnection() {
#if BLE_SMP_SUPPORTED
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (!impl.connected) {
    log_w("Client: secureConnection called but not connected");
    return BTStatus::NotConnected;
  }

  esp_bd_addr_t bda;
  impl.peerAddress.toEspBdAddr(bda);
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

// When a link is active, sends esp_ble_gattc_send_mtu_req and may block up to 3 s waiting for
// CFG_MTU_EVT on the internal mtuSync object.
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

uint16_t BLEClient::getMTU() const {
  return _impl ? _impl->mtu.load() : 23;
}

// --------------------------------------------------------------------------
// RSSI
// --------------------------------------------------------------------------

// Blocking up to 3 s: posts esp_ble_gap_read_rssi and waits for READ_RSSI_COMPLETE_EVT on rssiSync.
// RSSI is delivered in handleGAP.
int8_t BLEClient::getRSSI() const {
  if (!_impl || !_impl->connected) {
    return -128;
  }

  esp_bd_addr_t bda;
  _impl->peerAddress.toEspBdAddr(bda);

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

uint16_t BLEClient::getHandle() const {
  return _impl ? _impl->connId.load() : 0xFFFF;
}

BLEConnInfo BLEClient::getConnInfo() const {
  if (!_impl || !_impl->connected) {
    return BLEConnInfo();
  }
  BLEConnInfo conn = BLEConnInfoImpl::make(
    _impl->connId, _impl->peerAddress.data(), _impl->mtu, /*central=*/true, _impl->peerAddress.type()
  );
  // Bluedroid has no live security-level query, so reflect the flags latched from
  // AUTH_CMPL. Matches NimBLE getConnInfo() and the server's persisted flags.
  BLEConnInfoImpl::updateSecurityFlags(conn, _impl->secEncrypted, _impl->secAuthenticated, _impl->secBonded);
#if BLE5_SUPPORTED
  BLEConnInfoImpl::setPhy(conn, _impl->cachedTxPhy, _impl->cachedRxPhy);
#endif
  return conn;
}

// --------------------------------------------------------------------------
// Connection parameters
// --------------------------------------------------------------------------

// The update is asynchronous; LL acceptance is reported via the stack, not in this return value.
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
  impl.peerAddress.toEspBdAddr(cp.bda);
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
// PHY + DLE (BLE 5.0)
// --------------------------------------------------------------------------

BTStatus BLEClient::setPhy(BLEPhy txPhy, BLEPhy rxPhy) {
#if BLE5_SUPPORTED
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (!impl.connected) {
    log_w("Client: setPhy called but not connected");
    return BTStatus::InvalidState;
  }
  esp_bd_addr_t bda;
  impl.peerAddress.toEspBdAddr(bda);
  impl.phySync.take();
  esp_err_t err = esp_ble_gap_set_preferred_phy(
    bda, 0, blePhyToPrefMask(txPhy), blePhyToPrefMask(rxPhy), ESP_BLE_GAP_PHY_OPTIONS_NO_PREF
  );
  if (err != ESP_OK) {
    log_e("Client: esp_ble_gap_set_preferred_phy: %s", esp_err_to_name(err));
    impl.phySync.give(BTStatus::Fail);
    return BTStatus::Fail;
  }
  return impl.phySync.wait(2000);
#else
  (void)txPhy;
  (void)rxPhy;
  log_w("%s not supported on Bluedroid (BLE 5.0 unavailable)", __func__);
  return BTStatus::NotSupported;
#endif
}

BTStatus BLEClient::getPhy(BLEPhy &txPhy, BLEPhy &rxPhy) const {
  txPhy = BLEPhy::PHY_1M;
  rxPhy = BLEPhy::PHY_1M;
#if BLE5_SUPPORTED
  if (!_impl) {
    return BTStatus::InvalidState;
  }
  auto &impl = *_impl;
  if (!impl.connected) {
    log_w("Client: getPhy called but not connected");
    return BTStatus::InvalidState;
  }
  esp_bd_addr_t bda;
  impl.peerAddress.toEspBdAddr(bda);
  impl.phySync.take();
  esp_err_t err = esp_ble_gap_read_phy(bda);
  if (err != ESP_OK) {
    log_e("Client: esp_ble_gap_read_phy: %s", esp_err_to_name(err));
    impl.phySync.give(BTStatus::Fail);
    return BTStatus::Fail;
  }
  BTStatus st = impl.phySync.wait(2000);
  if (st == BTStatus::OK) {
    txPhy = impl.pendingTxPhy;
    rxPhy = impl.pendingRxPhy;
  }
  return st;
#else
  log_w("%s not supported on this Bluedroid path", __func__);
  return BTStatus::NotSupported;
#endif
}

// Bluedroid's set_pkt_data_len takes only tx octets; txTime is accepted for API parity and ignored.
BTStatus BLEClient::setDataLen(uint16_t txOctets, uint16_t /*txTime*/) {
#if BLE5_SUPPORTED
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (!impl.connected) {
    log_w("Client: setDataLen called but not connected");
    return BTStatus::InvalidState;
  }
  esp_bd_addr_t bda;
  impl.peerAddress.toEspBdAddr(bda);
  impl.dataLenSync.take();
  esp_err_t err = esp_ble_gap_set_pkt_data_len(bda, txOctets);
  if (err != ESP_OK) {
    log_e("Client: esp_ble_gap_set_pkt_data_len: %s", esp_err_to_name(err));
    impl.dataLenSync.give(BTStatus::Fail);
    return BTStatus::Fail;
  }
  return impl.dataLenSync.wait(2000);
#else
  (void)txOctets;
  log_w("%s not supported on Bluedroid (BLE 5.0 unavailable)", __func__);
  return BTStatus::NotSupported;
#endif
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
  BLEClient::Impl *client = nullptr;
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
      // CONNECT carries ble_addr_type; OPEN does not. Refresh peerAddress so
      // getConnInfo() / onConnect see the stack-reported type (may refine the
      // type passed into connect()).
      client->peerAddress = BTAddress(param->connect.remote_bda, static_cast<BTAddress::Type>(param->connect.ble_addr_type));
      // MTU exchange is performed from the caller thread in connect(), not from
      // any GATTC event callback: on Bluedroid a request issued at connection
      // setup is silently dropped (no CFG_MTU_EVT delivered).
      break;
    }

    case ESP_GATTC_OPEN_EVT:
    {
      if (param->open.status == ESP_GATT_OK) {
        client->connId = param->open.conn_id;
        client->connected = true;
        // Fresh link starts unencrypted; clear any latched flags from a prior
        // connection on this client so getConnInfo() cannot report stale state.
        client->secEncrypted = false;
        client->secAuthenticated = false;
        client->secBonded = false;
        log_i("Client: connected, connId=%u", client->connId.load());
        // The ATT MTU exchange is NOT requested here: on Bluedroid a request
        // issued at connection setup (event callback or immediately after) is
        // silently dropped. connect() performs it from the caller thread with a
        // retry once the link is up (see BLEClient::connect).
        // Prefer peerAddress (type latched on CONNECT) over open.remote_bda
        // (OPEN has no addr_type field).
        BLEConnInfo conn = BLEConnInfoImpl::make(
          client->connId, client->peerAddress.data(), client->mtu, /*central=*/true, client->peerAddress.type()
        );
        client->dispatchConnect(conn);
        client->connectSync.give(BTStatus::OK);
      } else {
        log_e("Client: GATTC open failed: status=%d", param->open.status);
        client->dispatchConnectFail(param->open.status);
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
      log_i("Client: disconnected, connId=%u reason=0x%02x", client->connId.load(), reason);

      // Release any waiting syncs
      client->connectSync.give(BTStatus::Fail);
      client->discoverSync.give(BTStatus::Fail);
      client->readSync.give(BTStatus::Fail);
      client->writeSync.give(BTStatus::Fail);
      client->mtuSync.give(BTStatus::Fail);
      client->rssiSync.give(BTStatus::Fail);

      if (wasConnected) {
        BLEConnInfo conn = BLEConnInfoImpl::make(
          client->connId, client->peerAddress.data(), client->mtu, /*central=*/true, client->peerAddress.type()
        );
        client->dispatchDisconnect(conn, reason);
      }

      client->connId = 0xFFFF;
      client->secEncrypted = false;
      client->secAuthenticated = false;
      client->secBonded = false;
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
      svc->uuid = bluedroidUuidToPublic(param->search_res.srvc_id.uuid);
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
      BLERemoteCharacteristic::NotifyCallback onNotifyCb;
      std::shared_ptr<BLERemoteCharacteristic::Impl> chrShared;
      {
        BLELockGuard lock(client->mtx);
        for (auto &svc : client->discoveredServices) {
          for (auto &chr : svc->characteristics) {
            if (chr->handle == handle && chr->onNotifyCb) {
              onNotifyCb = chr->onNotifyCb;
              chrShared = chr->shared_from_this();
              break;
            }
          }
          if (onNotifyCb) {
            break;
          }
        }
      }
      if (onNotifyCb) {
        BLERemoteCharacteristic chrHandle = BLERemoteCharacteristicImplCommon::makeHandle(chrShared);
        onNotifyCb(chrHandle, param->notify.value, param->notify.value_len, param->notify.is_notify);
      }
      break;
    }

    case ESP_GATTC_CFG_MTU_EVT:
    {
      if (param->cfg_mtu.status == ESP_GATT_OK) {
        client->mtu = param->cfg_mtu.mtu;
        log_i("Client: MTU exchanged, mtu=%u connId=%u", client->mtu.load(), client->connId.load());
      } else {
        log_w("Client: MTU exchange failed, status=%d", param->cfg_mtu.status);
      }
      client->mtuSync.give(param->cfg_mtu.status == ESP_GATT_OK ? BTStatus::OK : BTStatus::Fail);

      BLEConnInfo conn = BLEConnInfoImpl::make(
        client->connId, client->peerAddress.data(), client->mtu, /*central=*/true, client->peerAddress.type()
      );
      client->dispatchMtuChanged(conn, client->mtu);
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
#if BLE_SMP_SUPPORTED
  // Pairing/encryption completed, reported here as AUTH_CMPL: latch the security
  // level (there is no Bluedroid GATTC query for the live level so getConnInfo()
  // relies on this) and surface the peer identity. AUTH_CMPL carries only the peer
  // address, and Bluedroid lets several BLEClient app-ids share one physical ACL to
  // the same peer (each phase here makes its own client). Encryption/authentication
  // is a property of that shared link, so apply it to EVERY connected client on the
  // address, not just the first match.
  if (event == ESP_GAP_BLE_AUTH_CMPL_EVT && param->ble_security.auth_cmpl.success) {
    const auto addrType = static_cast<BTAddress::Type>(param->ble_security.auth_cmpl.addr_type);
    for (auto *c : s_clients) {
      if (c->connected && memcmp(c->peerAddress.data(), param->ble_security.auth_cmpl.bd_addr, 6) == 0) {
        c->peerAddress = BTAddress(param->ble_security.auth_cmpl.bd_addr, addrType);
        BLEConnInfo conn = BLEConnInfoImpl::make(c->connId, c->peerAddress.data(), c->mtu, /*central=*/true, addrType);
        BLEConnInfoImpl::updateSecurityFromAuthComplete(conn, param->ble_security.auth_cmpl.auth_mode);
        c->secEncrypted = conn.isEncrypted();
        c->secAuthenticated = conn.isAuthenticated();
        c->secBonded = conn.isBonded();
        c->dispatchIdentityResolved(conn);
      }
    }
  }
#endif /* BLE_SMP_SUPPORTED */

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

#if BLE5_SUPPORTED
  if (event == ESP_GAP_BLE_READ_PHY_COMPLETE_EVT) {
    BTStatus st = (param->read_phy.status == ESP_BT_STATUS_SUCCESS) ? BTStatus::OK : BTStatus::Fail;
    for (auto *c : s_clients) {
      if (c->connected && memcmp(c->peerAddress.data(), param->read_phy.bda, 6) == 0) {
        if (st == BTStatus::OK) {
          c->cachedTxPhy = param->read_phy.tx_phy;
          c->cachedRxPhy = param->read_phy.rx_phy;
          c->pendingTxPhy = static_cast<BLEPhy>(param->read_phy.tx_phy);
          c->pendingRxPhy = static_cast<BLEPhy>(param->read_phy.rx_phy);
        }
        c->phySync.give(st);
      }
    }
  }

  if (event == ESP_GAP_BLE_PHY_UPDATE_COMPLETE_EVT) {
    BTStatus st = (param->phy_update.status == ESP_BT_STATUS_SUCCESS) ? BTStatus::OK : BTStatus::Fail;
    for (auto *c : s_clients) {
      if (c->connected && memcmp(c->peerAddress.data(), param->phy_update.bda, 6) == 0) {
        if (st == BTStatus::OK) {
          c->cachedTxPhy = param->phy_update.tx_phy;
          c->cachedRxPhy = param->phy_update.rx_phy;
        }
        c->phySync.give(st);
      }
    }
  }

  if (event == ESP_GAP_BLE_SET_PKT_LENGTH_COMPLETE_EVT) {
    // Completion has no BDA; signal every client — only a taken waiter unblocks.
    BTStatus st = (param->pkt_data_length_cmpl.status == ESP_BT_STATUS_SUCCESS) ? BTStatus::OK : BTStatus::Fail;
    for (auto *c : s_clients) {
      c->dataLenSync.give(st);
    }
  }
#endif /* BLE5_SUPPORTED */
}

#else /* !BLE_GATT_CLIENT_SUPPORTED -- stubs */

#include "BLE.h"
#include "impl/BLEBackend.h"
#include "esp32-hal-log.h"

// Stubs for BLE_GATT_CLIENT_SUPPORTED == 0: log on most entry points; return NotSupported, empty,
// 0, -128, 0xFFFF, or no-op. getPhy() does not write txPhy/rxPhy; initialize or check the return.

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

#endif /* BLE_GATT_CLIENT_SUPPORTED */

#endif /* BLE_BLUEDROID */
