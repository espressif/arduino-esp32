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

#include "impl/BLEGuards.h"
#if BLE_NIMBLE

#include "BLE.h"

#include "impl/BLEImplHelpers.h"
#include "impl/BLESecurityBackend.h"
#include "esp32-hal-bt.h"
#include "esp32-hal-bt-mem.h"
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

// --------------------------------------------------------------------------
// BLEClass::Impl -- NimBLE backend state
// --------------------------------------------------------------------------

struct BLEClass::Impl {
  bool synced = false;
  uint16_t localMTU = BLE_ATT_MTU_DFLT;
  uint8_t ownAddrType = BLE_OWN_ADDR_PUBLIC;
  ble_gap_event_listener gapListener{};
  BLEClass::RawEventHandler customGapHandler = nullptr;

  /**
   * @brief FreeRTOS task body that runs the NimBLE host until the port is stopped.
   * @param param Unused task context pointer.
   */
  static void hostTask(void *param) {
    log_i("NimBLE host task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
  }

  /**
   * @brief Notifies the implementation that the host stack reset; clears sync and logs the reason.
   * @param reason Host reset reason from NimBLE.
   */
  static void onReset(int reason) {
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
  static void onSync() {
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

  /**
   * @brief Handles bond store full events; may ask security to evict a peer before using default store rotation.
   * @param event NimBLE store status event.
   * @param arg Opaque callback context from NimBLE.
   * @return 0 to consume overflow handling after bond notification, or the result of `ble_store_util_status_rr`.
   */
  static int onStoreStatus(struct ble_store_status_event *event, void *arg) {
    if (event->event_code == BLE_STORE_EVENT_FULL) {
      BLESecurity sec = BLE.getSecurity();
      if (sec) {
        struct ble_store_key_sec key = {};
        struct ble_store_value_sec value = {};
        key.idx = 0;
        if (ble_store_read_peer_sec(&key, &value) == 0) {
          BTAddress oldest(value.peer_addr.val, static_cast<BTAddress::Type>(value.peer_addr.type));
          if (BLESecurityBackend::notifyBondOverflow(sec, oldest)) {
            return 0;
          }
        }
      }
    }
    return ble_store_util_status_rr(event, arg);
  }
};

// --------------------------------------------------------------------------
// BLEClass lifecycle
// --------------------------------------------------------------------------

/**
 * @brief Allocates the NimBLE `Impl` state; stack init is deferred to `begin()`.
 */
BLEClass::BLEClass() : _impl(new Impl()) {}

/**
 * @brief Stops the stack if still initialized, then releases `Impl` storage.
 */
BLEClass::~BLEClass() {
  if (_initialized) {
    end(false);
  }
  delete _impl;
  _impl = nullptr;
}

/**
 * @brief Initializes the NimBLE port, host configuration, and waits until the host is synchronized.
 * @param deviceName GAP local name set after sync when non-empty.
 * @return `BTStatus::OK` on success, or a failure, timeout, or invalid-state code.
 * @note When `CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE` is set, `hostedInitBLE()` runs first; a failed init returns `BTStatus::Fail`.
 * @note If a prior `end(true)` released controller memory, re-initialization is rejected with `BTStatus::InvalidState`.
 */
BTStatus BLEClass::begin(const String &deviceName) {
  if (_initialized) {
    return BTStatus::OK;
  }

  if (_memoryReleased) {
    log_e("Cannot reinitialize BLE: memory was permanently released by end(true)");
    return BTStatus::InvalidState;
  }

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

  ble_hs_cfg.reset_cb = Impl::onReset;
  ble_hs_cfg.sync_cb = Impl::onSync;
  ble_hs_cfg.store_status_cb = Impl::onStoreStatus;
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
  nimble_port_freertos_init(Impl::hostTask);

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

/**
 * @brief Stops advertising, scanning, the NimBLE port, and optionally the controller, releasing heap when requested.
 * @param releaseMemory When true, call `esp_bt_controller_mem_release` and set `_memoryReleased` so `begin()` cannot be used again.
 * @note `hostedDeinitBLE()` runs when `CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE` is set, after the NimBLE port is torn down.
 * @note Permanent memory release via `releaseMemory` is irreversible for a later in-process `begin()`.
 */
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
#endif

  nimble_port_stop();
  nimble_port_deinit();

#if defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
  hostedDeinitBLE();
#endif

#if SOC_BLE_SUPPORTED && CONFIG_BT_CONTROLLER_ENABLED
  esp_bt_controller_disable();
  esp_bt_controller_deinit();
#endif

  if (releaseMemory) {
#if SOC_BLE_SUPPORTED && CONFIG_BT_CONTROLLER_ENABLED
    esp_bt_controller_mem_release(ESP_BT_MODE_BTDM);
#endif
    _memoryReleased = true;
  }

  _impl->synced = false;
  _initialized = false;
}

// --------------------------------------------------------------------------
// Identity
// --------------------------------------------------------------------------

/**
 * @brief Returns the current local address using the cached NimBLE own-address type and `ble_hs_id_copy_addr`.
 * @return A `BTAddress` for the public or random type in use, or a default/empty address if uninitialized or on error.
 */
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

/**
 * @brief Validates and caches the own-address type, enabling RPA on original ESP32 when a default RPA type is used.
 * @param type Requested `BTAddress::Type` to use for the local identity.
 * @return `BTStatus::OK` on success, or not-initialized, invalid parameter, or related error codes.
 * @note On `CONFIG_IDF_TARGET_ESP32`, RPA default types map to a random public type and `ble_hs_pvcy_rpa_config` is toggled; other types disable host privacy in that build.
 */
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

/**
 * @brief Sets a random static address via the NimBLE host.
 * @param addr Six-byte address to use as the random static identity.
 * @return `BTStatus::OK` on success, or not-initialized/fail on `ble_hs_id_set_rnd` error.
 */
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

/**
 * @brief Sets the preferred local ATT MTU.
 * @param mtu Desired maximum transmission unit in bytes.
 * @return `BTStatus::OK` on success, or not-initialized/invalid-param if the call fails.
 */
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

/**
 * @brief Returns the cached local MTU or the NimBLE default when not initialized.
 * @return Current preferred MTU, or `BLE_ATT_MTU_DFLT` if there is no implementation.
 */
uint16_t BLEClass::getMTU() const {
  return _impl ? _impl->localMTU : BLE_ATT_MTU_DFLT;
}

// --------------------------------------------------------------------------
// IRK
// --------------------------------------------------------------------------

/**
 * @brief Reads the local identity resolving key from the configured NimBLE bond store.
 * @param irk Receives 16 bytes when an IRK is present.
 * @return True if a local IRK was read; false if uninitialized, null buffer, or no IRK in store.
 * @note Uses `ble_store_read_our_sec` index 0; the IRK is only present when the store entry includes it.
 */
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

/**
 * @brief Looks up a bonded peer by iterating `ble_store_read_peer_sec` and matching the address with `irk_present`.
 * @param peer Address to find among stored peer bond entries.
 * @param irk Receives 16 bytes for the first matching entry that carries an IRK.
 * @return True on a match; false if no suitable bond or buffer is invalid.
 * @note The scan is capped at 100 bonds to avoid unbounded store iteration.
 */
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

/**
 * @brief Sets the controller default TX/RX LE PHY preferences when BLE 5.0 is supported.
 * @param txPhy Bitmask for preferred transmit PHY.
 * @param rxPhy Bitmask for preferred receive PHY.
 * @return `BTStatus::OK` on success, or not-initialized, fail, or not-supported.
 */
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

/**
 * @brief Not implemented on NimBLE; parameters are not updated.
 * @param txPhy Unused; not written.
 * @param rxPhy Unused; not written.
 * @return Always `BTStatus::NotSupported` on this backend.
 */
BTStatus BLEClass::getDefaultPhy(BLEPhy &txPhy, BLEPhy &rxPhy) const {
  log_w("getDefaultPhy not supported on NimBLE");
  return BTStatus::NotSupported;
}

// --------------------------------------------------------------------------
// Whitelist
// --------------------------------------------------------------------------

/**
 * @brief Adds an address to the in-memory list and programs the full filter via `ble_gap_wl_set`.
 * @param address Address and type to add as a new whitelist entry.
 * @return `BTStatus::OK` if already present or on success, or fail/invalid on controller errors; rolls back the vector on `ble_gap_wl_set` failure.
 * @note The entire list is re-sent to the stack on every change; duplicate add is a no-op to the driver.
 */
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

/**
 * @brief Removes one matching entry, then reapplies the remaining set with `ble_gap_wl_set`.
 * @param address The entry to remove from the tracked list and controller filter.
 * @return `BTStatus::OK` on success, not-found if absent, or fail on driver errors; restores the entry on a failed reapply.
 * @note Like add, the whole whitelist array is set atomically in one `ble_gap_wl_set` call.
 */
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

/**
 * @brief Returns the `Stack::NimBLE` enum value for the active build.
 * @return `BLEClass::Stack::NimBLE`.
 */
BLEClass::Stack BLEClass::getStack() const {
  return Stack::NimBLE;
}

/**
 * @brief Returns a string identifier for the NimBLE stack.
 * @return The literal "NimBLE".
 */
const char *BLEClass::getStackName() const {
  return "NimBLE";
}

/**
 * @brief Reports whether the build uses ESP-Hosted for the NimBLE path.
 * @return True if `CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE` is defined; otherwise false.
 * @note When true, `begin`/`end` and optional `setPins` call into the hosted co-processor path.
 */
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

/**
 * @brief Configures SPI/reset pins for ESP-Hosted BLE; no-op on builds without hosted NimBLE.
 * @param clk Host clock pin.
 * @param cmd Command pin.
 * @param d0 First data line.
 * @param d1 Second data line.
 * @param d2 Third data line.
 * @param d3 Fourth data line.
 * @param rst Optional reset line.
 * @return `BTStatus::OK` when hosted support is on and `hostedSetPins` succeeds, fail on pin setup error, or not-supported.
 * @note Only compiled with `CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE`; should be set before `begin` if the hosted transport needs reconfiguration.
 */
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

/**
 * @brief Registers a GAP event listener that forwards GAP events to a custom `RawEventHandler`.
 * @param handler Callback, or `nullptr` to allow clearing depending on use; listener wraps calls through `ble_gap_event_listener`.
 * @return `BTStatus::OK` on success (including if already registered), or not-initialized/fail on `ble_gap_event_listener_register` error.
 */
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

/**
 * @brief GATT client raw hook is not implemented for NimBLE; use the NimBLE client path instead.
 * @param handler Unused; no registration performed.
 * @return Always `BTStatus::NotSupported` on this backend.
 */
BTStatus BLEClass::setCustomGattcHandler(RawEventHandler /*handler*/) {
  log_w("setCustomGattcHandler not supported on NimBLE");
  return BTStatus::NotSupported;
}

/**
 * @brief GATT server raw hook is not implemented for NimBLE; use the NimBLE server path instead.
 * @param handler Unused; no registration performed.
 * @return Always `BTStatus::NotSupported` on this backend.
 */
BTStatus BLEClass::setCustomGattsHandler(RawEventHandler /*handler*/) {
  log_w("setCustomGattsHandler not supported on NimBLE");
  return BTStatus::NotSupported;
}

// Factory methods in their respective impl files:
// createServer() -> NimBLEServer.cpp, getAdvertising/startAdvertising/stopAdvertising -> NimBLEAdvertising.cpp
// getSecurity() -> NimBLESecurity.cpp, createClient() -> NimBLEClient.cpp, getScan() -> NimBLEScan.cpp

#endif /* BLE_NIMBLE */
