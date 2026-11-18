/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
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
 * @file BLE.cpp
 * @brief Stack-agnostic `BLEClass` lifecycle orchestration (`begin()` / `end()`),
 *        shared state accessors, IRK formatters, TX power helpers, and
 *        feature-disabled stubs.
 */

#include "core/BLEGuards.h"
#if BLE_ENABLED

#include "BLE.h"
#include "BLEImpl.h"
#include "core/BLEBackend.h"
#include "core/BLEAudioHooks.h"
#include "core/BLEMutex.h"
#include "core/BLETask.h"
#include "core/BLEValidationHooks.h"
#if BLE_ISO_SUPPORTED
#include "iso/BLEIso.h"
#include "iso/BLEIsoHost.h"
#endif
#include "HEXBuilder.h"
#include "base64.h"
#include "esp32-hal-bt.h"
#include "esp32-hal-alloc-ble-mem.h"
#include "esp32-hal-log.h"

#include <cstring>

#if BLE_LOCAL_CONTROLLER_SUPPORTED
#include <esp_bt.h>
#endif

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_BLE)
BLEClass BLE;
#endif

// ==========================================================================
// 1. Stack-task detection & deferred shutdown worker
// ==========================================================================

bool BLETask::inStackCallbackTask() {
  if (BLETask::isHostStackTask()) {
    return true;
  }
#if BLE_AUDIO_SUPPORTED || BLE_ISO_SUPPORTED
  TaskHandle_t current = xTaskGetCurrentTaskHandle();
  const char *name = current ? pcTaskGetName(current) : nullptr;
  return name && strcmp(name, "iso_task") == 0;
#else
  return false;
#endif
}

namespace {

#if BLE_ISO_SUPPORTED
void isoPreEndTrampoline() {
  BLEIsoInternal::preEnd();
}
#endif

void bleEndWorker(void *) {
  BLE.end(false);
  vTaskDelete(nullptr);
}

bool bleStartEndWorker() {
#if BLE_VALIDATION_HOOKS
  if (BLEValidationHooks::consumeFailure(BLEHooks.lifecycle.endWorkerStartFailures)) {
    return false;
  }
#endif
  return xTaskCreate(bleEndWorker, "ble-end", 6144, nullptr, 1, nullptr) == pdPASS;
}

}  // namespace

// ==========================================================================
// 2. Shared lifecycle & state implementations
// ==========================================================================

BLEClass::BLEClass() : _impl(new Impl()) {}

BLEClass::~BLEClass() {
  if (isInitialized()) {
    end(false);
  }
  delete _impl;
  _impl = nullptr;
}

BLEClass::operator bool() const {
  return isInitialized();
}

bool BLEClass::isInitialized() const {
  portENTER_CRITICAL(&_stateMux);
  bool initialized = _initialized;
  portEXIT_CRITICAL(&_stateMux);
  return initialized;
}

void BLEClass::setInitialized(bool initialized) {
  portENTER_CRITICAL(&_stateMux);
  _initialized = initialized;
  portEXIT_CRITICAL(&_stateMux);
}

bool BLEClass::isEnding() const {
  if (!_impl) {
    return false;
  }
  BLELockGuard lifecycle(_impl->lifecycleMtx);
  return _impl->ending;
}

BTStatus BLEClass::begin(const String &deviceName) {
  if (!_impl || !_impl->lifecycleMtx) {
    return BTStatus::NoMemory;
  }
  BLELockGuard lifecycle(_impl->lifecycleMtx);
  if (_impl->ending) {
    return BTStatus::InvalidState;
  }
  if (isInitialized()) {
    log_d("begin: BLE already initialized, keeping the current stack");
    return BTStatus::OK;
  }

#if BLE_CONTROLLER_ENABLED
  if (btMemReleased(BT_MODE_BLE)) {
    log_e("Cannot reinitialize BLE: memory has been released");
    return BTStatus::InvalidState;
  }
#endif

  BTStatus st = _impl->backendBegin(deviceName);
  if (!st) {
    return st;
  }

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
#if BLE_ISO_SUPPORTED
  _impl->storeHook(_impl->isoPreEndHook, isoPreEndTrampoline);
#endif
  _impl->backendAfterHooks();
#if BLE_SMP_SUPPORTED
  // getSecurity() keeps its Impl in a function-local static that outlives an
  // end()/begin() cycle. Re-push any retained configuration so the freshly
  // initialized stack matches getSecurity().
  if (auto hook = _impl->loadHook(_impl->securityBeginHook)) {
    hook();
  }
#endif

  _impl->backendFinishBegin();
  return BTStatus::OK;
}

void BLEClass::end(bool releaseMemory) {
  if (BLETask::inStackCallbackTask()) {
    // Claim ending before returning so begin() cannot treat the still-live
    // stack as a successful re-init. The worker, or a later user-thread end(),
    // runs the teardown body.
    {
      BLELockGuard lifecycle(_impl ? _impl->lifecycleMtx : nullptr);
      if (!_impl || (!isInitialized() && !_impl->endRunning)) {
        return;
      }
      if (releaseMemory) {
        _impl->endReleaseMemory = true;
      }
      if (_impl->ending) {
        return;
      }
      _impl->ending = true;
    }
    if (!bleStartEndWorker()) {
      log_e("BLE.end: cannot defer stack-task teardown");
    }
    return;
  }

  while (true) {
    bool waitForWorker = false;
    {
      BLELockGuard lifecycle(_impl ? _impl->lifecycleMtx : nullptr);
      if (!_impl) {
        return;
      }
      if (!isInitialized() && !_impl->endRunning) {
        return;
      }
      if (releaseMemory) {
        _impl->endReleaseMemory = true;
      }
      if (_impl->endRunning) {
        if (_impl->endTask == xTaskGetCurrentTaskHandle()) {
          return;
        }
        waitForWorker = true;
      } else {
        _impl->ending = true;
        _impl->endRunning = true;
        _impl->endTask = xTaskGetCurrentTaskHandle();
      }
    }
    if (!waitForWorker) {
      break;
    }
    vTaskDelay(1);
  }

  if (auto hook = _impl->loadHook(_impl->streamPreEndHook)) {
    hook();
  }

#if BLE_VALIDATION_HOOKS
  if (BLEValidationHooks::consumeFailure(BLEHooks.lifecycle.preEndAbortFailures)) {
    _impl->abortEnd();
    return;
  }
#endif
  // ISO groups own advertising/sync resources and both ISO and Audio need the
  // live host to complete asynchronous stream termination.
#if BLE_ISO_SUPPORTED
  if (auto hook = _impl->loadHook(_impl->isoPreEndHook)) {
    hook();
  }
  if (BLEIsoInternal::isActive()) {
    log_e("BLE.end: ISO teardown did not complete; host shutdown aborted");
    _impl->abortEnd();
    return;
  }
#endif
#if BLE_AUDIO_SUPPORTED
  bool audioEndOk = !bleAudioPreEndHook || bleAudioPreEndHook();
  if (!audioEndOk) {
    log_e("BLE.end: audio teardown did not complete; host shutdown aborted");
    _impl->abortEnd();
    return;
  }
#endif

  _impl->backendPreAdvScanEnd();

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

  _impl->backendPreSessionClose();

  // Close the public session before native shutdown callbacks run. The GATT
  // tree stays attached until host deinit, while callbacks cannot re-enter
  // createServer() or send through a host that is stopping.
  {
    BLELockGuard lifecycle(_impl->lifecycleMtx);
    setInitialized(false);
  }
#if BLE_GATT_SERVER_SUPPORTED
  if (auto hook = _impl->loadHook(_impl->serverDeactivateHook)) {
    hook();
  }
#endif

  _impl->backendStopAndDeinitHost();

#if BLE_GATT_SERVER_SUPPORTED
  if (auto hook = _impl->loadHook(_impl->serverPostEndHook)) {
    hook();
  }
#endif
  if (auto hook = _impl->loadHook(_impl->streamPostEndHook)) {
    hook();
  }

  bool release = false;
  {
    BLELockGuard lifecycle(_impl->lifecycleMtx);
    release = _impl->endReleaseMemory;
    _impl->endReleaseMemory = false;
  }
  _impl->backendFinalizeEnd(release);
  {
    BLELockGuard lifecycle(_impl->lifecycleMtx);
    _impl->endTask = nullptr;
    _impl->endRunning = false;
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

// ==========================================================================
// 3. Identity, MTU, whitelist, IRK, and TX power accessors
// ==========================================================================

String BLEClass::getDeviceName() const {
  return _deviceName;
}

BTAddress::Type BLEClass::getOwnAddressType() const {
  return _ownAddressType;
}

uint16_t BLEClass::getMTU() const {
  return _impl ? _impl->localMTU : 23;
}

bool BLEClass::isOnWhiteList(const BTAddress &address) const {
  for (const auto &a : _whiteList) {
    if (a == address) {
      return true;
    }
  }
  return false;
}

String BLEClass::getLocalIRKString() const {
  uint8_t irk[16];
  if (!getLocalIRK(irk)) {
    return "";
  }
  return HEXBuilder::bytes2hex(irk, sizeof(irk));
}

String BLEClass::getLocalIRKBase64() const {
  uint8_t irk[16];
  if (!getLocalIRK(irk)) {
    return "";
  }
  return base64::encode(irk, sizeof(irk));
}

String BLEClass::getPeerIRKString(const BTAddress &peer) const {
  uint8_t irk[16];
  if (!getPeerIRK(peer, irk)) {
    return "";
  }
  return HEXBuilder::bytes2hex(irk, sizeof(irk));
}

String BLEClass::getPeerIRKBase64(const BTAddress &peer) const {
  uint8_t irk[16];
  if (!getPeerIRK(peer, irk)) {
    return "";
  }
  return base64::encode(irk, sizeof(irk));
}

String BLEClass::getPeerIRKReverse(const BTAddress &peer) const {
  uint8_t irk[16];
  if (!getPeerIRK(peer, irk)) {
    return "";
  }

  unsigned char reversed[16];
  for (int i = 0; i < 16; i++) {
    reversed[i] = irk[15 - i];
  }
  return HEXBuilder::bytes2hex(reversed, sizeof(reversed));
}

#if BLE_LOCAL_CONTROLLER_SUPPORTED

void BLEClass::setPower(int8_t txPowerDbm) {
  if (!isInitialized()) {
    return;
  }
  esp_power_level_t level;
  if (txPowerDbm <= -12) {
    level = ESP_PWR_LVL_N12;
  } else if (txPowerDbm <= -9) {
    level = ESP_PWR_LVL_N9;
  } else if (txPowerDbm <= -6) {
    level = ESP_PWR_LVL_N6;
  } else if (txPowerDbm <= -3) {
    level = ESP_PWR_LVL_N3;
  } else if (txPowerDbm <= 0) {
    level = ESP_PWR_LVL_N0;
  } else if (txPowerDbm <= 3) {
    level = ESP_PWR_LVL_P3;
  } else if (txPowerDbm <= 6) {
    level = ESP_PWR_LVL_P6;
  } else {
    level = ESP_PWR_LVL_P9;
  }
  log_d("setPower: %d dBm (level=%d)", txPowerDbm, level);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, level);
}

int8_t BLEClass::getPower() const {
  if (!isInitialized()) {
    return -128;
  }
  switch (esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_DEFAULT)) {
    case ESP_PWR_LVL_N12: return -12;
    case ESP_PWR_LVL_N9:  return -9;
    case ESP_PWR_LVL_N6:  return -6;
    case ESP_PWR_LVL_N3:  return -3;
    case ESP_PWR_LVL_N0:  return 0;
    case ESP_PWR_LVL_P3:  return 3;
    case ESP_PWR_LVL_P6:  return 6;
    case ESP_PWR_LVL_P9:  return 9;
    default:              return -128;
  }
}

#endif /* BLE_LOCAL_CONTROLLER_SUPPORTED */

BTStatus BLEClass::getDefaultPhy(BLEPhy &txPhy, BLEPhy &rxPhy) const {
  txPhy = BLEPhy::PHY_1M;
  rxPhy = BLEPhy::PHY_1M;
  log_w("getDefaultPhy not supported");
  return BTStatus::NotSupported;
}

bool BLEClass::isHostedBLE() const {
  return BLE_HOSTED_NIMBLE != 0;
}

#if BLE_ADVERTISING_SUPPORTED

BTStatus BLEClass::startAdvertising() {
  return getAdvertising().start();
}

BTStatus BLEClass::stopAdvertising() {
  return getAdvertising().stop();
}

#endif /* BLE_ADVERTISING_SUPPORTED */

// ==========================================================================
// 4. Feature-disabled & capability stubs
// ==========================================================================

#if !BLE_LOCAL_CONTROLLER_SUPPORTED

void BLEClass::setPower(int8_t) {
  if (!isInitialized()) {
    return;
  }
  log_w("setPower not supported with hosted HCI");
}

int8_t BLEClass::getPower() const {
  return isInitialized() ? 0 : -128;
}

#endif /* !BLE_LOCAL_CONTROLLER_SUPPORTED */

#if !BLE_HOSTED_NIMBLE

BTStatus BLEClass::setPins(int8_t, int8_t, int8_t, int8_t, int8_t, int8_t, int8_t) {
  log_w("setPins not supported (hosted NimBLE unavailable)");
  return BTStatus::NotSupported;
}

#endif /* !BLE_HOSTED_NIMBLE */

#if !BLE5_SUPPORTED

BTStatus BLEClass::setDefaultPhy(BLEPhy, BLEPhy) {
  log_w("setDefaultPhy not supported (BLE 5.0 unavailable)");
  return BTStatus::NotSupported;
}

#endif /* !BLE5_SUPPORTED */

#if !BLE_ADVERTISING_SUPPORTED

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

#endif /* !BLE_ADVERTISING_SUPPORTED */

#if !BLE_SCANNING_SUPPORTED

BLEScan BLEClass::getScan() {
  log_w("Scanning not supported");
  return BLEScan();
}

#endif /* !BLE_SCANNING_SUPPORTED */

#if !BLE_GATT_SERVER_SUPPORTED

BLEServer BLEClass::createServer() {
  log_w("GATT server not supported");
  return BLEServer();
}

#endif /* !BLE_GATT_SERVER_SUPPORTED */

#if !BLE_GATT_CLIENT_SUPPORTED

BLEClient BLEClass::createClient() {
  log_w("GATT client not supported");
  return BLEClient();
}

#endif /* !BLE_GATT_CLIENT_SUPPORTED */

#if !BLE_SMP_SUPPORTED

BLESecurity BLEClass::getSecurity() {
  log_w("SMP not supported");
  return BLESecurity();
}

#endif /* !BLE_SMP_SUPPORTED */

#endif /* BLE_ENABLED */
