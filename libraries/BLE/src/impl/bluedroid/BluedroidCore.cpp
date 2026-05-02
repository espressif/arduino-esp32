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

#include "impl/BLEGuards.h"
#if BLE_BLUEDROID

#include "BLE.h"
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
#include "impl/BLEImplHelpers.h"
#include "esp32-hal-bt.h"
#include "esp32-hal-bt-mem.h"
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

#include "impl/BLESync.h"

#if BLE_SMP_SUPPORTED
// Defined in BluedroidSecurity.cpp
void bluedroidSecurityHandleGAP(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
#endif

// --------------------------------------------------------------------------
// BLEClass::Impl -- Bluedroid backend state
// --------------------------------------------------------------------------

struct BLEClass::Impl {
  uint16_t localMTU = 23;
  uint8_t ownAddrType = BLE_ADDR_TYPE_PUBLIC;
  BLESync privacySync;
  BLEClass::RawEventHandler customGapHandler = nullptr;
  BLEClass::RawEventHandler customGattcHandler = nullptr;
  BLEClass::RawEventHandler customGattsHandler = nullptr;

  static void gapCallback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
#if BLE_GATT_SERVER_SUPPORTED
  static void gattsCallback(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
#endif
#if BLE_GATT_CLIENT_SUPPORTED
  static void gattcCallback(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
#endif
};

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

/**
 * @brief Allocates the Bluedroid `Impl` state; `begin()` later enables the host.
 */
BLEClass::BLEClass() : _impl(new Impl()) {}

/**
 * @brief Calls `end(false)` if initialized, then deletes `Impl`.
 */
BLEClass::~BLEClass() {
  if (_initialized) {
    end(false);
  }
  delete _impl;
  _impl = nullptr;
}

/**
 * @brief Starts the BT controller and Bluedroid, registers GAP and optional GATTS/GATTC callbacks, and sets name and MTU.
 * @param deviceName Optional name passed to `esp_ble_gap_set_device_name` when non-empty.
 * @return `BTStatus::OK` on success, or a failure/invalid state when the stack cannot be brought up.
 * @note Fails with `InvalidState` if a prior `end(true)` already released controller memory; that state blocks re-`begin` in-process.
 */
BTStatus BLEClass::begin(const String &deviceName) {
  if (_initialized) {
    return BTStatus::OK;
  }

  if (_memoryReleased) {
    log_e("Cannot reinitialize BLE: memory was permanently released by end(true)");
    return BTStatus::InvalidState;
  }

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

  err = esp_ble_gap_register_callback(Impl::gapCallback);
  if (err != ESP_OK) {
    log_e("esp_ble_gap_register_callback: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }

#if BLE_GATT_SERVER_SUPPORTED
  err = esp_ble_gatts_register_callback(Impl::gattsCallback);
  if (err != ESP_OK) {
    log_e("esp_ble_gatts_register_callback: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }
#endif

#if BLE_GATT_CLIENT_SUPPORTED
  err = esp_ble_gattc_register_callback(Impl::gattcCallback);
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

/**
 * @brief Disables Bluedroid and the controller, optionally calling `esp_bt_controller_mem_release` to free heap.
 * @param releaseMemory When true, memory is not reclaimable for a new `begin()` in the same process; `_memoryReleased` is set.
 * @note Irreversible controller memory release: after `end(true)` you cannot call `begin()` again without a reset that restores heap mode.
 */
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
  // accumulated configuration (serviceUUIDs, appearance, etc.).  This is the
  // same pattern used by the NimBLE backend and makes both consistent.
#if BLE_ADVERTISING_SUPPORTED
  getAdvertising().reset();
#endif

  esp_bluedroid_disable();
  esp_bluedroid_deinit();

  esp_bt_controller_disable();
  esp_bt_controller_deinit();

  if (releaseMemory) {
    esp_bt_controller_mem_release(ESP_BT_MODE_BTDM);
    _memoryReleased = true;
  }

  _initialized = false;
}

// --------------------------------------------------------------------------
// Identity
// --------------------------------------------------------------------------

/**
 * @brief Returns the public controller address from `esp_bt_dev_get_address`.
 * @return A public `BTAddress` when initialized, or an empty address when not ready or the pointer is null.
 */
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

/**
 * @brief Reconfigures local random/private addressing via GAP and waits for privacy callback completion.
 * @param type Desired public vs privacy-oriented address type for the class cache.
 * @return `BTStatus::OK` on success, not-initialized, fail on `esp_ble_gap_config_local_privacy` error, or a timeout/invalid status from `BLESync::wait`.
 * @note Toggles `esp_ble_gap_config_local_privacy` only when the desired privacy level differs from the current state; uses a 2 s `privacySync` wait.
 */
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

/**
 * @brief Random static set-address is not available on the Bluedroid path in this API.
 * @param addr Ignored; setting a random static identity is not implemented here.
 * @return `BTStatus::NotSupported` always.
 */
BTStatus BLEClass::setOwnAddress(const BTAddress & /*addr*/) {
  log_w("setOwnAddress not supported on Bluedroid");
  return BTStatus::NotSupported;
}

// --------------------------------------------------------------------------
// MTU
// --------------------------------------------------------------------------

/**
 * @brief Sets the local GATT MTU in Bluedroid and caches the value in `Impl`.
 * @param mtu Requested local MTU in bytes.
 * @return `BTStatus::OK` on success, or not-initialized/invalid-param on `esp_ble_gatt_set_local_mtu` error.
 */
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

/**
 * @brief Returns the last successfully set local MTU or 23 as default.
 * @return Cached `localMTU` or 23 if there is no implementation.
 */
uint16_t BLEClass::getMTU() const {
  return _impl ? _impl->localMTU : 23;
}

// --------------------------------------------------------------------------
// IRK (Bluedroid uses different bond storage API)
// --------------------------------------------------------------------------

/**
 * @brief Retrieves the local IRK from the GAP/Bluedroid bond key interface.
 * @param irk 16-byte output for the identity key.
 * @return True on `ESP_OK` from `esp_ble_gap_get_local_irk`; false on null buffer, not initialized, or API error.
 * @note This path uses the Espressif Bluedroid helper rather than NimBLE store iteration.
 */
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

/**
 * @brief Looks up a bonded device by public or static identity and copies its PID key IRK when `ESP_LE_KEY_PID` and IRK are non-zero.
 * @param peer Address to match against the bond `bd_addr` or the PID static identity.
 * @param irk 16-byte output for the first matching peer.
 * @return True when a non-zero IRK is found; false for empty list, no PID key, or failed list fetch.
 * @note Skips entries with an all-zero IRK; compares both connection address and `pid_key.static_addr` to `peer` as public addresses.
 */
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

/**
 * @brief Configures the default preferred LE PHY when `BLE5_SUPPORTED` is on.
 * @param txPhy Preferred default transmit PHY flags.
 * @param rxPhy Preferred default receive PHY flags.
 * @return `BTStatus::OK` in the supported path, or not-initialized, or not-supported on older targets.
 */
BTStatus BLEClass::setDefaultPhy(BLEPhy txPhy, BLEPhy rxPhy) {
#if BLE5_SUPPORTED
  if (!_impl || !_initialized) {
    return BTStatus::NotInitialized;
  }
  esp_ble_gap_set_prefered_default_le_phy(static_cast<uint8_t>(txPhy), static_cast<uint8_t>(rxPhy));
  return BTStatus::OK;
#else
  log_w("setDefaultPhy not supported (BLE 5.0 unavailable)");
  return BTStatus::NotSupported;
#endif
}

/**
 * @brief Get-default-PHY is not implemented on the Bluedroid path.
 * @param txPhy Out parameter not written.
 * @param rxPhy Out parameter not written.
 * @return Always `BTStatus::NotSupported` here.
 */
BTStatus BLEClass::getDefaultPhy(BLEPhy & /*txPhy*/, BLEPhy & /*rxPhy*/) const {
  log_w("getDefaultPhy not supported on Bluedroid");
  return BTStatus::NotSupported;
}

// --------------------------------------------------------------------------
// Whitelist
// --------------------------------------------------------------------------

/**
 * @brief Adds a device to the GAP filter whitelist and mirrors it in the class vector on success.
 * @param address Six-byte and type to pass to `esp_ble_gap_update_whitelist` with add=true.
 * @return `BTStatus::OK` on success, not-initialized, or fail on the stack error path.
 * @note The driver update must succeed before `_whiteList` is updated; a failed call leaves the in-memory list unchanged.
 */
BTStatus BLEClass::whiteListAdd(const BTAddress &address) {
  if (!_initialized) {
    return BTStatus::NotInitialized;
  }
  esp_bd_addr_t bda;
  memcpy(bda, address.data(), 6);
  esp_err_t err = esp_ble_gap_update_whitelist(true, bda, static_cast<esp_ble_wl_addr_type_t>(address.type()));
  if (err == ESP_OK) {
    _whiteList.push_back(address);
    return BTStatus::OK;
  }
  log_e("whiteListAdd: esp_ble_gap_update_whitelist: %s", esp_err_to_name(err));
  return BTStatus::Fail;
}

/**
 * @brief Removes a device from the controller filter and, on success, erases it from the tracked list.
 * @param address The same address+type that was whitelisted.
 * @return `BTStatus::OK` on success, not-initialized, or fail when `esp_ble_gap_update_whitelist` rejects the removal.
 * @note Local `_whiteList` is pruned only after a successful GAP call.
 */
BTStatus BLEClass::whiteListRemove(const BTAddress &address) {
  if (!_initialized) {
    return BTStatus::NotInitialized;
  }
  esp_bd_addr_t bda;
  memcpy(bda, address.data(), 6);
  esp_err_t err = esp_ble_gap_update_whitelist(false, bda, static_cast<esp_ble_wl_addr_type_t>(address.type()));
  if (err == ESP_OK) {
    for (auto it = _whiteList.begin(); it != _whiteList.end(); ++it) {
      if (*it == address) {
        _whiteList.erase(it);
        break;
      }
    }
    return BTStatus::OK;
  }
  log_e("whiteListRemove: esp_ble_gap_update_whitelist: %s", esp_err_to_name(err));
  return BTStatus::Fail;
}

// --------------------------------------------------------------------------
// Stack info
// --------------------------------------------------------------------------

/**
 * @brief Returns the Bluedroid stack id enum.
 * @return `Stack::Bluedroid`.
 */
BLEClass::Stack BLEClass::getStack() const {
  return Stack::Bluedroid;
}

/**
 * @brief Human-readable name for the active stack.
 * @return The literal "Bluedroid".
 */
const char *BLEClass::getStackName() const {
  return "Bluedroid";
}

/**
 * @brief Indicates whether the stack runs on ESP-Hosted; always false in this file.
 * @return Always false: Bluedroid in this build uses the local controller, not the hosted co-pro path.
 * @note Contrasts with NimBLE hosted builds; pin wiring via `setPins` is a no-op here.
 */
bool BLEClass::isHostedBLE() const {
  return false;
}

/**
 * @brief SPI/reset pin setup for co-proc BLE is not used on the Bluedroid in-tree path.
 * @return `BTStatus::NotSupported` always; parameters are ignored.
 * @note Present for API symmetry with hosted NimBLE; this backend always returns not-supported.
 */
BTStatus BLEClass::setPins(int8_t, int8_t, int8_t, int8_t, int8_t, int8_t, int8_t) {
  log_w("setPins not supported on Bluedroid");
  return BTStatus::NotSupported;
}

// --------------------------------------------------------------------------
// Custom event handlers
// --------------------------------------------------------------------------

/**
 * @brief Stores a pointer invoked from `gapCallback` after the built-in GAP subsystems and security.
 * @param handler Custom handler, or null to clear.
 * @return `BTStatus::OK` or not-initialized; signature matches other stacks for compatibility.
 */
BTStatus BLEClass::setCustomGapHandler(RawEventHandler handler) {
  if (!_impl || !_initialized) {
    return BTStatus::NotInitialized;
  }
  _impl->customGapHandler = handler;
  return BTStatus::OK;
}

/**
 * @brief Registers a raw callback invoked from `gattcCallback` after the client `handleGATTC` logic.
 * @param handler GATTC raw handler, or null to clear.
 * @return `BTStatus::OK` or not-initialized.
 */
BTStatus BLEClass::setCustomGattcHandler(RawEventHandler handler) {
  if (!_impl || !_initialized) {
    return BTStatus::NotInitialized;
  }
  _impl->customGattcHandler = handler;
  return BTStatus::OK;
}

/**
 * @brief Registers a raw callback invoked from `gattsCallback` after the server `handleGATTS` logic.
 * @param handler GATTS raw handler, or null to clear.
 * @return `BTStatus::OK` or not-initialized.
 */
BTStatus BLEClass::setCustomGattsHandler(RawEventHandler handler) {
  if (!_impl || !_initialized) {
    return BTStatus::NotInitialized;
  }
  _impl->customGattsHandler = handler;
  return BTStatus::OK;
}

#endif /* BLE_BLUEDROID */
