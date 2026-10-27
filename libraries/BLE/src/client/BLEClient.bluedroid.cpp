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

#include "core/BLEGuards.h"
#if BLE_BLUEDROID

#if BLE_GATT_CLIENT_SUPPORTED

#include "BLE.h"
#include "scan/BLEAdvertisedDevice.h"

#include "client/BLEClient.bluedroid.h"
#include "BLE.bluedroid.h"
#include "remote/BLERemoteGatt.bluedroid.h"
#include "types/BLEUUID.bluedroid.h"
#include "core/BLEImplHelpers.h"
#include "core/BLEAdmission.h"
#include "types/BLEConnInfoData.h"
#include "types/BLEConnInfo.bluedroid.h"
#include "esp32-hal-log.h"

#include <esp_gap_ble_api.h>
#include <esp_gattc_api.h>
#include <esp_gatt_common_api.h>
#include <new>
#include <string.h>

/**
 * @file BLEClient.bluedroid.cpp
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
StaticSemaphore_t BLEClient::Impl::s_clientsMtxStorage{};
SemaphoreHandle_t BLEClient::Impl::s_clientsMtx = xSemaphoreCreateRecursiveMutexStatic(&BLEClient::Impl::s_clientsMtxStorage);
std::vector<std::weak_ptr<BLEClient::Impl>> BLEClient::Impl::s_clients;
static BLEAdmissionGate s_terminalCloseAdmission(false);
StaticSemaphore_t BLEClient::Impl::s_terminalCloseOwnersMtxStorage{};
SemaphoreHandle_t BLEClient::Impl::s_terminalCloseOwnersMtx = xSemaphoreCreateRecursiveMutexStatic(&BLEClient::Impl::s_terminalCloseOwnersMtxStorage);
std::vector<std::shared_ptr<BLEClient::Impl>> BLEClient::Impl::s_terminalCloseOwners;

#include "core/BLEValidationHooks.h"

// --------------------------------------------------------------------------
// Impl destructor
// --------------------------------------------------------------------------

/**
 * @brief Tear down the client: remove from the global list, unregister GATTC, delete mutex.
 * @note GATT unregistration and mutex deletion run here; not safe to use after return.
 */
BLEClient::Impl::~Impl() {
  removeClient(this);
  // Unregister GATTC app if registered
  if (gattcIf != ESP_GATT_IF_NONE) {
    esp_ble_gattc_app_unregister(gattcIf);
    gattcIf = ESP_GATT_IF_NONE;
  }
  // Note: mtx is owned and destroyed by BLEClientImplCommon.
}

std::vector<std::shared_ptr<BLEClient::Impl>> BLEClient::Impl::snapshotClients() {
  std::vector<std::shared_ptr<Impl>> clients;
  BLELockGuard lock(s_clientsMtx);
  clients.reserve(s_clients.size());
  for (auto it = s_clients.begin(); it != s_clients.end();) {
    auto client = it->lock();
    if (!client) {
      it = s_clients.erase(it);
      continue;
    }
    clients.push_back(std::move(client));
    ++it;
  }
  return clients;
}

void BLEClient::Impl::removeClient(Impl *client) {
  BLELockGuard lock(s_clientsMtx);
  for (auto it = s_clients.begin(); it != s_clients.end();) {
    auto owner = it->lock();
    if (!owner || owner.get() == client) {
      it = s_clients.erase(it);
    } else {
      ++it;
    }
  }
}

// --------------------------------------------------------------------------
// Callback dispatch helpers
// --------------------------------------------------------------------------

// dispatchConnect / dispatchDisconnect / dispatchConnectFail / dispatchMtuChanged
// are shared and live on BLEClientImplCommon (client/BLEClientImpl.cpp);
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
    for (const auto &c : BLEClient::Impl::snapshotClients()) {
      if (c->appId == candidate) {
        inUse = true;
        break;
      }
    }
    if (!inUse) {
      return candidate;
    }

    candidate = BLEClient::Impl::s_nextAppId;
    if (candidate == startedAt) {
      return APP_ID_INVALID;
    }
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

  {
    BLELockGuard lock(BLEClient::Impl::s_clientsMtx);
    BLEClient::Impl::s_clients.push_back(impl);
  }

  impl->regSync.take();
  esp_err_t err = esp_ble_gattc_app_register(impl->appId);
  if (err != ESP_OK) {
    log_e("esp_ble_gattc_app_register: %s", esp_err_to_name(err));
    BLEClient::Impl::removeClient(impl.get());
    return false;
  }

  BTStatus st = impl->regSync.wait(5000);
  if (!st) {
    log_e("GATTC app registration failed");
    BLEClient::Impl::removeClient(impl.get());
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
    esp_err_t preferredParamsErr = esp_ble_gap_prefer_ext_connect_params_set(bda, phyMask, p1m, p2m, pcoded);
    if (preferredParamsErr != ESP_OK) {
      log_w("Client: prefer_ext_connect_params_set: %s (continuing with gattc_enh_open)", esp_err_to_name(preferredParamsErr));
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
    esp_err_t preferredParamsErr = esp_ble_gap_prefer_ext_connect_params_set(bda, phyMask, p1m, p2m, pcoded);
    if (preferredParamsErr != ESP_OK) {
      log_w("Client: prefer_ext_connect_params_set: %s (continuing with gattc_enh_open)", esp_err_to_name(preferredParamsErr));
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

// Blocks until DISCONNECT_EVT clears state and invokes user disconnect callbacks (when not on BTC_TASK).
BTStatus BLEClient::disconnect() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);

  if (!impl.connected) {
    log_w("Client: disconnect called but not connected");
    return BTStatus::NotConnected;
  }

  const char *taskName = pcTaskGetName(nullptr);
  bool wait = !(taskName && strcmp(taskName, "BTC_TASK") == 0);
  if (wait) {
    impl.disconnectSync.take();
  }

  esp_err_t err = esp_ble_gattc_close(impl.gattcIf, impl.connId);
  if (err != ESP_OK) {
    if (wait) {
      impl.disconnectSync.give(BTStatus::Fail);
    }
    log_e("esp_ble_gattc_close: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }
  if (wait) {
    // Wait unconditionally: esp_ble_gattc_close() may complete synchronously
    // enough that `connected` is already cleared by CLOSE/DISCONNECT_EVT
    // before we re-read it (notably on a shared ACL, where only CLOSE_EVT
    // arrives). Re-reading the flag here raced that teardown and returned
    // before onDisconnect had been dispatched.
    return impl.disconnectSync.wait(5000);
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
    impl.clearDiscoveredServices();
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

  esp_err_t mtuErr = esp_ble_gatt_set_local_mtu(mtu);
  if (mtuErr != ESP_OK) {
    log_e("Client: esp_ble_gatt_set_local_mtu(%u): %s", mtu, esp_err_to_name(mtuErr));
    return;
  }

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
  BLEConnInfo conn = BLEConnInfoImpl::make(_impl->connId, _impl->peerAddress, _impl->mtu, /*central=*/true);
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
  esp_err_t err = esp_ble_gap_set_preferred_phy(bda, 0, blePhyToPrefMask(txPhy), blePhyToPrefMask(rxPhy), ESP_BLE_GAP_PHY_OPTIONS_NO_PREF);
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

// Idempotent virtual-disconnect teardown shared by DISCONNECT_EVT and
// CLOSE_EVT. On a shared ACL (several GATTC apps on one physical link),
// esp_ble_gattc_close() for one virtual connection produces CLOSE_EVT but no
// DISCONNECT_EVT, so CLOSE_EVT must complete the same teardown. A later
// DISCONNECT_EVT for the same conn_id no-ops because connId has already been
// reset to 0xFFFF. Ordering (DESIGN.md): connection state, security flags and
// the discovered tree are torn down BEFORE onDisconnect is dispatched, and
// disconnectSync is given only after the dispatch so a blocking disconnect()
// cannot return before the user callback has run.
void BLEClient::Impl::retainTerminalCloseOwner(const std::shared_ptr<Impl> &owner) {
  if (!owner) {
    return;
  }
  BLELockGuard lock(s_terminalCloseOwnersMtx);
  for (const auto &existing : s_terminalCloseOwners) {
    if (existing == owner) {
      return;
    }
  }
  s_terminalCloseOwners.push_back(owner);
}

std::shared_ptr<BLEClient::Impl> BLEClient::Impl::takeTerminalCloseOwner(Impl *client) {
  BLELockGuard lock(s_terminalCloseOwnersMtx);
  for (auto it = s_terminalCloseOwners.begin(); it != s_terminalCloseOwners.end(); ++it) {
    if (it->get() == client) {
      auto owner = std::move(*it);
      s_terminalCloseOwners.erase(it);
      return owner;
    }
  }
  return {};
}

static void completeClientDisconnect(BLEClient::Impl *client, uint16_t evtConnId, uint8_t reason) {
  if (!client) {
    return;
  }
  bool wasConnected;
  BLEConnInfo conn;
  std::shared_ptr<BLEClient::Impl> pendingWriteOwner;
  std::shared_ptr<BLEClient::Impl> terminalCloseOwner;
  {
    BLELockGuard lock(client->mtx);
    if (evtConnId != client->connId.load()) {
      return;
    }
    wasConnected = client->connected;
    // Capture conn info before connId is reset so the callback still sees the
    // connection that just ended.
    conn = BLEConnInfoImpl::make(client->connId, client->peerAddress, client->mtu, /*central=*/true);
    client->connected = false;
    client->connId = 0xFFFF;
    client->secEncrypted = false;
    client->secAuthenticated = false;
    client->secBonded = false;
    client->pendingReadHandle = 0;
    client->requestHandle = 0;
    client->requestWaiter = false;
    client->commandCount = 0;
    pendingWriteOwner = std::move(client->writeOwner);
    client->terminalCloseRetryActive = false;
#if BLE_VALIDATION_HOOKS
    client->validationHoldWriteCommand = false;
    client->validationHeldWriteCommandEventSeen = false;
#endif
    client->clearDiscoveredServices();
  }
  terminalCloseOwner = BLEClient::Impl::takeTerminalCloseOwner(client);
  log_i("Client: disconnected, connId=%u reason=0x%02x", evtConnId, reason);

  // Release any waiting syncs
  client->connectSync.give(BTStatus::Fail);
  client->discoverSync.give(BTStatus::Fail);
  client->readSync.give(BTStatus::Fail);
  client->writeSync.give(BTStatus::Fail);
  client->mtuSync.give(BTStatus::Fail);
  client->rssiSync.give(BTStatus::Fail);

  if (wasConnected) {
    client->dispatchDisconnect(conn, reason);
  }
  client->disconnectSync.give(BTStatus::OK);
}

struct TerminalCloseTaskArg {
  std::shared_ptr<BLEClient::Impl> owner;
};

static void retryTerminalClientClose(const std::shared_ptr<BLEClient::Impl> &owner) {
  BLEClient::Impl *client = owner.get();
  if (!client) {
    return;
  }
  constexpr uint8_t kMaxCloseAttempts = 8;
  for (uint8_t retry = 0; retry < kMaxCloseAttempts; ++retry) {
    if (!client->terminalCloseRetryActive.load() || !BLE.isInitialized()) {
      break;
    }
    uint16_t connId = client->connId.load();
    if (!client->connected || connId == 0xFFFF) {
      return;
    }
    if (esp_ble_gattc_close(client->gattcIf, connId) == ESP_OK) {
      return;
    }
    vTaskDelay(pdMS_TO_TICKS(1U << retry));
  }

  if (BLE.isInitialized() && client->connected) {
    esp_bd_addr_t peer;
    client->peerAddress.toEspBdAddr(peer);
    if (esp_ble_gap_disconnect(peer) == ESP_OK) {
      return;
    }
    // No retry task remains. Keep the self-owner as a quarantine reference;
    // disconnect or BLE.end() releases it explicitly.
    log_e("Client: terminal close retries exhausted; connection quarantined");
    client->terminalCloseRetryActive = false;
    return;
  }

  // Host shutdown makes native callbacks impossible. Drop the registry owner,
  // but keep the task-local owner alive until this function has returned.
  auto terminalCloseOwner = BLEClient::Impl::takeTerminalCloseOwner(client);
  client->terminalCloseRetryActive = false;
}

static void terminalClientCloseTask(void *arg) {
  auto *taskArg = static_cast<TerminalCloseTaskArg *>(arg);
  std::shared_ptr<BLEClient::Impl> owner = std::move(taskArg->owner);
  delete taskArg;
#if BLE_VALIDATION_HOOKS
  BLEHooks.gatt.terminalWorkerEntered.store(true, std::memory_order_release);
  BLEValidationHooks::holdWhile(BLEHooks.gatt.holdTerminalWorker);
#endif
  retryTerminalClientClose(owner);
  // FreeRTOS task deletion does not unwind C++ locals.
  owner.reset();
  s_terminalCloseAdmission.leave();
  vTaskDelete(nullptr);
}

static void scheduleTerminalClientClose(BLEClient::Impl *client) {
  if (!client || !s_terminalCloseAdmission.enter()) {
    return;
  }
  bool expected = false;
  if (!client->terminalCloseRetryActive.compare_exchange_strong(expected, true)) {
    s_terminalCloseAdmission.leave();
    return;
  }
  auto owner = client->shared_from_this();
  BLEClient::Impl::retainTerminalCloseOwner(owner);
  auto *taskArg = new (std::nothrow) TerminalCloseTaskArg{owner};
  bool forceTaskFailure = false;
#if BLE_VALIDATION_HOOKS
  int remaining = BLEHooks.gatt.terminalWorkerStartFailures.load(std::memory_order_relaxed);
  while (remaining > 0 && !BLEHooks.gatt.terminalWorkerStartFailures.compare_exchange_weak(remaining, remaining - 1, std::memory_order_relaxed)) {}
  forceTaskFailure = remaining > 0;
#endif
  if (!taskArg || forceTaskFailure || xTaskCreate(terminalClientCloseTask, "ble-gatt-close", 3072, taskArg, 1, nullptr) != pdPASS) {
    delete taskArg;
    client->terminalCloseRetryActive = false;
    esp_bd_addr_t peer;
    client->peerAddress.toEspBdAddr(peer);
    if (esp_ble_gap_disconnect(peer) != ESP_OK) {
      // Keep the terminal owner registry entry as a quarantine reference. A later explicit
      // disconnect or host shutdown still releases it through normal teardown.
      log_e("Client: unable to schedule terminal close; connection quarantined");
    }
    s_terminalCloseAdmission.leave();
  }
}

void BLEClient::Impl::openTerminalCloseAdmission() {
  s_terminalCloseAdmission.reopen();
}

void BLEClient::Impl::stopTerminalCloseWorkers() {
  s_terminalCloseAdmission.close();
#if BLE_VALIDATION_HOOKS
  BLEHooks.gatt.terminalShutdownEntered.store(true, std::memory_order_release);
#endif
  // BLE.end() has already cleared the initialized flag, which makes every
  // worker leave its bounded retry loop. Wait before traversing s_clients:
  // dropping a worker's final owner erases that client from this registry.
  s_terminalCloseAdmission.waitUntilIdle();
}

void BLEClient::Impl::invalidateAfterHostDeinit() {
  for (const auto &client : snapshotClients()) {
    // A new Bluedroid instance may reuse gattc_if values. Retained handles must
    // therefore become detached shells before begin() can register fresh
    // clients, or a stale handle can steal the fresh client's callbacks.
    client->gattcIf = ESP_GATT_IF_NONE;
    client->appId = APP_ID_INVALID;
    client->connected = false;
    client->connId = UINT16_MAX;
    client->mtu = 23;
    client->connectionGeneration.fetch_add(1, std::memory_order_relaxed);
    client->secEncrypted = false;
    client->secAuthenticated = false;
    client->secBonded = false;
    client->terminalCloseRetryActive = false;

    {
      BLELockGuard lock(client->mtx);
      client->clearDiscoveredServices();
      client->readBuf.clear();
      client->pendingReadHandle = 0;
      client->requestHandle = 0;
      client->requestWaiter = false;
      client->commandCount = 0;
      client->writeOwner.reset();
#if BLE_VALIDATION_HOOKS
      client->validationHoldWriteCommand = false;
      client->validationHeldWriteCommandEventSeen = false;
#endif
    }

    // Release any API call whose native completion disappeared with the host.
    client->regSync.give(BTStatus::Fail);
    client->connectSync.give(BTStatus::Fail);
    client->disconnectSync.give(BTStatus::Fail);
    client->discoverSync.give(BTStatus::Fail);
    client->readSync.give(BTStatus::Fail);
    client->writeSync.give(BTStatus::Fail);
    client->mtuSync.give(BTStatus::Fail);
    client->rssiSync.give(BTStatus::Fail);
#if BLE5_SUPPORTED
    client->phySync.give(BTStatus::Fail);
    client->dataLenSync.give(BTStatus::Fail);
#endif
  }
}

void BLEClient::Impl::releaseTerminalCloseOwners() {
  std::vector<std::shared_ptr<Impl>> owners;
  {
    BLELockGuard lock(s_terminalCloseOwnersMtx);
    owners.swap(s_terminalCloseOwners);
  }
  for (const auto &owner : owners) {
    owner->terminalCloseRetryActive = false;
  }
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
  auto clients = snapshotClients();
  // REG_EVT: match by app_id
  if (event == ESP_GATTC_REG_EVT) {
    for (const auto &owner : clients) {
      auto *c = owner.get();
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
  for (const auto &owner : clients) {
    auto *c = owner.get();
    if (c->gattcIf == gattc_if) {
      client = c;
      break;
    }
  }

  // For CONNECT_EVT, also try matching by BDA if gattc_if didn't match.
  // Several clients may share one link to the same peer, so the address alone
  // does not identify one: only consider a client that is still waiting for a
  // connection. Without that guard a stray event (one whose gattc_if belongs to
  // a client destroyed mid-connect) could land on a live client and overwrite
  // its connId, pointing every later GATT operation at a foreign connection.
  if (!client && event == ESP_GATTC_CONNECT_EVT) {
    for (const auto &owner : clients) {
      auto *c = owner.get();
      if (!c->connected && c->peerAddress.equalsEspBdAddr(param->connect.remote_bda)) {
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
      // CONNECT_EVT is broadcast by Bluedroid to every registered gattc_if when
      // any physical ACL link comes up. Do NOT set client->connected or
      // client->connId here: only ESP_GATTC_OPEN_EVT establishes this client's
      // virtual connection. Just refine the peer's address type when this
      // client's target BDA matches.
      if (client->peerAddress.equalsEspBdAddr(param->connect.remote_bda)) {
        client->peerAddress = BTAddress::fromEspBdAddr(param->connect.remote_bda, static_cast<BTAddress::Type>(param->connect.ble_addr_type));
      }
      break;
    }

    case ESP_GATTC_OPEN_EVT:
    {
      if (param->open.status == ESP_GATT_OK) {
        BLEConnInfo conn;
        {
          BLELockGuard lock(client->mtx);
          client->connId = param->open.conn_id;
          client->connected = true;
          client->connectionGeneration.fetch_add(1);
          // Fresh link starts unencrypted; clear any latched flags from a prior
          // connection on this client so getConnInfo() cannot report stale state.
          client->secEncrypted = false;
          client->secAuthenticated = false;
          client->secBonded = false;
          conn = BLEConnInfoImpl::make(client->connId, client->peerAddress, client->mtu, /*central=*/true);
        }
        log_i("Client: connected, connId=%u", client->connId.load());
        // The ATT MTU exchange is NOT requested here: on Bluedroid a request
        // issued at connection setup (event callback or immediately after) is
        // silently dropped. connect() performs it from the caller thread with a
        // retry once the link is up (see BLEClient::connect).
        // Prefer peerAddress (type latched on CONNECT) over open.remote_bda
        // (OPEN has no addr_type field).
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
      completeClientDisconnect(client, param->disconnect.conn_id, static_cast<uint8_t>(param->disconnect.reason));
      break;
    }

    case ESP_GATTC_CLOSE_EVT:
    {
      // Virtual-connection close (e.g. close() on a shared ACL): no
      // DISCONNECT_EVT follows, so this is the disconnect completion.
      completeClientDisconnect(client, param->close.conn_id, static_cast<uint8_t>(param->close.reason));
      break;
    }

    case ESP_GATTC_SEARCH_RES_EVT:
    {
      auto svc = std::make_shared<BLERemoteService::Impl>();
      svc->uuid = bluedroidUuidToPublic(param->search_res.srvc_id.uuid);
      svc->startHandle = param->search_res.start_handle;
      svc->endHandle = param->search_res.end_handle;
      svc->client = client;
      svc->clientOwner = client->shared_from_this();
      svc->cacheMtx = client->mtxOwner;
      svc->clientGeneration = client->connectionGeneration.load();
      BLELockGuard lock(client->mtx);
      client->discoveredServices.push_back(svc);
      break;
    }

    case ESP_GATTC_SEARCH_CMPL_EVT:
    {
      client->discoverSync.give(param->search_cmpl.status == ESP_GATT_OK ? BTStatus::OK : BTStatus::Fail);
      break;
    }

    case ESP_GATTC_QUEUE_FULL_EVT:
    {
      if (!param->queue_full.is_full || param->queue_full.conn_id != client->connId.load()) {
        break;
      }
      // Bluedroid does not identify which deferred command was rejected.
      // Retiring any one local slot would risk completing the wrong caller, so
      // fail the connection as a unit. DISCONNECT/CLOSE teardown releases all
      // waiters and retained asynchronous-write ownership consistently.
      log_e("Client: Bluedroid GATTC command queue full; closing connection");
      if (esp_ble_gattc_close(client->gattcIf, client->connId.load()) != ESP_OK) {
        scheduleTerminalClientClose(client);
      }
      break;
    }

    case ESP_GATTC_READ_CHAR_EVT:
    case ESP_GATTC_READ_DESCR_EVT:
    {
      {
        BLELockGuard lock(client->mtx);
        if (client->pendingReadHandle == 0 || param->read.handle != client->pendingReadHandle) {
          break;
        }
        client->pendingReadHandle = 0;
        client->lastReadStatus = param->read.status;
        if (param->read.status == ESP_GATT_OK) {
          client->readBuf.assign(param->read.value, param->read.value + param->read.value_len);
        } else {
          client->readBuf.clear();
        }
      }
      client->readSync.give(param->read.status == ESP_GATT_OK ? BTStatus::OK : BTStatus::Fail);
      break;
    }

    case ESP_GATTC_WRITE_CHAR_EVT:
    case ESP_GATTC_WRITE_DESCR_EVT:
    {
      bool signalWaiter = false;
      std::shared_ptr<BLEClient::Impl> pendingWriteOwner;
      {
        BLELockGuard lock(client->mtx);
        esp_gatt_status_t completionStatus = param->write.status;
        if (client->requestHandle != 0 && param->write.handle == client->requestHandle) {
          signalWaiter = client->requestWaiter;
          bool holdResponseRetirement = false;
#if BLE_VALIDATION_HOOKS
          holdResponseRetirement = client->requestWaiter && BLEHooks.remote.holdWriteResponseRetirement.load(std::memory_order_relaxed);
#endif
          if (!holdResponseRetirement) {
            client->requestHandle = 0;
            client->requestWaiter = false;
            if (client->commandCount == 0) {
              pendingWriteOwner = std::move(client->writeOwner);
            }
          }
          client->lastWriteStatus = completionStatus;
        } else if (client->commandCount > 0) {
#if BLE_VALIDATION_HOOKS
          if (client->validationHoldWriteCommand) {
            client->validationHeldWriteCommandEventSeen = true;
            break;
          }
          int remainingFailures = BLEHooks.remote.writeCommandCompletionFailures.load(std::memory_order_relaxed);
          while (remainingFailures > 0
                 && !BLEHooks.remote.writeCommandCompletionFailures.compare_exchange_weak(remainingFailures, remainingFailures - 1, std::memory_order_relaxed)
          ) {}
          if (remainingFailures > 0) {
            completionStatus = ESP_GATT_ERROR;
          }
          bool checkCombinedOwner = client->requestHandle != 0;
#endif
          if (--client->commandCount == 0 && client->requestHandle == 0) {
            pendingWriteOwner = std::move(client->writeOwner);
          }
#if BLE_VALIDATION_HOOKS
          if (checkCombinedOwner) {
            BLEHooks.remote.combinedWriteOwnerObserved.store(true, std::memory_order_relaxed);
            if (!client->writeOwner) {
              BLEHooks.remote.combinedWriteOwnerHeld.store(false, std::memory_order_relaxed);
            }
            BLEHooks.remote.holdWriteResponseRetirement.store(false, std::memory_order_release);
          }
#endif
        } else {
          break;
        }
      }
      if (signalWaiter) {
        client->writeSync.give(param->write.status == ESP_GATT_OK ? BTStatus::OK : BTStatus::Fail);
      }
#if BLE_VALIDATION_HOOKS
      if (BLEHooks.gatt.forceTerminalClose.exchange(false, std::memory_order_acq_rel)) {
        scheduleTerminalClientClose(client);
      }
#endif
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
              // Cache the payload into the characteristic value before
              // dispatch (parity with NimBLE) so readRawData() after a
              // notification returns these bytes, and pass the stable
              // cached buffer rather than the transient stack buffer.
              chr->value.assign(param->notify.value, param->notify.value + param->notify.value_len);
              break;
            }
          }
          if (onNotifyCb) {
            break;
          }
        }
      }
      if (onNotifyCb && chrShared) {
        BLERemoteCharacteristic chrHandle = BLERemoteCharacteristicImplCommon::makeHandle(chrShared);
        onNotifyCb(chrHandle, param->notify.value, param->notify.value_len, param->notify.is_notify ? BLESubscription::Notify : BLESubscription::Indicate);
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

      BLEConnInfo conn = BLEConnInfoImpl::make(client->connId, client->peerAddress, client->mtu, /*central=*/true);
      client->dispatchMtuChanged(conn, client->mtu);
      break;
    }

    case ESP_GATTC_SRVC_CHG_EVT:
    {
      log_i("Client: service changed indication received");
      // Bluedroid intercepts 0x2A05 instead of delivering NOTIFY_EVT and does
      // not expose the changed range. Existing handles are therefore unsafe:
      // invalidate the whole discovered tree and require rediscovery. Preserve
      // the Service Changed callback itself long enough to report the event.
      BLERemoteCharacteristic::NotifyCallback svcChgCb;
      std::shared_ptr<BLERemoteCharacteristic::Impl> svcChgChr;
      {
        BLELockGuard lock(client->mtx);
        for (auto &svc : client->discoveredServices) {
          if (!svc) {
            continue;
          }
          const bool isGattSvc = (svc->uuid == BLEUUID(static_cast<uint16_t>(0x1801)));
          for (auto &chr : svc->characteristics) {
            if (!chr || !chr->onNotifyCb) {
              continue;
            }
            if (isGattSvc && chr->uuid == BLEUUID(static_cast<uint16_t>(0x2A05))) {
              svcChgCb = chr->onNotifyCb;
              svcChgChr = chr->shared_from_this();
            }
          }
        }
        client->clearDiscoveredServices();
      }
      if (svcChgCb && svcChgChr) {
        // ESP_GATTC_SRVC_CHG_EVT omits the on-air range. Report the conservative
        // full database range that matches the full-tree invalidation above;
        // do not reuse or re-register any stale native handle.
        static const uint8_t range[4] = {0x01, 0x00, 0xFF, 0xFF};
        {
          BLELockGuard lock(client->mtx);
          svcChgChr->value.assign(range, range + sizeof(range));
        }
        BLERemoteCharacteristic chrHandle = BLERemoteCharacteristicImplCommon::makeHandle(svcChgChr);
        svcChgCb(chrHandle, range, sizeof(range), BLESubscription::Indicate);
      }
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
  auto clients = snapshotClients();
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
    for (const auto &owner : clients) {
      auto *c = owner.get();
      if (c->connected && c->peerAddress.equalsEspBdAddr(param->ble_security.auth_cmpl.bd_addr)) {
        c->peerAddress = BTAddress::fromEspBdAddr(param->ble_security.auth_cmpl.bd_addr, addrType);
        BLEConnInfo conn = BLEConnInfoImpl::make(c->connId, c->peerAddress, c->mtu, /*central=*/true);
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
      // RSSI belongs to the physical ACL, and Bluedroid lets several BLEClient
      // app-ids share one link to the same peer (see the AUTH_CMPL handler
      // above). The completion event carries only the peer address, so it
      // cannot say which of them asked. Update and release every client on that
      // address: stopping at the first match released whichever client happened
      // to be first in the list and left the actual requester waiting until it
      // timed out, so getRSSI() returned -128 whenever more than one client
      // shared the link.
      for (const auto &owner : clients) {
        auto *c = owner.get();
        if (c->peerAddress.equalsEspBdAddr(param->read_rssi_cmpl.remote_addr)) {
          c->lastRssi = param->read_rssi_cmpl.rssi;
          c->rssiSync.give(BTStatus::OK);
        }
      }
    } else {
      // GAP RSSI failure does not include BDA — give(Fail) to all clients to
      // avoid leaving any waiter stuck.  This may unblock a client that did not
      // request the read; callers already treat Fail as -128, so the impact is
      // limited to a spurious early return.
      for (const auto &owner : clients) {
        auto *c = owner.get();
        c->rssiSync.give(BTStatus::Fail);
      }
    }
  }

#if BLE5_SUPPORTED
  if (event == ESP_GAP_BLE_READ_PHY_COMPLETE_EVT) {
    BTStatus st = (param->read_phy.status == ESP_BT_STATUS_SUCCESS) ? BTStatus::OK : BTStatus::Fail;
    for (const auto &owner : clients) {
      auto *c = owner.get();
      if (c->connected && c->peerAddress.equalsEspBdAddr(param->read_phy.bda)) {
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
    for (const auto &owner : clients) {
      auto *c = owner.get();
      if (c->connected && c->peerAddress.equalsEspBdAddr(param->phy_update.bda)) {
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
    for (const auto &owner : clients) {
      auto *c = owner.get();
      c->dataLenSync.give(st);
    }
  }
#endif /* BLE5_SUPPORTED */
}

#else /* !BLE_GATT_CLIENT_SUPPORTED -- stubs */

#include "BLE.h"
#include "core/BLEBackend.h"
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
