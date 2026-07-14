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

/**
 * @file BluedroidCore.cpp
 * @brief Bluedroid backend for `BLEClass`: stack init/teardown, identity, MTU, IRK, whitelist, and GAP/GATT callback dispatch.
 */

#include "impl/common/BLEGuards.h"
#if BLE_BLUEDROID

#include "BLE.h"
#include "BluedroidCore.h"
#if BLE_GATT_SERVER_SUPPORTED
#include "BluedroidServer.h"
#endif
#if BLE_GATT_CLIENT_SUPPORTED
#include "BluedroidClient.h"
#endif
#if BLE_ADVERTISING_SUPPORTED
#include "BluedroidAdvertising.h"
#endif
#if BLE_SCANNING_SUPPORTED
#include "BluedroidScan.h"
#endif
#if BLE_SMP_SUPPORTED
#include "BluedroidSecurity.h"
#endif
#include "impl/common/BLEImplHelpers.h"
#include "esp32-hal-bt.h"
#include "esp32-hal-alloc-ble-mem.h"
#include "esp32-hal-log.h"

#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>
#if BLE_GATT_SERVER_SUPPORTED
#include <esp_gatts_api.h>
#endif
#if BLE_GATT_CLIENT_SUPPORTED
#include <esp_gattc_api.h>
#endif
#include <esp_gatt_common_api.h>
#include <esp_bt_device.h>

#include "impl/common/BLESync.h"

#if BLE_SMP_SUPPORTED
// Defined in BluedroidSecurity.cpp
void bluedroidSecurityHandleGAP(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
#endif

// API contract is documented on the declarations in the public BLE*.h headers; the definitions below carry implementation notes only.

// BLEClass::Impl (Bluedroid backend state) is declared in BluedroidCore.h.

/**
 * @brief Dispatches GAP events to advertising, scan, GATT, security, and custom handlers; signals local privacy completion.
 * @param event GAP event code from the Bluedroid callback.
 * @param param Event-specific parameters from Bluedroid.
 */
void BLEClass::Impl::gapCallback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  auto *impl = BLE._impl;
  if (!impl) {
    return;
  }

  if (event == ESP_GAP_BLE_SET_LOCAL_PRIVACY_COMPLETE_EVT) {
    BTStatus st = (param->local_privacy_cmpl.status == ESP_BT_STATUS_SUCCESS) ? BTStatus::OK : BTStatus::Fail;
    impl->privacySync.give(st);
  }

  if (event == ESP_GAP_BLE_UPDATE_WHITELIST_COMPLETE_EVT) {
    BTStatus st = (param->update_whitelist_cmpl.status == ESP_BT_STATUS_SUCCESS) ? BTStatus::OK : BTStatus::Fail;
    if (st != BTStatus::OK) {
      log_w("Whitelist update complete status=%d op=%d", param->update_whitelist_cmpl.status, param->update_whitelist_cmpl.wl_operation);
    }
    impl->whitelistSync.give(st);
  }

#if BLE_GATT_SERVER_SUPPORTED
  BLEServer::Impl::handleGAP(event, param);
#endif
#if BLE_ADVERTISING_SUPPORTED
  BLEAdvertising::Impl::handleGAP(event, param);
#endif
#if BLE_SCANNING_SUPPORTED
  BLEScan::Impl::handleGAP(event, param);
#endif
#if BLE_GATT_CLIENT_SUPPORTED
  BLEClient::Impl::handleGAP(event, param);
#endif
#if BLE_SMP_SUPPORTED
  bluedroidSecurityHandleGAP(event, param);
#endif

  if (impl->customGapHandler) {
    impl->customGapHandler(reinterpret_cast<void *>(&event), param);
  }
}

#if BLE_GATT_SERVER_SUPPORTED
/**
 * @brief Forwards GATTS events to the server stack and the optional custom GATTS `RawEventHandler`.
 * @param event GATTS event code.
 * @param gatts_if GATTS application interface index.
 * @param param GATTS callback parameters.
 */
void BLEClass::Impl::gattsCallback(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
  auto *impl = BLE._impl;
  if (!impl) {
    return;
  }

  BLEServer::Impl::handleGATTS(event, gatts_if, param);

  if (impl->customGattsHandler) {
    impl->customGattsHandler(reinterpret_cast<void *>(&event), param);
  }
}
#endif

#if BLE_GATT_CLIENT_SUPPORTED
/**
 * @brief Forwards GATTC events to the client stack and the optional custom GATTC `RawEventHandler`.
 * @param event GATTC event code.
 * @param gattc_if GATTC application interface index.
 * @param param GATTC callback parameters.
 */
void BLEClass::Impl::gattcCallback(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) {
  auto *impl = BLE._impl;
  if (!impl) {
    return;
  }

  BLEClient::Impl::handleGATTC(event, gattc_if, param);

  if (impl->customGattcHandler) {
    impl->customGattcHandler(reinterpret_cast<void *>(&event), param);
  }
}
#endif

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

// Fails with InvalidState if a prior end(true) already released controller memory; that state blocks re-begin in-process.
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

  log_i("Initializing BLE stack: Bluedroid");

  if (!btStart()) {
    log_e("btStart() failed");
    return BTStatus::Fail;
  }

  esp_err_t err = esp_bluedroid_init();
  if (err != ESP_OK) {
    log_e("esp_bluedroid_init: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }

  err = esp_bluedroid_enable();
  if (err != ESP_OK) {
    log_e("esp_bluedroid_enable: %s", esp_err_to_name(err));
    esp_bluedroid_deinit();
    return BTStatus::Fail;
  }

  err = esp_ble_gap_register_callback(BLEClass::Impl::gapCallback);
  if (err != ESP_OK) {
    log_e("esp_ble_gap_register_callback: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }

#if BLE_GATT_SERVER_SUPPORTED
  err = esp_ble_gatts_register_callback(BLEClass::Impl::gattsCallback);
  if (err != ESP_OK) {
    log_e("esp_ble_gatts_register_callback: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }
#endif

#if BLE_GATT_CLIENT_SUPPORTED
  err = esp_ble_gattc_register_callback(BLEClass::Impl::gattcCallback);
  if (err != ESP_OK) {
    log_e("esp_ble_gattc_register_callback: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }
#endif

  _deviceName = deviceName;
  if (deviceName.length() > 0) {
    esp_ble_gap_set_device_name(deviceName.c_str());
  }

  if (_impl->localMTU != 23) {
    esp_ble_gatt_set_local_mtu(_impl->localMTU);
  }

  _initialized = true;
  _ownAddressType = static_cast<BTAddress::Type>(_impl->ownAddrType);
  return BTStatus::OK;
}

// Irreversible: after end(true) BLE memory is freed via btMemRelease() and begin() cannot succeed again without a hardware reset.
void BLEClass::end(bool releaseMemory) {
  if (!_initialized) {
    return;
  }

#if BLE_GATT_SERVER_SUPPORTED
  BLEServer::Impl::invalidate();
#endif
  // Reset advertising state before tearing down the stack.  The Bluedroid
  // stack is still running here, so the public reset() can call stop() through
  // the normal async path (esp_ble_gap_stop_advertising + wait), then wipe all
  // accumulated configuration (serviceUUIDs, appearance, etc.).
#if BLE_ADVERTISING_SUPPORTED
  getAdvertising().reset();
#endif
  // Stop any active scan while the stack is still enabled so a late scan-result
  // callback cannot fire into the scan handler after esp_bluedroid_disable().
#if BLE_SCANNING_SUPPORTED
  if (getScan().isScanning()) {
    getScan().stop();
  }
  // Tear down periodic syncs while the host is still enabled (DESIGN.md contract).
  getScan().terminateAllPeriodicSyncs();
#endif

  esp_bluedroid_disable();
  esp_bluedroid_deinit();

  esp_bt_controller_disable();
  esp_bt_controller_deinit();

  if (releaseMemory) {
    btMemRelease(BT_MODE_BLE);
  }

  _initialized = false;
}

// --------------------------------------------------------------------------
// Identity
// --------------------------------------------------------------------------

BTAddress BLEClass::getAddress() const {
  if (!_initialized) {
    return BTAddress();
  }
  const uint8_t *addr = esp_bt_dev_get_address();
  if (!addr) {
    return BTAddress();
  }
  return BTAddress(addr, BTAddress::Type::Public);
}

// Toggles esp_ble_gap_config_local_privacy only when the desired privacy level differs from the current state; uses a 2 s privacySync wait.
BTStatus BLEClass::setOwnAddressType(BTAddress::Type type) {
  if (!_impl || !_initialized) {
    return BTStatus::NotInitialized;
  }
  bool privacyNeeded = (type != BTAddress::Type::Public);
  bool privacyCurrent = (_impl->ownAddrType != BLE_ADDR_TYPE_PUBLIC);
  if (privacyNeeded != privacyCurrent) {
    _impl->privacySync.take();
    esp_err_t err = esp_ble_gap_config_local_privacy(privacyNeeded);
    if (err != ESP_OK) {
      _impl->privacySync.give(BTStatus::Fail);
      return BTStatus::Fail;
    }
    BTStatus st = _impl->privacySync.wait(2000);
    if (st != BTStatus::OK) {
      log_e("setOwnAddressType: privacy config timeout");
      return st;
    }
  }
  _impl->ownAddrType = static_cast<uint8_t>(type);
  _ownAddressType = type;
  return BTStatus::OK;
}

// Setting a random static identity is not implemented on the Bluedroid path; always returns NotSupported.
BTStatus BLEClass::setOwnAddress(const BTAddress & /*addr*/) {
  log_w("setOwnAddress not supported on Bluedroid");
  return BTStatus::NotSupported;
}

// --------------------------------------------------------------------------
// MTU
// --------------------------------------------------------------------------

BTStatus BLEClass::setMTU(uint16_t mtu) {
  if (!_impl || !_initialized) {
    return BTStatus::NotInitialized;
  }
  esp_err_t err = esp_ble_gatt_set_local_mtu(mtu);
  if (err != ESP_OK) {
    log_e("setMTU: esp_ble_gatt_set_local_mtu(%u): %s", mtu, esp_err_to_name(err));
    return BTStatus::InvalidParam;
  }
  _impl->localMTU = mtu;
  return BTStatus::OK;
}

uint16_t BLEClass::getMTU() const {
  return _impl ? _impl->localMTU : 23;
}

// --------------------------------------------------------------------------
// IRK (Bluedroid uses different bond storage API)
// --------------------------------------------------------------------------

// Uses the Espressif Bluedroid helper esp_ble_gap_get_local_irk rather than NimBLE store iteration.
bool BLEClass::getLocalIRK(uint8_t irk[16]) const {
  if (!_impl || !_initialized || !irk) {
    return false;
  }
  esp_err_t ret = esp_ble_gap_get_local_irk(irk);
  if (ret != ESP_OK) {
    log_e("esp_ble_gap_get_local_irk: %s", esp_err_to_name(ret));
    return false;
  }
  return true;
}

// Skips entries with an all-zero IRK; compares both the bond connection address and pid_key.static_addr to peer as public addresses.
bool BLEClass::getPeerIRK(const BTAddress &peer, uint8_t irk[16]) const {
  if (!_impl || !_initialized || !irk) {
    return false;
  }

  int num = esp_ble_get_bond_device_num();
  if (num <= 0) {
    return false;
  }

  std::vector<esp_ble_bond_dev_t> devList(num);
  esp_err_t err = esp_ble_get_bond_device_list(&num, devList.data());
  if (err != ESP_OK) {
    return false;
  }

  for (int i = 0; i < num; i++) {
    if (!(devList[i].bond_key.key_mask & ESP_LE_KEY_PID)) {
      continue;
    }

    BTAddress bondAddr(devList[i].bd_addr, BTAddress::Type::Public);
    BTAddress identityAddr(devList[i].bond_key.pid_key.static_addr, BTAddress::Type::Public);

    if (bondAddr == peer || identityAddr == peer) {
      bool nonzero = false;
      for (int k = 0; k < 16; k++) {
        if (devList[i].bond_key.pid_key.irk[k] != 0) {
          nonzero = true;
          break;
        }
      }
      if (!nonzero) {
        continue;
      }
      memcpy(irk, devList[i].bond_key.pid_key.irk, 16);
      return true;
    }
  }
  return false;
}

// --------------------------------------------------------------------------
// Default PHY (BLE 5.0)
// --------------------------------------------------------------------------

// Only available when BLE5_SUPPORTED is on; older targets return NotSupported.
// Bluedroid takes preferred-PHY *masks* (not the single-PHY enum values NimBLE uses).
BTStatus BLEClass::setDefaultPhy(BLEPhy txPhy, BLEPhy rxPhy) {
#if BLE5_SUPPORTED
  if (!_impl || !_initialized) {
    return BTStatus::NotInitialized;
  }
  esp_err_t err = esp_ble_gap_set_preferred_default_phy(blePhyToPrefMask(txPhy), blePhyToPrefMask(rxPhy));
  if (err != ESP_OK) {
    log_e("setDefaultPhy: esp_ble_gap_set_preferred_default_phy: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }
  return BTStatus::OK;
#else
  log_w("setDefaultPhy not supported (BLE 5.0 unavailable)");
  return BTStatus::NotSupported;
#endif
}

// Not implemented on the Bluedroid path; out-parameters are still initialized to PHY_1M so callers that ignore the NotSupported return never read uninitialized data.
BTStatus BLEClass::getDefaultPhy(BLEPhy &txPhy, BLEPhy &rxPhy) const {
  txPhy = BLEPhy::PHY_1M;
  rxPhy = BLEPhy::PHY_1M;
  log_w("getDefaultPhy not supported on Bluedroid");
  return BTStatus::NotSupported;
}

// --------------------------------------------------------------------------
// Whitelist
// --------------------------------------------------------------------------

// The GAP update must succeed (and complete) before _whiteList is updated; a
// failed call leaves the in-memory list unchanged. esp_ble_gap_update_whitelist
// is async — wait on UPDATE_WHITELIST_COMPLETE_EVT so the public API is
// synchronous (matching NimBLE's ble_gap_wl_set).
BTStatus BLEClass::whiteListAdd(const BTAddress &address) {
  if (!_initialized) {
    return BTStatus::NotInitialized;
  }
  if (!_impl) {
    return BTStatus::InvalidState;
  }
  esp_bd_addr_t bda;
  address.toEspBdAddr(bda);
  _impl->whitelistSync.take();
  esp_err_t err = esp_ble_gap_update_whitelist(true, bda, static_cast<esp_ble_wl_addr_type_t>(address.type()));
  if (err != ESP_OK) {
    log_e("whiteListAdd: esp_ble_gap_update_whitelist: %s", esp_err_to_name(err));
    _impl->whitelistSync.give(BTStatus::Fail);
    return BTStatus::Fail;
  }
  BTStatus st = _impl->whitelistSync.wait(5000);
  if (st != BTStatus::OK) {
    log_e("whiteListAdd: whitelist update did not complete: %s", st.toString());
    return st;
  }
  _whiteList.push_back(address);
  return BTStatus::OK;
}

// Local _whiteList is pruned only after a successful GAP completion.
BTStatus BLEClass::whiteListRemove(const BTAddress &address) {
  if (!_initialized) {
    return BTStatus::NotInitialized;
  }
  if (!_impl) {
    return BTStatus::InvalidState;
  }
  esp_bd_addr_t bda;
  address.toEspBdAddr(bda);
  _impl->whitelistSync.take();
  esp_err_t err = esp_ble_gap_update_whitelist(false, bda, static_cast<esp_ble_wl_addr_type_t>(address.type()));
  if (err != ESP_OK) {
    log_e("whiteListRemove: esp_ble_gap_update_whitelist: %s", esp_err_to_name(err));
    _impl->whitelistSync.give(BTStatus::Fail);
    return BTStatus::Fail;
  }
  BTStatus st = _impl->whitelistSync.wait(5000);
  if (st != BTStatus::OK) {
    log_e("whiteListRemove: whitelist update did not complete: %s", st.toString());
    return st;
  }
  for (auto it = _whiteList.begin(); it != _whiteList.end(); ++it) {
    if (*it == address) {
      _whiteList.erase(it);
      break;
    }
  }
  return BTStatus::OK;
}

// --------------------------------------------------------------------------
// Stack info
// --------------------------------------------------------------------------

BLEClass::Stack BLEClass::getStack() const {
  return Stack::Bluedroid;
}

const char *BLEClass::getStackName() const {
  return "Bluedroid";
}

// Always false: Bluedroid in this build uses the local controller, not the hosted co-processor path (contrast with NimBLE hosted builds, where setPins matters).
bool BLEClass::isHostedBLE() const {
  return false;
}

// Present only for API symmetry with hosted NimBLE; the Bluedroid in-tree path ignores the pins and always returns NotSupported.
BTStatus BLEClass::setPins(int8_t, int8_t, int8_t, int8_t, int8_t, int8_t, int8_t) {
  log_w("setPins not supported on Bluedroid");
  return BTStatus::NotSupported;
}

// --------------------------------------------------------------------------
// Custom event handlers
// --------------------------------------------------------------------------

// The handler is invoked from gapCallback after the built-in GAP subsystems and security handling.
BTStatus BLEClass::setCustomGapHandler(RawEventHandler handler) {
  if (!_impl || !_initialized) {
    return BTStatus::NotInitialized;
  }
  _impl->customGapHandler = handler;
  return BTStatus::OK;
}

// The handler is invoked from gattcCallback after the client handleGATTC logic.
BTStatus BLEClass::setCustomGattcHandler(RawEventHandler handler) {
  if (!_impl || !_initialized) {
    return BTStatus::NotInitialized;
  }
  _impl->customGattcHandler = handler;
  return BTStatus::OK;
}

// The handler is invoked from gattsCallback after the server handleGATTS logic.
BTStatus BLEClass::setCustomGattsHandler(RawEventHandler handler) {
  if (!_impl || !_initialized) {
    return BTStatus::NotInitialized;
  }
  _impl->customGattsHandler = handler;
  return BTStatus::OK;
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
    BLEScan::Impl::s_instance = slot.get();
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
    BLEAdvertising::Impl::s_instance = slot.get();
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
// A successful create isn't the end of the story here -- GATTS app registration
// with the stack is asynchronous, so an existing slot may still be waiting on
// its `gattsIf`. Every call re-checks that and retries registration (and drops
// the slot on repeated failure) instead of a strict once-and-done create.
BLEServer BLEClass::createServer() {
  static std::shared_ptr<BLEServer::Impl> slot;
  if (!isInitialized()) {
    log_e("createServer: BLE not initialized");
    return BLEServer();
  }
  if (!slot) {
    auto fresh = std::make_shared<BLEServer::Impl>();
    BLEServer::Impl::s_instance = fresh.get();
    esp_timer_create_args_t timerArgs = {};
    timerArgs.callback = [](void *) { BLE.startAdvertising(); };
    timerArgs.name = "ble_adv_restart";
    esp_timer_create(&timerArgs, &fresh->advRestartTimer);
    if (!bluedroidRegisterGattsApp(fresh)) {
      BLEServer::Impl::s_instance = nullptr;
      return BLEServer();
    }
    slot = fresh;
  } else if (slot->gattsIf == ESP_GATT_IF_NONE) {
    if (!bluedroidRegisterGattsApp(slot)) {
      BLEServer::Impl::s_instance = nullptr;
      slot.reset();
      return BLEServer();
    }
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
  auto impl = std::make_shared<BLEClient::Impl>();
  if (!bluedroidRegisterClient(impl)) {
    return BLEClient();
  }
  return BLEClient(impl);
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
    slot->applySecurityParams();
  }
  return BLESecurity(slot);
}
#else
BLESecurity BLEClass::getSecurity() {
  log_w("SMP not supported");
  return BLESecurity();
}
#endif

#endif /* BLE_BLUEDROID */
