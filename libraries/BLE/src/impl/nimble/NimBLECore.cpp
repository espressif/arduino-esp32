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

/**
 * @file NimBLECore.cpp
 * @brief NimBLE backend for `BLEClass`: stack init/teardown, identity, MTU, IRK, whitelist, and GAP hook registration.
 */

#include "impl/common/BLEGuards.h"
#if BLE_NIMBLE

#include "BLE.h"

#include "NimBLECore.h"
#include "impl/common/BLEImplHelpers.h"
#include "impl/BLEBackend.h"
#if BLE_L2CAP_SUPPORTED
#include "NimBLEL2CAP.h"
#endif
#include "esp32-hal-bt.h"
#include "esp32-hal-alloc-ble-mem.h"
#include "esp32-hal-log.h"

#if defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
#include "esp32-hal-hosted.h"
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <nvs_flash.h>

#if SOC_BLE_SUPPORTED
#include <esp_bt.h>
#endif

#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
#include <host/ble_hs.h>
#include <host/ble_gap.h>

extern "C" void ble_store_config_init(void);
#include <host/util/util.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>
#include <store/config/ble_store_config.h>
#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LL_PRIVACY)
#include <host/ble_hs_pvcy.h>
#endif

// API contract is documented on the declarations in the public BLE*.h headers; the definitions below carry implementation notes only.

#if BLE_GATT_SERVER_SUPPORTED
// Defined in NimBLEServer.cpp: drops the deferred re-advertise event's backing
// reference so it is re-initialized after a stack restart (see the definition
// for the use-after-free this prevents across BLE.end()/begin()).
void nimbleResetReAdvertiseEvent();
#endif

// --------------------------------------------------------------------------
// BLEClass::Impl -- NimBLE backend state (declared in NimBLECore.h)
// --------------------------------------------------------------------------

void BLEClass::Impl::hostTask(void *param) {
  log_i("NimBLE host task started");
  nimble_port_run();
  nimble_port_freertos_deinit();
}

int BLEClass::Impl::onStoreStatus(struct ble_store_status_event *event, void *arg) {
  if (event->event_code == BLE_STORE_EVENT_FULL) {
    auto *sec = BLESecurity::Impl::instance();
    if (sec) {
      struct ble_store_key_sec key = {};
      struct ble_store_value_sec value = {};
      key.idx = 0;
      if (ble_store_read_peer_sec(&key, &value) == 0) {
        BTAddress oldest(value.peer_addr.val, static_cast<BTAddress::Type>(value.peer_addr.type));
        if (sec->notifyBondOverflow(oldest)) {
          return 0;
        }
      }
    }
  }
  return ble_store_util_status_rr(event, arg);
}

/**
 * @brief Notifies the implementation that the host stack reset; clears sync and logs the reason.
 * @param reason Host reset reason from NimBLE.
 */
void BLEClass::Impl::onReset(int reason) {
  auto *impl = BLE._impl;
  if (!impl || !impl->synced) {
    return;
  }
  impl->synced = false;
  log_i("NimBLE host reset, reason=%d", reason);
}

/**
 * @brief Invoked when the host stack is synchronized: sets GAP name, ensures addresses, and marks the stack ready.
 */
void BLEClass::Impl::onSync() {
  auto *impl = BLE._impl;
  if (!impl) {
    return;
  }

  if (impl->synced) {
    return;
  }

  if (BLE._deviceName.length() > 0) {
    int rc = ble_svc_gap_device_name_set(BLE._deviceName.c_str());
    if (rc != 0) {
      log_e("ble_svc_gap_device_name_set: rc=%d", rc);
    }
  }

  int rc = ble_hs_util_ensure_addr(0);
  if (rc == 0) {
    rc = ble_hs_util_ensure_addr(1);
  }
  if (rc != 0) {
    log_e("onSync: failed to ensure BLE address, rc=%d", rc);
    return;
  }

  rc = ble_hs_id_copy_addr(BLE_OWN_ADDR_PUBLIC, NULL, NULL);
  if (rc != 0) {
    log_d("No public address available, using random");
    impl->ownAddrType = BLE_OWN_ADDR_RANDOM;
  }
  ble_npl_time_delay(1);
  impl->synced = true;
}

// --------------------------------------------------------------------------
// BLEClass lifecycle
// --------------------------------------------------------------------------

BLEClass::BLEClass() : _impl(new Impl()) {}

BLEClass::~BLEClass() {
  if (_initialized) {
    end(false);
  }
  delete _impl;
  _impl = nullptr;
}

// When CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE is set, hostedInitBLE() runs first; a failed init returns BTStatus::Fail.
// If a prior end(true) released controller memory, re-initialization is rejected with BTStatus::InvalidState.
BTStatus BLEClass::begin(const String &deviceName) {
  if (_initialized) {
    return BTStatus::OK;
  }

#if defined(CONFIG_BT_CONTROLLER_ENABLED)
  if (btMemReleased(BT_MODE_BLE)) {
    log_e("Cannot reinitialize BLE: memory has been released");
    return BTStatus::InvalidState;
  }
#endif

  log_i("Initializing BLE stack: NimBLE");

#if defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
  if (!hostedInitBLE()) {
    log_e("Failed to initialize ESP-Hosted for BLE");
    return BTStatus::Fail;
  }
#endif

  esp_err_t err = nimble_port_init();
  if (err != ESP_OK) {
    log_e("nimble_port_init: rc=%d", err);
    return BTStatus::Fail;
  }

  ble_hs_cfg.reset_cb = BLEClass::Impl::onReset;
  ble_hs_cfg.sync_cb = BLEClass::Impl::onSync;
  ble_hs_cfg.store_status_cb = BLEClass::Impl::onStoreStatus;
  ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
  ble_hs_cfg.sm_bonding = 0;
  ble_hs_cfg.sm_mitm = 0;
  ble_hs_cfg.sm_sc = 1;
  ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC;
  ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC;
#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LL_PRIVACY)
  ble_hs_cfg.sm_our_key_dist |= BLE_SM_PAIR_KEY_DIST_ID;
  ble_hs_cfg.sm_their_key_dist |= BLE_SM_PAIR_KEY_DIST_ID;
#endif

  _deviceName = deviceName;

  ble_store_config_init();
  nimble_port_freertos_init(BLEClass::Impl::hostTask);

  constexpr int kSyncTimeoutMs = 5000;
  int waited = 0;
  while (!_impl->synced && waited < kSyncTimeoutMs) {
    vTaskDelay(pdMS_TO_TICKS(10));
    waited += 10;
  }
  if (!_impl->synced) {
    log_e("BLE host sync timeout after %d ms", kSyncTimeoutMs);
    nimble_port_stop();
    nimble_port_deinit();
    return BTStatus::Timeout;
  }

  _initialized = true;
  _ownAddressType = static_cast<BTAddress::Type>(_impl->ownAddrType);
  vTaskDelay(200 / portTICK_PERIOD_MS);
  return BTStatus::OK;
}

// hostedDeinitBLE() runs when CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE is set, after the NimBLE port is torn down.
// Irreversible: after end(true) you cannot call begin() again without a hardware reset.
void BLEClass::end(bool releaseMemory) {
  if (!_impl || !_initialized) {
    return;
  }

  // Reset advertising state before tearing down the stack.  reset() calls
  // stop() first (handles BLE_HS_EALREADY gracefully for a fresh stack), then
  // wipes all accumulated configuration (serviceUUIDs, appearance, custom data,
  // etc.).  This ensures that after begin() any BLEHIDDevice (or other service
  // that calls adv.addServiceUUID() in its constructor) starts from a clean
  // slate without requiring a manual adv.reset() call from user code.
  // The Bluedroid backend uses the same public reset() call for consistency.
  //
  // For scanning, stop() is sufficient; scan results are cleared by
  // startBlocking()/clearResults() and there is no equivalent accumulated state.
#if BLE_ADVERTISING_SUPPORTED
  getAdvertising().reset();
#endif
#if BLE_SCANNING_SUPPORTED
  getScan().stop();
  // Terminate any established periodic syncs while the host is still enabled so
  // applications need not call terminatePeriodicSync() before end() (no-op if
  // none are tracked). Doing this after nimble_port_stop() would race the host
  // teardown and log a benign BLE_HS_EDISABLED.
  getScan().terminateAllPeriodicSyncs();
#endif

#if BLE_GATT_SERVER_SUPPORTED
  // nimble_port_deinit() just freed the npl event pool; drop the deferred
  // re-advertise event's now-dangling backing pointer so a later begin()
  // re-initializes it instead of dispatching a stale/NULL callback.
  nimbleResetReAdvertiseEvent();
#endif

  nimble_port_stop();
  nimble_port_deinit();

#if defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
  hostedDeinitBLE();
#endif

  if (releaseMemory) {
#if SOC_BLE_SUPPORTED && CONFIG_BT_CONTROLLER_ENABLED
    btMemRelease(BT_MODE_BLE);
#endif
  }

  _impl->synced = false;
  _initialized = false;
}

// --------------------------------------------------------------------------
// Identity
// --------------------------------------------------------------------------

BTAddress BLEClass::getAddress() const {
  if (!_impl || !_initialized) {
    return BTAddress();
  }
  uint8_t addr[6];
  uint8_t type = _impl->ownAddrType;
  int rc = ble_hs_id_copy_addr(type & 1, addr, NULL);
  if (rc != 0) {
    return BTAddress();
  }
  return BTAddress(addr, static_cast<BTAddress::Type>(type));
}

// On CONFIG_IDF_TARGET_ESP32, RPA default types map to a random public type and ble_hs_pvcy_rpa_config is toggled; other types disable host privacy in that build.
BTStatus BLEClass::setOwnAddressType(BTAddress::Type type) {
  if (!_impl || !_initialized) {
    return BTStatus::NotInitialized;
  }
  uint8_t nimbleType = static_cast<uint8_t>(type);
  int rc = ble_hs_id_copy_addr(nimbleType & 1, NULL, NULL);
  if (rc != 0) {
    log_e("Unable to set address type %u, rc=%d", nimbleType, rc);
    return BTStatus::InvalidParam;
  }
  _impl->ownAddrType = nimbleType;

  if (nimbleType == BLE_OWN_ADDR_RPA_PUBLIC_DEFAULT || nimbleType == BLE_OWN_ADDR_RPA_RANDOM_DEFAULT) {
#ifdef CONFIG_IDF_TARGET_ESP32
    _impl->ownAddrType = BLE_OWN_ADDR_RANDOM;
    ble_hs_pvcy_rpa_config(NIMBLE_HOST_ENABLE_RPA);
#endif
  } else {
#ifdef CONFIG_IDF_TARGET_ESP32
    ble_hs_pvcy_rpa_config(NIMBLE_HOST_DISABLE_PRIVACY);
#endif
  }
  _ownAddressType = static_cast<BTAddress::Type>(_impl->ownAddrType);
  return BTStatus::OK;
}

BTStatus BLEClass::setOwnAddress(const BTAddress &addr) {
  if (!_impl || !_initialized) {
    return BTStatus::NotInitialized;
  }
  int rc = ble_hs_id_set_rnd(addr.data());
  if (rc != 0) {
    log_e("Failed to set address, rc=%d", rc);
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

// --------------------------------------------------------------------------
// MTU
// --------------------------------------------------------------------------

BTStatus BLEClass::setMTU(uint16_t mtu) {
  if (!_impl || !_initialized) {
    return BTStatus::NotInitialized;
  }
  int rc = ble_att_set_preferred_mtu(mtu);
  if (rc != 0) {
    log_e("ble_att_set_preferred_mtu: rc=%d", rc);
    return BTStatus::InvalidParam;
  }
  _impl->localMTU = mtu;
  return BTStatus::OK;
}

uint16_t BLEClass::getMTU() const {
  return _impl ? _impl->localMTU : BLE_ATT_MTU_DFLT;
}

// --------------------------------------------------------------------------
// IRK
// --------------------------------------------------------------------------

// Uses ble_store_read_our_sec index 0; the IRK is only present when the store entry includes it.
bool BLEClass::getLocalIRK(uint8_t irk[16]) const {
  if (!_impl || !_initialized || !irk) {
    return false;
  }
  struct ble_store_key_sec key = {};
  struct ble_store_value_sec value = {};
  key.peer_addr = (ble_addr_t){0, {0}};
  key.idx = 0;
  int rc = ble_store_read_our_sec(&key, &value);
  if (rc != 0 || !value.irk_present) {
    return false;
  }
  memcpy(irk, value.irk, 16);
  return true;
}

// The scan is capped at 100 bonds to avoid unbounded store iteration.
bool BLEClass::getPeerIRK(const BTAddress &peer, uint8_t irk[16]) const {
  if (!_impl || !_initialized || !irk) {
    return false;
  }
  int numBonds = 0;
  struct ble_store_key_sec key = {};
  struct ble_store_value_sec value = {};
  key.idx = 0;

  while (ble_store_read_peer_sec(&key, &value) == 0) {
    BTAddress bondAddr(value.peer_addr.val, static_cast<BTAddress::Type>(value.peer_addr.type));
    if (bondAddr == peer && value.irk_present) {
      memcpy(irk, value.irk, 16);
      return true;
    }
    key.idx++;
    numBonds++;
    if (numBonds > 100) {
      break;
    }
  }
  return false;
}

// --------------------------------------------------------------------------
// Default PHY (BLE 5.0)
// --------------------------------------------------------------------------

BTStatus BLEClass::setDefaultPhy(BLEPhy txPhy, BLEPhy rxPhy) {
#if BLE5_SUPPORTED
  if (!_impl || !_initialized) {
    log_w("setDefaultPhy: BLE not initialized");
    return BTStatus::NotInitialized;
  }
  uint8_t txMask = static_cast<uint8_t>(txPhy);
  uint8_t rxMask = static_cast<uint8_t>(rxPhy);
  int rc = ble_gap_set_prefered_default_le_phy(txMask, rxMask);
  if (rc != 0) {
    log_e("setDefaultPhy: ble_gap_set_prefered_default_le_phy rc=%d", rc);
    return BTStatus::Fail;
  }
  return BTStatus::OK;
#else
  log_w("setDefaultPhy not supported (BLE 5.0 unavailable)");
  return BTStatus::NotSupported;
#endif
}

// Not implemented on NimBLE; always returns BTStatus::NotSupported.
BTStatus BLEClass::getDefaultPhy(BLEPhy &txPhy, BLEPhy &rxPhy) const {
  txPhy = BLEPhy::PHY_1M;
  rxPhy = BLEPhy::PHY_1M;
  log_w("getDefaultPhy not supported on NimBLE");
  return BTStatus::NotSupported;
}

// --------------------------------------------------------------------------
// Whitelist
// --------------------------------------------------------------------------

// The entire list is re-sent to the stack on every change; duplicate add is a no-op to the driver.
BTStatus BLEClass::whiteListAdd(const BTAddress &address) {
  if (!_initialized) {
    return BTStatus::NotInitialized;
  }
  if (isOnWhiteList(address)) {
    return BTStatus::OK;
  }
  _whiteList.push_back(address);

  std::vector<ble_addr_t> addrs(_whiteList.size());
  for (size_t i = 0; i < _whiteList.size(); i++) {
    addrs[i].type = static_cast<uint8_t>(_whiteList[i].type());
    memcpy(addrs[i].val, _whiteList[i].data(), 6);
  }
  int rc = ble_gap_wl_set(addrs.data(), addrs.size());
  if (rc != 0) {
    log_e("Failed adding to whitelist, rc=%d", rc);
    _whiteList.pop_back();
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

// Like add, the whole whitelist array is set atomically in one ble_gap_wl_set call.
BTStatus BLEClass::whiteListRemove(const BTAddress &address) {
  if (!_initialized) {
    return BTStatus::NotInitialized;
  }
  for (auto it = _whiteList.begin(); it != _whiteList.end(); ++it) {
    if (*it == address) {
      _whiteList.erase(it);
      std::vector<ble_addr_t> addrs(_whiteList.size());
      for (size_t i = 0; i < _whiteList.size(); i++) {
        addrs[i].type = static_cast<uint8_t>(_whiteList[i].type());
        memcpy(addrs[i].val, _whiteList[i].data(), 6);
      }
      int rc = ble_gap_wl_set(addrs.data(), addrs.size());
      if (rc != 0) {
        log_e("Failed removing from whitelist, rc=%d", rc);
        _whiteList.push_back(address);
        return BTStatus::Fail;
      }
      return BTStatus::OK;
    }
  }
  log_d("whiteListRemove: address not found");
  return BTStatus::NotFound;
}

// startAdvertising/stopAdvertising are in NimBLEAdvertising.cpp

// --------------------------------------------------------------------------
// Stack info
// --------------------------------------------------------------------------

BLEClass::Stack BLEClass::getStack() const {
  return Stack::NimBLE;
}

const char *BLEClass::getStackName() const {
  return "NimBLE";
}

// When true, begin/end and optional setPins call into the hosted co-processor path.
bool BLEClass::isHostedBLE() const {
#if defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
  return true;
#else
  return false;
#endif
}

// --------------------------------------------------------------------------
// Hosted BLE pins
// --------------------------------------------------------------------------

// Only compiled with CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE; should be set before begin if the hosted transport needs reconfiguration.
BTStatus BLEClass::setPins(int8_t clk, int8_t cmd, int8_t d0, int8_t d1, int8_t d2, int8_t d3, int8_t rst) {
#if defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
  bool ok = hostedSetPins(clk, cmd, d0, d1, d2, d3, rst);
  if (!ok) {
    log_e("setPins: hostedSetPins failed");
    return BTStatus::Fail;
  }
  return BTStatus::OK;
#else
  log_w("setPins not supported (hosted NimBLE unavailable)");
  return BTStatus::NotSupported;
#endif
}

// --------------------------------------------------------------------------
// Custom event handlers
// --------------------------------------------------------------------------

BTStatus BLEClass::setCustomGapHandler(RawEventHandler handler) {
  if (!_impl || !_initialized) {
    return BTStatus::NotInitialized;
  }
  _impl->customGapHandler = handler;

  auto wrapper = [](struct ble_gap_event *event, void *arg) -> int {
    auto *impl = static_cast<BLEClass::Impl *>(arg);
    if (impl->customGapHandler) {
      return impl->customGapHandler(event, nullptr);
    }
    return 0;
  };

  int rc = ble_gap_event_listener_register(&_impl->gapListener, wrapper, _impl);
  if (rc == BLE_HS_EALREADY) {
    log_i("Already listening to GAP events");
  } else if (rc != 0) {
    log_e("ble_gap_event_listener_register: rc=%d", rc);
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

// GATT client raw hook not implemented on NimBLE; always returns BTStatus::NotSupported.
BTStatus BLEClass::setCustomGattcHandler(RawEventHandler /*handler*/) {
  log_w("setCustomGattcHandler not supported on NimBLE");
  return BTStatus::NotSupported;
}

// GATT server raw hook not implemented on NimBLE; always returns BTStatus::NotSupported.
BTStatus BLEClass::setCustomGattsHandler(RawEventHandler /*handler*/) {
  log_w("setCustomGattsHandler not supported on NimBLE");
  return BTStatus::NotSupported;
}

// --------------------------------------------------------------------------
// BLEClass factory methods
// --------------------------------------------------------------------------

#if BLE_SCANNING_SUPPORTED
BLEScan BLEClass::getScan() {
  static std::shared_ptr<BLEScan::Impl> slot;
  if (!isInitialized()) {
    log_e("getScan: BLE not initialized");
    return BLEScan();
  }
  if (!slot) {
    slot = std::make_shared<BLEScan::Impl>();
  }
  return BLEScan(slot);
}
#else
BLEScan BLEClass::getScan() {
  log_w("Scanning not supported");
  return BLEScan();
}
#endif

#if BLE_ADVERTISING_SUPPORTED
BLEAdvertising BLEClass::getAdvertising() {
  static std::shared_ptr<BLEAdvertising::Impl> slot;
  if (!isInitialized()) {
    log_e("getAdvertising: BLE not initialized");
    return BLEAdvertising();
  }
  if (!slot) {
    slot = std::make_shared<BLEAdvertising::Impl>();
  }
  return BLEAdvertising(slot);
}

BTStatus BLEClass::startAdvertising() {
  return getAdvertising().start();
}

BTStatus BLEClass::stopAdvertising() {
  return getAdvertising().stop();
}
#else
BLEAdvertising BLEClass::getAdvertising() {
  log_w("Advertising not supported");
  return BLEAdvertising();
}

BTStatus BLEClass::startAdvertising() {
  log_w("Advertising not supported");
  return BTStatus::NotSupported;
}

BTStatus BLEClass::stopAdvertising() {
  log_w("Advertising not supported");
  return BTStatus::NotSupported;
}
#endif

#if BLE_GATT_SERVER_SUPPORTED
BLEServer BLEClass::createServer() {
  static std::shared_ptr<BLEServer::Impl> slot;
  if (!isInitialized()) {
    log_e("createServer: BLE not initialized");
    return BLEServer();
  }
  if (!slot) {
    log_d("createServer: creating new server instance");
    slot = std::make_shared<BLEServer::Impl>();
  }
  return BLEServer(slot);
}
#else
BLEServer BLEClass::createServer() {
  log_w("GATT server not supported");
  return BLEServer();
}
#endif

#if BLE_GATT_CLIENT_SUPPORTED
BLEClient BLEClass::createClient() {
  if (!isInitialized()) {
    log_e("createClient: BLE not initialized");
    return BLEClient();
  }
  return BLEClient(std::make_shared<BLEClient::Impl>());
}
#else
BLEClient BLEClass::createClient() {
  log_w("GATT client not supported");
  return BLEClient();
}
#endif

#if BLE_SMP_SUPPORTED
BLESecurity BLEClass::getSecurity() {
  static std::shared_ptr<BLESecurity::Impl> slot;
  if (!isInitialized()) {
    log_e("getSecurity: BLE not initialized");
    return BLESecurity();
  }
  if (!slot) {
    slot = std::make_shared<BLESecurity::Impl>();
    BLESecurity::Impl::s_instance = slot.get();
  }
  return BLESecurity(slot);
}
#else
BLESecurity BLEClass::getSecurity() {
  log_w("SMP not supported");
  return BLESecurity();
}
#endif

#if BLE_L2CAP_SUPPORTED
BLEL2CAPServer BLEClass::createL2CAPServer(uint16_t psm, uint16_t mtu) {
  if (!isInitialized()) {
    log_e("createL2CAPServer: BLE not initialized");
    return BLEL2CAPServer();
  }
  auto impl = std::make_shared<BLEL2CAPServer::Impl>();
  if (!nimbleSetupL2CAPServer(impl, psm, mtu)) {
    return BLEL2CAPServer();
  }
  return BLEL2CAPServer(impl);
}

BLEL2CAPChannel BLEClass::connectL2CAP(uint16_t connHandle, uint16_t psm, uint16_t mtu) {
  if (!isInitialized()) {
    log_e("connectL2CAP: BLE not initialized");
    return BLEL2CAPChannel();
  }
  auto impl = std::make_shared<BLEL2CAPChannel::Impl>();
  if (!nimbleSetupL2CAPChannel(impl, connHandle, psm, mtu)) {
    return BLEL2CAPChannel();
  }
  return BLEL2CAPChannel(impl);
}
#endif

#endif /* BLE_NIMBLE */
