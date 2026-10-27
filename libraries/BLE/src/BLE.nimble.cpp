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
 * @file BLE.nimble.cpp
 * @brief NimBLE backend for `BLEClass`: stack init/teardown, identity, MTU, IRK, whitelist, and GAP hook registration.
 */

#include "core/BLEGuards.h"
#if BLE_NIMBLE

#include "BLE.h"

#include "BLE.nimble.h"
#if BLE_SMP_SUPPORTED
#include "security/BLESecurity.nimble.h"
#endif
#include "core/BLEImplHelpers.h"
#include "core/BLEBackend.h"
#include "core/BLETask.h"
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

using NimBLEGapHook = int (*)(struct ble_gap_event *);
static portMUX_TYPE s_gapHookMux = portMUX_INITIALIZER_UNLOCKED;
#if BLE_GATT_SERVER_SUPPORTED
static NimBLEGapHook s_serverGapEventHook = nullptr;
#endif
#if BLE_SMP_SUPPORTED
static NimBLEGapHook s_securityGapEventHook = nullptr;
#endif

static NimBLEGapHook loadGapHook(NimBLEGapHook &slot) {
  portENTER_CRITICAL(&s_gapHookMux);
  NimBLEGapHook hook = slot;
  portEXIT_CRITICAL(&s_gapHookMux);
  return hook;
}

static void storeGapHook(NimBLEGapHook &slot, NimBLEGapHook hook) {
  portENTER_CRITICAL(&s_gapHookMux);
  slot = hook;
  portEXIT_CRITICAL(&s_gapHookMux);
}

#if BLE_GATT_SERVER_SUPPORTED
// Defined in BLEServer.nimble.cpp: drops the deferred re-advertise event's backing
// reference so it is re-initialized after a stack restart (see the definition
// for the use-after-free this prevents across BLE.end()/begin()).
void nimbleResetReAdvertiseEvent();

#endif

// --------------------------------------------------------------------------
// BLEClass::Impl -- NimBLE backend state (declared in NimBLECore.h)
// --------------------------------------------------------------------------

static TaskHandle_t s_nimbleHostTaskHandle = nullptr;

bool nimbleIsHostTask() {
  TaskHandle_t h = s_nimbleHostTaskHandle;
  return h != nullptr && xTaskGetCurrentTaskHandle() == h;
}

bool bleInStackCallbackTask() {
  return nimbleIsHostTask();
}

void BLEClass::Impl::hostTask(void *param) {
  s_nimbleHostTaskHandle = xTaskGetCurrentTaskHandle();
  log_i("NimBLE host task started");
  nimble_port_run();
  s_nimbleHostTaskHandle = nullptr;
  nimble_port_freertos_deinit();
}

int nimbleForwardServerGapEvent(struct ble_gap_event *event) {
#if BLE_GATT_SERVER_SUPPORTED
  if (auto hook = loadGapHook(s_serverGapEventHook)) {
    return hook(event);
  }
#else
  (void)event;
#endif
  return 0;
}

int nimbleForwardSecurityGapEvent(struct ble_gap_event *event) {
#if BLE_SMP_SUPPORTED
  if (auto hook = loadGapHook(s_securityGapEventHook)) {
    return hook(event);
  }
  if (!event) {
    return 0;
  }
  if (event->type == BLE_GAP_EVENT_REPEAT_PAIRING) {
    struct ble_gap_conn_desc desc;
    if (ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc) != 0) {
      return BLE_GAP_REPEAT_PAIRING_IGNORE;
    }
    ble_store_util_delete_peer(&desc.peer_id_addr);
    return BLE_GAP_REPEAT_PAIRING_RETRY;
  }
#if defined(BLE_GAP_EVENT_AUTHORIZE)
  if (event->type == BLE_GAP_EVENT_AUTHORIZE) {
    log_w(
      "BLESecurity: attribute 0x%04x requires authorization but no handler is installed - denying %s", event->authorize.attr_handle,
      event->authorize.is_read ? "read" : "write"
    );
    event->authorize.out_response = BLE_GAP_AUTHORIZE_REJECT;
  }
#endif
#else
  (void)event;
#endif
  return 0;
}

int BLEClass::Impl::onStoreStatus(struct ble_store_status_event *event, void *arg) {
#if BLE_SMP_SUPPORTED
  auto overflowHook = BLE._impl ? BLE._impl->loadHook(BLE._impl->securityOverflowHook) : nullptr;
  if (event->event_code == BLE_STORE_EVENT_FULL && overflowHook) {
    struct ble_store_key_sec key = {};
    struct ble_store_value_sec value = {};
    key.idx = 0;
    if (ble_store_read_peer_sec(&key, &value) == 0) {
      BTAddress oldest(value.peer_addr.val, static_cast<BTAddress::Type>(value.peer_addr.type));
      if (overflowHook(oldest)) {
        return 0;
      }
    }
  }
#endif
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
  if (isInitialized()) {
    end(false);
  }
  delete _impl;
  _impl = nullptr;
}

// When CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE is set, hostedInitBLE() runs first; a failed init returns BTStatus::Fail.
// If a prior end(true) released controller memory, re-initialization is rejected with BTStatus::InvalidState.
BTStatus BLEClass::begin(const String &deviceName) {
  if (!_impl || !_impl->lifecycleMtx) {
    return BTStatus::NoMemory;
  }
  BLELockGuard lifecycle(_impl ? _impl->lifecycleMtx : nullptr);
  if (_impl->ending) {
    return BTStatus::InvalidState;
  }
  if (isInitialized()) {
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
  ble_hs_cfg.sm_bonding = 1;
  ble_hs_cfg.sm_mitm = 0;
  ble_hs_cfg.sm_sc = 1;
  ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
  ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

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

#if BLE_L2CAP_SUPPORTED
  if (auto hook = _impl->loadHook(_impl->l2capBeginHook)) {
    hook();
  }
#endif
  _impl->ending = false;
  setInitialized(true);
  _ownAddressType = static_cast<BTAddress::Type>(_impl->ownAddrType);
#if BLE_SCANNING_SUPPORTED
  if (auto hook = _impl->loadHook(_impl->scanBeginHook)) {
    hook();
  }
#endif
#if BLE_ADVERTISING_SUPPORTED
  if (auto hook = _impl->loadHook(_impl->advBeginHook)) {
    hook();
  }
#endif
  if (_impl->localMTU != BLE_ATT_MTU_DFLT) {
    (void)ble_att_set_preferred_mtu(_impl->localMTU);
  }

#if BLE_SMP_SUPPORTED
  // The ble_hs_cfg.sm_* defaults above are written on every begin(), but
  // getSecurity() keeps its Impl in a function-local static that outlives an
  // end()/begin() cycle. Re-push the retained configuration so those defaults
  // do not silently overwrite security settings the application already made.
  if (auto hook = _impl->loadHook(_impl->securityBeginHook)) {
    hook();
  }
#endif

  vTaskDelay(200 / portTICK_PERIOD_MS);
  return BTStatus::OK;
}

namespace {
struct BleEndJob {
  bool releaseMemory;
};

void bleEndWorker(void *arg) {
  auto *job = static_cast<BleEndJob *>(arg);
  BLE.end(job->releaseMemory);
  delete job;
  vTaskDelete(nullptr);
}
}  // namespace

// hostedDeinitBLE() runs when CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE is set, after the NimBLE port is torn down.
// Irreversible: after end(true) you cannot call begin() again without a hardware reset.
void BLEClass::end(bool releaseMemory) {
  if (bleInStackCallbackTask()) {
    auto *job = new (std::nothrow) BleEndJob{releaseMemory};
    if (!job || xTaskCreate(bleEndWorker, "ble-end", 6144, job, 1, nullptr) != pdPASS) {
      log_e("BLE.end: cannot defer stack-task teardown");
      delete job;
    }
    return;
  }
  {
    BLELockGuard lifecycle(_impl ? _impl->lifecycleMtx : nullptr);
    if (!_impl || !isInitialized() || _impl->ending) {
      return;
    }
    _impl->ending = true;
  }

  if (auto hook = _impl->loadHook(_impl->streamPreEndHook)) {
    hook();
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
  if (auto hook = _impl->loadHook(_impl->advEndHook)) {
    hook();
  }
#endif
#if BLE_SCANNING_SUPPORTED
  if (auto hook = _impl->loadHook(_impl->scanEndHook)) {
    hook();
  }
#endif

  // Close the public session before native shutdown callbacks run. Keep the
  // GATT tree alive until the host has stopped, but reject re-entrant server
  // creation and value sends while it is stopping.
#if BLE_L2CAP_SUPPORTED
  // Close the operation queue before the public initialized flag changes so a
  // racing channel call cannot enqueue behind nimble_port_stop().
  if (auto hook = _impl->loadHook(_impl->l2capPreEndHook)) {
    hook();
  }
#endif
  {
    BLELockGuard lifecycle(_impl ? _impl->lifecycleMtx : nullptr);
    setInitialized(false);
  }
#if BLE_GATT_SERVER_SUPPORTED
  if (auto hook = _impl->loadHook(_impl->serverPreEndHook)) {
    hook();
  }
#endif

  nimble_port_stop();
#if BLE_SCANNING_SUPPORTED
  // Native scan callbacks can remain retained in NimBLE's privacy-preemption
  // slot after stop() reports EALREADY. Reclaim their contexts only now, when
  // the host task can no longer dereference them.
  if (auto hook = _impl->loadHook(_impl->scanHostStoppedHook)) {
    hook();
  }
#endif
#if BLE_GATT_SERVER_SUPPORTED
  // Drop the deferred re-advertise event's backing reference only after the
  // host task has stopped: while it was alive it could still dispatch that
  // event, so resetting it before nimble_port_stop() raced the host task.
  if (auto hook = _impl->loadHook(_impl->serverHostStoppedHook)) {
    hook();
  }
#endif
#if BLE_L2CAP_SUPPORTED
  // The host task is stopped, so remove every queued per-channel operation and
  // orphan-disconnect event, release their waiters, and return their NPL
  // backing storage before nimble_port_deinit() destroys the event pool.
  if (auto hook = _impl->loadHook(_impl->l2capHostStoppedHook)) {
    hook();
  }
#endif
  nimble_port_deinit();
  _impl->synced = false;

#if BLE_L2CAP_SUPPORTED
  if (auto hook = _impl->loadHook(_impl->l2capPostEndHook)) {
    hook();
  }
#endif

#if BLE_GATT_SERVER_SUPPORTED
  // Native callbacks can retain raw pointers into the GATT tree. Release it
  // only after the host task has stopped and deinit has discarded those
  // registrations.
  if (auto hook = _impl->loadHook(_impl->serverPostEndHook)) {
    hook();
  }
#endif
  if (auto hook = _impl->loadHook(_impl->streamPostEndHook)) {
    hook();
  }

#if defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
  hostedDeinitBLE();
#endif

  if (releaseMemory) {
#if SOC_BLE_SUPPORTED && CONFIG_BT_CONTROLLER_ENABLED
    btMemRelease(BT_MODE_BLE);
#endif
  }

  {
    BLELockGuard lifecycle(_impl ? _impl->lifecycleMtx : nullptr);
    _impl->ending = false;
  }
}

bool BLEClass::setStreamEndHooks(void (*preHook)(), void (*postHook)()) {
  if (!_impl) {
    return false;
  }
  BLELockGuard lifecycle(_impl->lifecycleMtx);
  if (_impl->ending) {
    return false;
  }
  portENTER_CRITICAL(&_impl->hookMux);
  _impl->streamPreEndHook = preHook;
  _impl->streamPostEndHook = postHook;
  portEXIT_CRITICAL(&_impl->hookMux);
  return true;
}

// --------------------------------------------------------------------------
// Identity
// --------------------------------------------------------------------------

BTAddress BLEClass::getAddress() const {
  if (!_impl || !isInitialized()) {
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
  if (!_impl || !isInitialized()) {
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
  if (!_impl || !isInitialized()) {
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
  if (!_impl || !isInitialized()) {
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
  if (!_impl || !isInitialized() || !irk) {
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
  if (!_impl || !isInitialized() || !irk) {
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
  if (!_impl || !isInitialized()) {
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
  if (!isInitialized()) {
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
  if (!isInitialized()) {
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
  if (!_impl || !isInitialized()) {
    return BTStatus::NotInitialized;
  }
  _impl->storeHook(_impl->customGapHandler, handler);

  auto wrapper = [](struct ble_gap_event *event, void *arg) -> int {
    auto *impl = static_cast<BLEClass::Impl *>(arg);
    if (auto custom = impl->loadHook(impl->customGapHandler)) {
      return custom(event, nullptr);
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
  BLELockGuard lifecycle(_impl ? _impl->lifecycleMtx : nullptr);
  if (!isInitialized() || _impl->ending) {
    log_e("getScan: BLE not initialized");
    return BLEScan();
  }
  if (!slot) {
    slot = std::make_shared<BLEScan::Impl>();
    _impl->scan = slot.get();
    _impl->storeHook(
      _impl->scanBeginHook,
      +[]() {
        BLEScan scan(slot);
        scan.prepareBackendStart();
      }
    );
    _impl->storeHook(
      _impl->scanEndHook,
      +[]() {
        BLEScan scan(slot);
        scan.beginBackendShutdown();
        scan.terminateAllPeriodicSyncs();
      }
    );
    _impl->storeHook(
      _impl->scanHostStoppedHook,
      +[]() {
        BLEScan scan(slot);
        scan.resetBackendEvents();
      }
    );
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
  BLELockGuard lifecycle(_impl ? _impl->lifecycleMtx : nullptr);
  if (!isInitialized() || _impl->ending) {
    log_e("getAdvertising: BLE not initialized");
    return BLEAdvertising();
  }
  if (!slot) {
    slot = std::make_shared<BLEAdvertising::Impl>();
    _impl->adv = slot.get();
    _impl->storeHook(
      _impl->advBeginHook,
      +[]() {
        BLEAdvertising(slot).prepareBackendStart();
      }
    );
    _impl->storeHook(
      _impl->advEndHook,
      +[]() {
        BLEAdvertising(slot).beginBackendShutdown();
      }
    );
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
  // Process-lifetime singleton. end() clears its GATT tree but keeps this slot so
  // existing BLEServer handles remain the same server after begin().
  static std::shared_ptr<BLEServer::Impl> s_server;
  BLELockGuard lifecycle(_impl ? _impl->lifecycleMtx : nullptr);
  if (!isInitialized() || _impl->ending) {
    log_e("createServer: BLE not initialized");
    return BLEServer();
  }
  if (!s_server) {
    log_d("createServer: creating new server instance");
    s_server = std::make_shared<BLEServer::Impl>();
    storeGapHook(
      s_serverGapEventHook,
      +[](struct ble_gap_event *event) -> int {
        if (s_server) {
          BLEServer srv(s_server);
          return BLEServerImplCommon::forwardGapEvent(srv, event);
        }
        return 0;
      }
    );
  }
  _impl->storeHook(
    _impl->serverPreEndHook,
    +[]() {
      if (s_server) {
        BLELockGuard lock(s_server->mtx);
        s_server->active = false;
      }
    }
  );
  _impl->storeHook(_impl->serverHostStoppedHook, nimbleResetReAdvertiseEvent);
  _impl->storeHook(
    _impl->serverPostEndHook,
    +[]() {
      if (s_server) {
        s_server->releaseGattTree();
      }
      nimbleClearRetainedGattDefs();
    }
  );
  {
    BLELockGuard lock(s_server->mtx);
    s_server->active = true;
  }
  return BLEServer(s_server);
}
#else
BLEServer BLEClass::createServer() {
  log_w("GATT server not supported");
  return BLEServer();
}
#endif

#if BLE_GATT_CLIENT_SUPPORTED
BLEClient BLEClass::createClient() {
  BLELockGuard lifecycle(_impl ? _impl->lifecycleMtx : nullptr);
  if (!isInitialized() || _impl->ending) {
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
  BLELockGuard lifecycle(_impl ? _impl->lifecycleMtx : nullptr);
  if (!isInitialized() || _impl->ending) {
    log_e("getSecurity: BLE not initialized");
    return BLESecurity();
  }
  if (!slot) {
    slot = std::make_shared<BLESecurity::Impl>();
    BLESecurity::Impl::s_instance = slot.get();
    storeGapHook(s_securityGapEventHook, BLESecurity::Impl::handleGapEvent);
    _impl->storeHook(
      _impl->securityBeginHook,
      +[]() {
        if (BLESecurity::Impl *sec = BLESecurity::Impl::instance()) {
          sec->applySecurityParams();
        }
      }
    );
    _impl->storeHook(
      _impl->securityOverflowHook,
      +[](const BTAddress &oldest) -> bool {
        if (BLESecurity::Impl *sec = BLESecurity::Impl::instance()) {
          return sec->notifyBondOverflow(oldest);
        }
        return false;
      }
    );
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

#endif /* BLE_NIMBLE */
