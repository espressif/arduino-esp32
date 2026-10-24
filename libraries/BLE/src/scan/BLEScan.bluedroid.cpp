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
 * @file BLEScan.bluedroid.cpp
 * @brief Bluedroid (ESP-IDF) implementation of BLE GAP scan and result handling.
 */

#include "core/BLEGuards.h"
#if BLE_BLUEDROID

#if BLE_SCANNING_SUPPORTED

#include "BLE.h"

#include "scan/BLEScan.bluedroid.h"
#include "scan/BLEAdvertisedDeviceImpl.h"
#include "scan/BLEScanImpl.h"
#include "core/BLEImplHelpers.h"
#include "advertising/BLEAdvScanHelpers.h"
#include "esp32-hal-log.h"

#include <esp_gap_ble_api.h>
#include <algorithm>
#include <cstring>
#include <vector>

// API contract is documented on the declarations in the public BLE*.h headers; the definitions below carry implementation notes only.

BLEScan::Impl *BLEScan::Impl::s_instance = nullptr;

#include "core/BLEValidationHooks.h"

namespace {

bool duplicateKeySeen(const BLEScan::Impl &impl, const BTAddress &address, uint8_t sid) {
  return std::any_of(impl.duplicateKeys.begin(), impl.duplicateKeys.end(), [&](const auto &key) {
    return key.first == address && key.second == sid;
  });
}

void markDuplicateKey(BLEScan::Impl &impl, const BTAddress &address, uint8_t sid) {
  if (!duplicateKeySeen(impl, address, sid)) {
    impl.duplicateKeys.emplace_back(address, sid);
  }
}

}  // namespace

// One-shot expiry for fractional/sub-second legacy durations: stop the
// (continuous) stack scan; SCAN_STOP_COMPLETE then dispatches onComplete and
// gives scanSync because timerExpiryStop is set (see handleGAP).
void BLEScan::Impl::durationTimerCb(void *arg) {
  auto *inst = static_cast<BLEScan::Impl *>(arg);
  if (!inst || !inst->isScanning) {
    return;
  }
  BLEAdmissionGuard admission(inst->nativeAdmission);
  if (!admission) {
    return;
  }
  inst->timerExpiryStop = true;
  esp_err_t err = esp_ble_gap_stop_scanning();
  if (err != ESP_OK) {
    inst->timerExpiryStop = false;
    log_e("scan duration timer: esp_ble_gap_stop_scanning: %s", esp_err_to_name(err));
  }
}

// --------------------------------------------------------------------------
// Configuration
// --------------------------------------------------------------------------

void BLEScan::clearDuplicateCache() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.duplicateKeys.clear();
}

// --------------------------------------------------------------------------
// Start / stop
// --------------------------------------------------------------------------

BTStatus BLEScan::start(uint32_t durationMs, bool appendToExistingResults) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLEAdmissionGuard admission(impl.nativeAdmission);
  if (!admission) {
    return BTStatus::InvalidState;
  }

  if (impl.isScanning) {
    BTStatus stopStatus = stopBackend();
    if (!stopStatus) {
      return stopStatus;
    }
  }

  if (!appendToExistingResults) {
    {
      BLELockGuard lock(impl.mtx);
      impl.results = BLEScan::Results();
    }
    clearDuplicateCache();
  }

#if BLE5_SUPPORTED
  // On BLE5-capable Bluedroid legacy results are delivered as
  // ESP_GAP_BLE_EXT_ADV_REPORT_EVT, so drive the extended scan API and let the
  // report handler merge scan responses. NOTE: not buildable on today's esp32
  // Bluedroid libs (BLE5 off); not yet HW-tested.
  esp_ble_ext_scan_params_t extParams = {};
  extParams.own_addr_type = static_cast<esp_ble_addr_type_t>(BLE.getOwnAddressType());
  extParams.filter_policy = BLE_SCAN_FILTER_ALLOW_ALL;
  // Controller duplicate filters may suppress a same-address report even when
  // its payload changed. Keep native filtering off and apply the public policy
  // to byte-identical reports in the GAP callback instead.
  extParams.scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE;
  extParams.cfg_mask = ESP_BLE_GAP_EXT_SCAN_CFG_UNCODE_MASK;
  extParams.uncoded_cfg.scan_type = impl.activeScan ? BLE_SCAN_TYPE_ACTIVE : BLE_SCAN_TYPE_PASSIVE;
  extParams.uncoded_cfg.scan_interval = impl.interval;
  extParams.uncoded_cfg.scan_window = impl.window;

  impl.paramSync.take();
  esp_err_t err = esp_ble_gap_set_ext_scan_params(&extParams);
  if (err != ESP_OK) {
    impl.paramSync.give(BTStatus::Fail);
    log_e("esp_ble_gap_set_ext_scan_params: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }
  BTStatus rc = impl.paramSync.wait(5000);
  if (rc != BTStatus::OK) {
    log_e("Ext scan param set did not complete (status=%d)", static_cast<int>(rc.code()));
    return rc;
  }

  const char *taskName = pcTaskGetName(nullptr);
  const bool onBtcTask = taskName && strcmp(taskName, "BTC_TASK") == 0;

  // Extended scan duration is in 10 ms units; 0 == scan until stopped.
  uint32_t durationUnits = (durationMs == 0) ? 0 : std::max<uint32_t>(1, (durationMs + 9) / 10);
  impl.scanSync.take();
  if (!onBtcTask) {
    impl.paramSync.take();
  }
  impl.isScanning = true;
  err = esp_ble_gap_start_ext_scan(durationUnits, 0);
  if (err != ESP_OK) {
    impl.isScanning = false;
    if (!onBtcTask) {
      impl.paramSync.give(BTStatus::Fail);
    }
    impl.scanSync.give(BTStatus::Fail);
    log_e("esp_ble_gap_start_ext_scan: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }
  if (!onBtcTask) {
    return impl.paramSync.wait(5000);
  }
  return BTStatus::OK;
#else
  // Configure scan parameters
  esp_ble_scan_params_t scanParams = {};
  scanParams.scan_type = impl.activeScan ? BLE_SCAN_TYPE_ACTIVE : BLE_SCAN_TYPE_PASSIVE;
  scanParams.own_addr_type = static_cast<esp_ble_addr_type_t>(BLE.getOwnAddressType());
  scanParams.scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL;
  scanParams.scan_interval = impl.interval;
  scanParams.scan_window = impl.window;
  scanParams.scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE;

  // Set scan params and wait for completion event
  impl.paramSync.take();
  esp_err_t err = esp_ble_gap_set_scan_params(&scanParams);
  if (err != ESP_OK) {
    impl.paramSync.give(BTStatus::Fail);
    log_e("esp_ble_gap_set_scan_params: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }

  BTStatus rc = impl.paramSync.wait(5000);
  if (rc != BTStatus::OK) {
    log_e("Scan param set did not complete (status=%d)", static_cast<int>(rc.code()));
    return rc;
  }

  const char *taskName = pcTaskGetName(nullptr);
  const bool onBtcTask = taskName && strcmp(taskName, "BTC_TASK") == 0;

  // Bluedroid uses whole seconds. A sub-second duration would truncate to 0
  // (continuous) and a fractional one would lose its remainder, unlike
  // NimBLE's exact ms. Run those as a continuous scan stopped by a one-shot
  // esp_timer at exactly durationMs; whole-second durations keep the native
  // stack duration (whose INQ_CMPL expiry dispatches onComplete).
  const bool needsTimer = (durationMs != 0 && (durationMs % 1000) != 0);
  uint32_t durationSec = needsTimer ? 0 : (durationMs / 1000);
  impl.scanSync.take();
  if (!onBtcTask) {
    impl.paramSync.take();
  }
  impl.isScanning = true;

  err = esp_ble_gap_start_scanning(durationSec);
  if (err != ESP_OK) {
    impl.isScanning = false;
    if (!onBtcTask) {
      impl.paramSync.give(BTStatus::Fail);
    }
    impl.scanSync.give(BTStatus::Fail);
    log_e("esp_ble_gap_start_scanning: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }

  if (!onBtcTask) {
    rc = impl.paramSync.wait(5000);
    if (rc != BTStatus::OK) {
      log_e("Scan start did not complete (status=%d)", static_cast<int>(rc.code()));
      return rc;
    }
  }

  if (needsTimer) {
    if (!impl.durationTimer) {
      esp_timer_create_args_t timerArgs = {};
      timerArgs.callback = BLEScan::Impl::durationTimerCb;
      timerArgs.arg = &impl;
      timerArgs.name = "ble_scan_dur";
      timerArgs.dispatch_method = ESP_TIMER_TASK;
      esp_err_t terr = esp_timer_create(&timerArgs, &impl.durationTimer);
      if (terr != ESP_OK) {
        log_e("scan duration timer create: %s", esp_err_to_name(terr));
        (void)stopBackend();
        return BTStatus::Fail;
      }
    }
    esp_timer_stop(impl.durationTimer);
    esp_err_t terr = esp_timer_start_once(impl.durationTimer, static_cast<uint64_t>(durationMs) * 1000ULL);
    if (terr != ESP_OK) {
      log_e("scan duration timer start: %s", esp_err_to_name(terr));
      (void)stopBackend();
      return BTStatus::Fail;
    }
  }

  return BTStatus::OK;
#endif /* BLE5_SUPPORTED */
}

BLEScan::Results BLEScan::startBlocking(uint32_t durationMs) {
  BLE_CHECK_IMPL(BLEScan::Results());

  BTStatus rc = start(durationMs, false);
  if (rc != BTStatus::OK) {
    log_e("Scan: startBlocking failed to start scan");
    return BLEScan::Results();
  }

  // start() already armed scanSync before launching the scan, so the semaphore
  // is drained and ready. Wait directly for the scan-completion or stop give().
  uint32_t waitMs = (durationMs == 0) ? portMAX_DELAY : durationMs + 2000;
  BTStatus waitSt = impl.scanSync.wait(waitMs);
  if (waitSt != BTStatus::OK) {
    log_w("Scan: startBlocking wait timed out");
  }

  BLELockGuard lock(impl.mtx);
  return impl.results;
}

BTStatus BLEScan::stop() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLEAdmissionGuard admission(impl.nativeAdmission);
  if (!admission) {
    return BTStatus::OK;
  }
  return stopBackend();
}

BTStatus BLEScan::stopBackend() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (impl.durationTimer) {
    esp_timer_stop(impl.durationTimer);
  }
  impl.timerExpiryStop = false;
  if (!impl.isScanning) {
    return BTStatus::OK;
  }
  TaskHandle_t cur = xTaskGetCurrentTaskHandle();
  const char *tn = cur ? pcTaskGetName(cur) : "";
  const bool waitStop = (!tn || strcmp(tn, "BTC_TASK") != 0);
  if (waitStop) {
    impl.stopSync.take();
  }
  esp_err_t err;
#if BLE_VALIDATION_HOOKS
  int remainingFailures = BLEHooks.scan.cancelFailures.load(std::memory_order_relaxed);
  while (remainingFailures > 0
         && !BLEHooks.scan.cancelFailures.compare_exchange_weak(
           remainingFailures, remainingFailures - 1, std::memory_order_relaxed
         )) {}
  bool forceStopFailure = remainingFailures > 0;
#else
  bool forceStopFailure = false;
#endif
#if BLE5_SUPPORTED
  err = forceStopFailure ? ESP_FAIL : esp_ble_gap_stop_ext_scan();
#else
  err = forceStopFailure ? ESP_FAIL : esp_ble_gap_stop_scanning();
#endif
  if (err != ESP_OK) {
    if (waitStop) {
      impl.stopSync.give(BTStatus::Fail);
    }
    log_e("stop scanning: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }
  if (waitStop) {
    return impl.stopSync.wait(2000);
  }
  return BTStatus::OK;
}

// --------------------------------------------------------------------------
// Extended / Periodic
// --------------------------------------------------------------------------

BTStatus BLEScan::startExtended(uint32_t durationMs, const ExtScanConfig *codedConfig, const ExtScanConfig *uncodedConfig) {
#if BLE5_SUPPORTED
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLEAdmissionGuard admission(impl.nativeAdmission);
  if (!admission) {
    return BTStatus::InvalidState;
  }
  if (impl.isScanning) {
    BTStatus stopStatus = stopBackend();
    if (!stopStatus) {
      return stopStatus;
    }
  }
  {
    BLELockGuard lock(impl.mtx);
    impl.results = BLEScan::Results();
  }
  clearDuplicateCache();

  esp_ble_ext_scan_params_t extParams = {};
  extParams.own_addr_type = static_cast<esp_ble_addr_type_t>(BLE.getOwnAddressType());
  extParams.filter_policy = BLE_SCAN_FILTER_ALLOW_ALL;
  extParams.scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE;
  esp_ble_scan_type_t type = impl.activeScan ? BLE_SCAN_TYPE_ACTIVE : BLE_SCAN_TYPE_PASSIVE;
  // ExtScanConfig interval/window are in milliseconds; impl.interval/window are
  // already in 0.625 ms controller units.
  extParams.cfg_mask = ESP_BLE_GAP_EXT_SCAN_CFG_UNCODE_MASK;
  extParams.uncoded_cfg.scan_type = type;
  extParams.uncoded_cfg.scan_interval = uncodedConfig ? bleMsToUnits0625(uncodedConfig->interval) : impl.interval;
  extParams.uncoded_cfg.scan_window = uncodedConfig ? bleMsToUnits0625(uncodedConfig->window) : impl.window;
  if (codedConfig) {
    extParams.cfg_mask |= ESP_BLE_GAP_EXT_SCAN_CFG_CODE_MASK;
    extParams.coded_cfg.scan_type = type;
    extParams.coded_cfg.scan_interval = bleMsToUnits0625(codedConfig->interval);
    extParams.coded_cfg.scan_window = bleMsToUnits0625(codedConfig->window);
  }

  impl.paramSync.take();
  esp_err_t err = esp_ble_gap_set_ext_scan_params(&extParams);
  if (err != ESP_OK) {
    impl.paramSync.give(BTStatus::Fail);
    log_e("esp_ble_gap_set_ext_scan_params: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }
  BTStatus rc = impl.paramSync.wait(5000);
  if (rc != BTStatus::OK) {
    return rc;
  }

  const char *taskName = pcTaskGetName(nullptr);
  const bool onBtcTask = taskName && strcmp(taskName, "BTC_TASK") == 0;

  uint32_t durationUnits = (durationMs == 0) ? 0 : std::max<uint32_t>(1, (durationMs + 9) / 10);
  impl.scanSync.take();
  if (!onBtcTask) {
    impl.paramSync.take();
  }
  impl.isScanning = true;
  err = esp_ble_gap_start_ext_scan(durationUnits, 0);
  if (err != ESP_OK) {
    impl.isScanning = false;
    if (!onBtcTask) {
      impl.paramSync.give(BTStatus::Fail);
    }
    impl.scanSync.give(BTStatus::Fail);
    log_e("esp_ble_gap_start_ext_scan: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }
  if (!onBtcTask) {
    return impl.paramSync.wait(5000);
  }
  return BTStatus::OK;
#else
  (void)durationMs;
  (void)codedConfig;
  (void)uncodedConfig;
  log_w("%s not supported on this Bluedroid build (BLE 5.0 unavailable)", __func__);
  return BTStatus::NotSupported;
#endif
}

BTStatus BLEScan::stopExtended() {
  return stop();
}

BTStatus BLEScan::createPeriodicSync(const BTAddress &addr, uint8_t sid, uint16_t skipCount, uint16_t timeoutMs) {
#if BLE_PERIODIC_ADV_SUPPORTED
  // Null-check only: the create_sync call does not touch Impl state (unlike NimBLE,
  // which passes &impl as the GAP callback context). Avoid BLE_CHECK_IMPL here —
  // it would leave an unused `impl` reference under -Werror=unused-variable.
  if (!_impl) {
    return BTStatus::InvalidState;
  }
  BLEAdmissionGuard admission(_impl->nativeAdmission);
  if (!admission) {
    return BTStatus::InvalidState;
  }

  esp_ble_gap_periodic_adv_sync_params_t params = {};
  params.filter_policy = 0;  // Use SID + address
  params.sid = sid;
  params.addr_type = static_cast<esp_ble_addr_type_t>(addr.type());
  addr.toEspBdAddr(params.addr);
  params.skip = skipCount;
  params.sync_timeout = timeoutMs / 10;  // units of 10 ms

  esp_err_t err = esp_ble_gap_periodic_adv_create_sync(&params);
  if (err != ESP_OK) {
    log_e("esp_ble_gap_periodic_adv_create_sync: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }
  return BTStatus::OK;
#else
  (void)addr;
  (void)sid;
  (void)skipCount;
  (void)timeoutMs;
  log_w("%s not supported on Bluedroid (periodic adv unavailable)", __func__);
  return BTStatus::NotSupported;
#endif
}

BTStatus BLEScan::cancelPeriodicSync() {
#if BLE_PERIODIC_ADV_SUPPORTED
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLEAdmissionGuard admission(impl.nativeAdmission);
  if (!admission) {
    return BTStatus::InvalidState;
  }
  esp_err_t err = esp_ble_gap_periodic_adv_sync_cancel();
  if (err != ESP_OK) {
    log_e("esp_ble_gap_periodic_adv_sync_cancel: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }
  return BTStatus::OK;
#else
  log_w("%s not supported on Bluedroid (periodic adv unavailable)", __func__);
  return BTStatus::NotSupported;
#endif
}

BTStatus BLEScan::terminatePeriodicSync(uint16_t syncHandle) {
#if BLE_PERIODIC_ADV_SUPPORTED
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLEAdmissionGuard admission(impl.nativeAdmission);
  if (!admission) {
    return BTStatus::InvalidState;
  }
  esp_err_t err = esp_ble_gap_periodic_adv_sync_terminate(syncHandle);
  if (err != ESP_OK) {
    log_e("esp_ble_gap_periodic_adv_sync_terminate handle=%u: %s", syncHandle, esp_err_to_name(err));
    return BTStatus::Fail;
  }
  if (_impl) {
    BLELockGuard lock(_impl->mtx);
    auto &v = _impl->periodicSyncs;
    v.erase(std::remove(v.begin(), v.end(), syncHandle), v.end());
  }
  return BTStatus::OK;
#else
  (void)syncHandle;
  log_w("%s not supported on Bluedroid (periodic adv unavailable)", __func__);
  return BTStatus::NotSupported;
#endif
}

// Internal teardown hook (see BLEScan.h). Runs from BLEClass::end() while the
// Bluedroid host is still enabled.
void BLEScan::terminateAllPeriodicSyncs() {
#if BLE_PERIODIC_ADV_SUPPORTED
  if (!_impl) {
    return;
  }
  std::vector<uint16_t> handles;
  {
    BLELockGuard lock(_impl->mtx);
    handles.swap(_impl->periodicSyncs);
  }
  for (uint16_t h : handles) {
    esp_ble_gap_periodic_adv_sync_terminate(h);
  }
#endif
}

void BLEScan::resetBackendEvents() {}

void BLEScan::prepareBackendStart() {
  if (_impl) {
    _impl->nativeAdmission.reopen();
  }
}

void BLEScan::beginBackendShutdown() {
  if (!_impl) {
    return;
  }
  _impl->nativeAdmission.close();
#if BLE_VALIDATION_HOOKS
  BLEHooks.scan.bluedroidShutdownEntered.store(true, std::memory_order_release);
  BLEValidationHooks::holdWhile(BLEHooks.scan.holdBluedroidShutdown);
#endif
  _impl->nativeAdmission.waitUntilIdle();
  bool wasScanning = _impl->isScanning.load(std::memory_order_acquire);
  BTStatus stopStatus = stopBackend();
  if (!stopStatus && wasScanning) {
    // The native host will be deinitialized next, so no completion is
    // guaranteed after a failed stop submission. Retire the public session
    // and release an unlimited startBlocking() waiter deterministically.
    _impl->isScanning.store(false, std::memory_order_release);
    _impl->timerExpiryStop.store(false, std::memory_order_release);
    _impl->scanSync.give(BTStatus::Fail);
    _impl->stopSync.give(BTStatus::Fail);
  }
}

// --------------------------------------------------------------------------
// GAP event handler (called from BluedroidCore gapCallback)
// --------------------------------------------------------------------------

/**
 * @brief Handles Bluedroid GAP scan-related callbacks for the singleton scan instance.
 * @param event GAP event type.
 * @param param Event parameters from the stack.
 */
void BLEScan::Impl::handleGAP(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  auto *inst = s_instance;
  if (!inst) {
    return;
  }

  switch (event) {

    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
      log_d("SCAN_PARAM_SET_COMPLETE status=%d", param->scan_param_cmpl.status);
      inst->paramSync.give(param->scan_param_cmpl.status == ESP_BT_STATUS_SUCCESS ? BTStatus::OK : BTStatus::Fail);
      break;

    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
    {
      log_d("SCAN_START_COMPLETE status=%d", param->scan_start_cmpl.status);
      bool ok = (param->scan_start_cmpl.status == ESP_BT_STATUS_SUCCESS);
      inst->isScanning = ok;
      inst->paramSync.give(ok ? BTStatus::OK : BTStatus::Fail);
      if (!ok) {
        inst->scanSync.give(BTStatus::Fail);
      }
      break;
    }

    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
    {
      log_d("SCAN_STOP_COMPLETE status=%d", param->scan_stop_cmpl.status);
      BTStatus st = param->scan_stop_cmpl.status == ESP_BT_STATUS_SUCCESS ? BTStatus::OK : BTStatus::Fail;
      if (st) {
        inst->isScanning = false;
      }
      // A stop driven by the fractional-duration timer is a natural expiry:
      // dispatch onComplete like ESP_GAP_SEARCH_INQ_CMPL_EVT does.
      if (inst->timerExpiryStop.exchange(false) && st) {
        BLEScan::CompleteHandler completeCb;
        BLEScan::Results resultsCopy;
        {
          BLELockGuard lock(inst->mtx);
          completeCb = inst->onCompleteCb;
          resultsCopy = inst->results;
        }
        if (completeCb) {
          completeCb(resultsCopy);
        }
      }
      inst->scanSync.give(st);
      inst->stopSync.give(st);
      break;
    }

    case ESP_GAP_BLE_SCAN_RESULT_EVT:
    {
      switch (param->scan_rst.search_evt) {

        case ESP_GAP_SEARCH_INQ_RES_EVT:
        {
          if (!inst->isScanning) {
            break;
          }

          BTAddress addr = BTAddress::fromEspBdAddr(param->scan_rst.bda, static_cast<BTAddress::Type>(param->scan_rst.ble_addr_type));
          if (param->scan_rst.ble_evt_type == ESP_BLE_EVT_SCAN_RSP || (param->scan_rst.adv_data_len == 0 && param->scan_rst.scan_rsp_len > 0)) {
            BLEAdvertisedDevice merged;
            bool found = false;
            bool duplicate = false;
            BLEScan::ResultHandler cb;
            {
              BLELockGuard lock(inst->mtx);
              const uint8_t *scanRsp = param->scan_rst.ble_adv + param->scan_rst.adv_data_len;
              for (const auto &existing : inst->results) {
                if (existing.getAddress() == addr && existing.getAdvSID() == 0xFF) {
                  duplicate = inst->filterDuplicates && duplicateKeySeen(*inst, addr, 0xFF)
                              && BLEScanImplCommon::sameScanResponsePayload(existing, scanRsp, param->scan_rst.scan_rsp_len);
                  break;
                }
              }
              if (!duplicate) {
                found = BLEScanImplCommon::mergeScanResponse(
                  inst->results, addr, 0xFF, scanRsp, param->scan_rst.scan_rsp_len, param->scan_rst.rssi, merged
                );
                if (found && inst->filterDuplicates) {
                  markDuplicateKey(*inst, addr, 0xFF);
                }
                cb = inst->onResultCb;
              } else {
                found = true;
              }
            }
            if (found) {
              if (!duplicate && cb) {
                cb(merged);
              }
              break;
            }
          }

          // Build advertised device from scan result
          auto devImpl = std::make_shared<BLEAdvertisedDevice::Impl>();
          devImpl->address = addr;
          devImpl->addrType = static_cast<BTAddress::Type>(param->scan_rst.ble_addr_type);
          devImpl->rssi = param->scan_rst.rssi;
          devImpl->hasRSSI = true;
          devImpl->legacy = true;

          switch (param->scan_rst.ble_evt_type) {
            case ESP_BLE_EVT_CONN_ADV:
              devImpl->advType = BLEAdvType::ConnectableScannable;
              devImpl->connectable = true;
              devImpl->scannable = true;
              break;
            case ESP_BLE_EVT_CONN_DIR_ADV:
              devImpl->advType = BLEAdvType::ConnectableDirected;
              devImpl->connectable = true;
              devImpl->directed = true;
              break;
            case ESP_BLE_EVT_DISC_ADV:
              devImpl->advType = BLEAdvType::ScannableUndirected;
              devImpl->scannable = true;
              break;
            case ESP_BLE_EVT_NON_CONN_ADV: devImpl->advType = BLEAdvType::NonConnectable; break;
            default:                       break;
          }

          devImpl->parsePayload(param->scan_rst.ble_adv, param->scan_rst.adv_data_len + param->scan_rst.scan_rsp_len);
          // parsePayload() defaults the primary length to the complete buffer.
          // Bluedroid combines legacy ADV and SCAN_RSP bytes in that buffer, so
          // restore the native split after parsing for later scan-response reuse.
          devImpl->advPayloadLen = param->scan_rst.adv_data_len;

          BLEAdvertisedDevice dev = BLEScanImplCommon::makeAdvertisedDeviceHandle(devImpl);

          // Snapshot callbacks under lock, invoke outside
          {
            BLEScan::ResultHandler resultCb;
            bool isDuplicate = false;
            {
              BLELockGuard lock(inst->mtx);

              // Match appendOrReplace and the extended path: distinct advertising
              // sets from one address (different SIDs) are not duplicates. Legacy
              // PDUs carry SID 0xFF, so this stays address-based among legacy
              // entries while not colliding with extended sets from the same peer.
              // Suppress only byte-identical reports. Same-length primary or
              // scan-response updates are fresh data, not duplicates.
              if (inst->filterDuplicates) {
                const bool keySeen = duplicateKeySeen(*inst, dev.getAddress(), dev.getAdvSID());
                for (const auto &existing : inst->results) {
                  if (existing.getAddress() == dev.getAddress() && existing.getAdvSID() == dev.getAdvSID()) {
                    isDuplicate = keySeen && BLEScanImplCommon::samePayload(existing, dev);
                    break;
                  }
                }
              }

              if (!isDuplicate) {
                // Dispatch the winning (possibly preserved cached) entry.
                dev = inst->results.appendOrReplace(dev);
                if (inst->filterDuplicates) {
                  markDuplicateKey(*inst, dev.getAddress(), dev.getAdvSID());
                }
                resultCb = inst->onResultCb;
              }
            }

            if (!isDuplicate) {
              if (resultCb) {
                resultCb(dev);
              }
            }
          }
          break;
        }

        case ESP_GAP_SEARCH_INQ_CMPL_EVT:
        {
          log_d("SCAN_INQ_CMPL_EVT");
          inst->isScanning = false;

          BLEScan::CompleteHandler completeCb;
          BLEScan::Results resultsCopy;
          {
            BLELockGuard lock(inst->mtx);
            completeCb = inst->onCompleteCb;
            resultsCopy = inst->results;
          }

          if (completeCb) {
            completeCb(resultsCopy);
          }

          inst->scanSync.give();
          break;
        }

        default: break;
      }
      break;
    }

#if BLE5_SUPPORTED
    // --- BLE5 extended scan (see BLEScan::start ext path) ---
    case ESP_GAP_BLE_SET_EXT_SCAN_PARAMS_COMPLETE_EVT:
      log_d("SET_EXT_SCAN_PARAMS_COMPLETE status=%d", param->set_ext_scan_params.status);
      inst->paramSync.give(param->set_ext_scan_params.status == ESP_BT_STATUS_SUCCESS ? BTStatus::OK : BTStatus::Fail);
      break;

    case ESP_GAP_BLE_EXT_SCAN_START_COMPLETE_EVT:
    {
      log_d("EXT_SCAN_START_COMPLETE status=%d", param->ext_scan_start.status);
      bool ok = (param->ext_scan_start.status == ESP_BT_STATUS_SUCCESS);
      inst->isScanning = ok;
      inst->paramSync.give(ok ? BTStatus::OK : BTStatus::Fail);
      if (!ok) {
        inst->scanSync.give(BTStatus::Fail);
      }
      break;
    }

    case ESP_GAP_BLE_EXT_SCAN_STOP_COMPLETE_EVT:
    {
      log_d("EXT_SCAN_STOP_COMPLETE status=%d", param->ext_scan_stop.status);
      BTStatus st = param->ext_scan_stop.status == ESP_BT_STATUS_SUCCESS ? BTStatus::OK : BTStatus::Fail;
      if (st) {
        inst->isScanning = false;
      }
      inst->scanSync.give(st);
      inst->stopSync.give(st);
      break;
    }

    case ESP_GAP_BLE_SCAN_TIMEOUT_EVT:
    {
      log_d("SCAN_TIMEOUT_EVT");
      inst->isScanning = false;
      BLEScan::CompleteHandler completeCb;
      BLEScan::Results resultsCopy;
      {
        BLELockGuard lock(inst->mtx);
        completeCb = inst->onCompleteCb;
        resultsCopy = inst->results;
      }
      if (completeCb) {
        completeCb(resultsCopy);
      }
      inst->scanSync.give();
      break;
    }

    case ESP_GAP_BLE_EXT_ADV_REPORT_EVT:
    {
      if (!inst->isScanning) {
        break;
      }
      const esp_ble_gap_ext_adv_report_t &r = param->ext_adv_report.params;
      BTAddress addr(r.addr, static_cast<BTAddress::Type>(r.addr_type));

      // Merge scan responses (which carry the Local Name) into the device from
      // the preceding advertisement.
      const bool isScanRsp = (r.event_type & ESP_BLE_ADV_REPORT_EXT_SCAN_RSP) != 0 || r.event_type == ESP_BLE_LEGACY_ADV_TYPE_SCAN_RSP_TO_ADV_IND
                             || r.event_type == ESP_BLE_LEGACY_ADV_TYPE_SCAN_RSP_TO_ADV_SCAN_IND;
      if (isScanRsp) {
        BLEAdvertisedDevice merged;
        bool found = false;
        bool duplicate = false;
        BLEScan::ResultHandler cb;
        {
          BLELockGuard lock(inst->mtx);
          for (const auto &existing : inst->results) {
            if (existing.getAddress() == addr && existing.getAdvSID() == r.sid) {
              duplicate = inst->filterDuplicates && duplicateKeySeen(*inst, addr, r.sid)
                          && BLEScanImplCommon::sameScanResponsePayload(existing, r.adv_data, r.adv_data_len);
              break;
            }
          }
          if (!duplicate) {
            found = BLEScanImplCommon::mergeScanResponse(inst->results, addr, r.sid, r.adv_data, r.adv_data_len, r.rssi, merged);
            if (found && inst->filterDuplicates) {
              markDuplicateKey(*inst, addr, r.sid);
            }
            cb = inst->onResultCb;
          } else {
            found = true;
          }
        }
        if (found) {
          if (!duplicate && cb) {
            cb(merged);
          }
          break;
        }
        // No prior advertisement: fall through and surface the scan response as
        // a standalone device so its name is not lost.
      }

      auto devImpl = std::make_shared<BLEAdvertisedDevice::Impl>();
      devImpl->address = addr;
      devImpl->addrType = static_cast<BTAddress::Type>(r.addr_type);
      devImpl->rssi = r.rssi;
      devImpl->hasRSSI = true;
      const uint8_t et = r.event_type;
      const bool legacyReport =
        (et == ESP_BLE_LEGACY_ADV_TYPE_IND || et == ESP_BLE_LEGACY_ADV_TYPE_DIRECT_IND || et == ESP_BLE_LEGACY_ADV_TYPE_SCAN_IND
         || et == ESP_BLE_LEGACY_ADV_TYPE_NONCON_IND || et == ESP_BLE_LEGACY_ADV_TYPE_SCAN_RSP_TO_ADV_IND
         || et == ESP_BLE_LEGACY_ADV_TYPE_SCAN_RSP_TO_ADV_SCAN_IND);
      devImpl->legacy = legacyReport;
      devImpl->connectable = (et & ESP_BLE_ADV_REPORT_EXT_ADV_IND) != 0 || et == ESP_BLE_LEGACY_ADV_TYPE_IND || et == ESP_BLE_LEGACY_ADV_TYPE_DIRECT_IND;
      devImpl->scannable = (et & ESP_BLE_ADV_REPORT_EXT_SCAN_IND) != 0 || et == ESP_BLE_LEGACY_ADV_TYPE_SCAN_IND;
      devImpl->primaryPhy = static_cast<BLEPhy>(r.primary_phy);
      devImpl->sid = r.sid;
      devImpl->periodicInterval = r.per_adv_interval;
      devImpl->parsePayload(r.adv_data, r.adv_data_len);

      BLEAdvertisedDevice dev = BLEScanImplCommon::makeAdvertisedDeviceHandle(devImpl);
      BLEScan::ResultHandler resultCb;
      bool isDuplicate = false;
      {
        BLELockGuard lock(inst->mtx);
        if (inst->filterDuplicates) {
          const bool keySeen = duplicateKeySeen(*inst, dev.getAddress(), dev.getAdvSID());
          // Match appendOrReplace: distinct advertising sets from one address
          // (different SIDs) are not duplicates of each other.
          for (const auto &existing : inst->results) {
            if (existing.getAddress() == dev.getAddress() && existing.getAdvSID() == dev.getAdvSID()) {
              isDuplicate = keySeen && BLEScanImplCommon::samePrimaryPayload(existing, dev);
              break;
            }
          }
        }
        if (!isDuplicate) {
          dev = inst->results.appendOrReplace(dev);
          if (inst->filterDuplicates) {
            markDuplicateKey(*inst, dev.getAddress(), dev.getAdvSID());
          }
          resultCb = inst->onResultCb;
        }
      }
      if (!isDuplicate && resultCb) {
        resultCb(dev);
      }
      break;
    }

#if BLE_PERIODIC_ADV_SUPPORTED
    case ESP_GAP_BLE_PERIODIC_ADV_SYNC_ESTAB_EVT:
    {
      const auto &e = param->periodic_adv_sync_estab;
      if (e.status != ESP_BT_STATUS_SUCCESS) {
        log_w("PERIODIC_ADV_SYNC_ESTAB failed status=%d", e.status);
        break;
      }
      {
        BLELockGuard lock(inst->mtx);
        inst->periodicSyncs.push_back(e.sync_handle);
      }
      BLEScan::PeriodicSyncHandler cb;
      {
        BLELockGuard lock(inst->mtx);
        cb = inst->periodicSyncCb;
      }
      if (cb) {
        BTAddress addr(e.adv_addr, static_cast<BTAddress::Type>(e.adv_addr_type));
        cb(e.sync_handle, e.sid, addr, static_cast<BLEPhy>(e.adv_phy), e.period_adv_interval);
      }
      break;
    }

    case ESP_GAP_BLE_PERIODIC_ADV_REPORT_EVT:
    {
      const auto &r = param->period_adv_report.params;
      BLEScan::PeriodicReportHandler cb;
      {
        BLELockGuard lock(inst->mtx);
        cb = inst->periodicReportCb;
      }
      if (cb) {
        cb(r.sync_handle, r.rssi, static_cast<int8_t>(r.tx_power), r.data, r.data_length);
      }
      break;
    }

    case ESP_GAP_BLE_PERIODIC_ADV_SYNC_LOST_EVT:
    {
      const uint16_t handle = param->periodic_adv_sync_lost.sync_handle;
      {
        BLELockGuard lock(inst->mtx);
        auto &v = inst->periodicSyncs;
        v.erase(std::remove(v.begin(), v.end(), handle), v.end());
      }
      BLEScan::PeriodicLostHandler cb;
      {
        BLELockGuard lock(inst->mtx);
        cb = inst->periodicLostCb;
      }
      if (cb) {
        cb(handle);
      }
      break;
    }
#endif /* BLE_PERIODIC_ADV_SUPPORTED */
#endif /* BLE5_SUPPORTED */

    default: break;
  }
}

#else /* !BLE_SCANNING_SUPPORTED -- stubs */

#include "BLE.h"
#include "esp32-hal-log.h"

// Stubs for BLE_SCANNING_SUPPORTED == 0: log; start* return NotSupported or empty results/scan.

void BLEScan::clearDuplicateCache() {}

BTStatus BLEScan::start(uint32_t, bool) {
  log_w("Scanning not supported");
  return BTStatus::NotSupported;
}

BLEScan::Results BLEScan::startBlocking(uint32_t) {
  log_w("Scanning not supported");
  return BLEScan::Results();
}

BTStatus BLEScan::stop() {
  log_w("Scanning not supported");
  return BTStatus::NotSupported;
}

BTStatus BLEScan::stopBackend() {
  return BTStatus::NotSupported;
}

BTStatus BLEScan::startExtended(uint32_t, const ExtScanConfig *, const ExtScanConfig *) {
  log_w("Scanning not supported");
  return BTStatus::NotSupported;
}

BTStatus BLEScan::stopExtended() {
  log_w("Scanning not supported");
  return BTStatus::NotSupported;
}

BTStatus BLEScan::createPeriodicSync(const BTAddress &, uint8_t, uint16_t, uint16_t) {
  log_w("Scanning not supported");
  return BTStatus::NotSupported;
}

BTStatus BLEScan::cancelPeriodicSync() {
  log_w("Scanning not supported");
  return BTStatus::NotSupported;
}

BTStatus BLEScan::terminatePeriodicSync(uint16_t) {
  log_w("Scanning not supported");
  return BTStatus::NotSupported;
}

void BLEScan::terminateAllPeriodicSyncs() {}

void BLEScan::resetBackendEvents() {}

void BLEScan::prepareBackendStart() {}

void BLEScan::beginBackendShutdown() {}

#endif /* BLE_SCANNING_SUPPORTED */

#endif /* BLE_BLUEDROID */
