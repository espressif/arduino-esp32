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
 * @file BLEAdvertising.bluedroid.cpp
 * @brief Bluedroid (ESP-IDF) implementation of BLE GAP advertising, data setup, and GAP events.
 *
 * Spec references:
 *  - BT Core Spec v5.x, Vol 3, Part C (GAP) — discoverable/connectable modes.
 *  - BT Core Spec v5.x, Vol 6, Part B, §2.3.1 — LE advertising PDU types:
 *    ADV_IND=ConnectableScannable, ADV_DIRECT_IND=ConnectableDirected,
 *    ADV_NONCONN_IND=NonConnectable, ADV_SCAN_IND=ScannableUndirected.
 *  - BT Core Spec v5.x, Vol 6, Part B, §4.4.2 — advertising interval timing:
 *    unit = 0.625 ms; minimum = 0x0020 (20 ms); maximum = 0x4000 (10.24 s).
 *  - BT Core Spec v5.x, Vol 6, Part B, §4.3.3 — advertising filter policy
 *    (scan/connect from all vs. from whitelist only).
 *  - CSS (Supplement to Core Spec), Part A — AD type codes and structures.
 */

#include "core/BLEGuards.h"
#if BLE_BLUEDROID

#if BLE_ADVERTISING_SUPPORTED

#include "BLE.h"
#include "advertising/BLEAdvertising.bluedroid.h"

#include "core/BLESync.h"
#include "core/BLEMutex.h"
#include "core/BLEImplHelpers.h"
#include "advertising/BLEAdvScanHelpers.h"
#include "advertising/BLEAdvertisingData.h"
#include "esp32-hal-log.h"

#include <esp_gap_ble_api.h>
#include <esp_bt_defs.h>
#include <algorithm>
#include <string.h>
#include "core/BLEValidationHooks.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#if BLE_GATT_SERVER_SUPPORTED
void bluedroidCancelPendingReAdvertise();
void bluedroidInvalidatePendingReAdvertise();
SemaphoreHandle_t bluedroidReAdvertiseMutex();

// Taken before nativeAdmission so stop() can wait out an in-flight
// advertiseOnDisconnect start() without inverting the admission gate.
struct ReAdvGuard {
  BLELockGuard lock;
  ReAdvGuard() : lock(bluedroidReAdvertiseMutex()) {
    bluedroidInvalidatePendingReAdvertise();
  }
};
#endif

// API contract is documented on the declarations in the public BLE*.h headers; the definitions below carry implementation notes only.

static const uint32_t ADV_SYNC_TIMEOUT_MS = 2000;

namespace {
struct CompleteDispatch {
  BLEAdvertising::CompleteHandler cb;
  uint8_t instance;
};

void completeDispatchTask(void *arg) {
  auto *job = static_cast<CompleteDispatch *>(arg);
  if (job->cb) {
    job->cb(job->instance);
  }
  delete job;
  vTaskDelete(nullptr);
}

// onComplete runs from duration expiry / ADV_TERMINATED. Those GAP events
// arrive on BTC; start() waits on advSync from that same task, so invoking
// a handler that calls start() inline deadlocks. Defer to a worker.
void dispatchOnComplete(BLEAdvertising::Impl *inst, uint8_t instance) {
  BLEAdvertising::CompleteHandler completeCb;
  {
    BLELockGuard lock(inst->callbackMtx);
    completeCb = inst->onCompleteCb;
  }
  if (!completeCb) {
    return;
  }
  auto *job = new (std::nothrow) CompleteDispatch{std::move(completeCb), instance};
  if (!job) {
    return;
  }
  if (xTaskCreate(completeDispatchTask, "ble-adv-done", 4096, job, tskIDLE_PRIORITY + 1, nullptr) != pdPASS) {
    delete job;
  }
}
}  // namespace

// --------------------------------------------------------------------------
// Static instance for event routing
// --------------------------------------------------------------------------

BLEAdvertising::Impl *BLEAdvertising::Impl::s_instance = nullptr;

void BLEAdvertising::Impl::durationTimerCb(void *arg) {
  auto *inst = static_cast<BLEAdvertising::Impl *>(arg);
  if (!inst || !inst->isAdvertising) {
    return;
  }
  BLEAdmissionGuard admission(inst->nativeAdmission);
  if (!admission) {
    return;
  }
  inst->durationExpiryStop = true;
  esp_err_t err = esp_ble_gap_stop_advertising();
  if (err != ESP_OK) {
    inst->durationExpiryStop = false;
    log_e("advertising duration timer: esp_ble_gap_stop_advertising: %s", esp_err_to_name(err));
  }
}

// --------------------------------------------------------------------------
// Helpers: map BLEAdvType to Bluedroid esp_ble_adv_type_t
// --------------------------------------------------------------------------

#if !BLE5_SUPPORTED
/**
 * @brief Maps the API advertising type to the Bluedroid GAP advertising type constant.
 * @param type High-level BLEAdvType.
 * @return Corresponding esp_ble_adv_type_t.
 * @note PDU type mapping (BT Core Spec v5.x, Vol 6, Part B, §2.3.1):
 *       ADV_IND (ConnectableScannable/Connectable): undirected connectable + scannable.
 *       ADV_DIRECT_IND high duty (DirectedHighDuty): directed connectable, fast advertising.
 *       ADV_DIRECT_IND low duty (DirectedLowDuty/ConnectableDirected): slower directed.
 *       ADV_SCAN_IND (ScannableUndirected): scannable, not connectable.
 *       ADV_NONCONN_IND (NonConnectable): non-connectable, non-scannable.
 * @note Only used by the native-legacy start() path (BLE5_SUPPORTED == 0).
 */
static esp_ble_adv_type_t mapAdvType(BLEAdvType type) {
  switch (type) {
    case BLEAdvType::ConnectableScannable: return ADV_TYPE_IND;
    case BLEAdvType::ConnectableDirected:  return ADV_TYPE_DIRECT_IND_LOW;
    case BLEAdvType::DirectedHighDuty:     return ADV_TYPE_DIRECT_IND_HIGH;
    case BLEAdvType::DirectedLowDuty:      return ADV_TYPE_DIRECT_IND_LOW;
    case BLEAdvType::ScannableUndirected:  return ADV_TYPE_SCAN_IND;
    case BLEAdvType::NonConnectable:       return ADV_TYPE_NONCONN_IND;
    default:                               return ADV_TYPE_IND;
  }
}
#endif /* !BLE5_SUPPORTED */

/**
 * @brief Maps whitelist flags to Bluedroid advertising filter policy.
 * @param scanWL Restrict scan requests to whitelist.
 * @param connectWL Restrict connection requests to whitelist.
 * @return esp_ble_adv_filter_t policy value.
 * @note Filter policy definitions (BT Core Spec v5.x, Vol 6, Part B, §4.3.3):
 *       - All / All: process all scan and connection requests.
 *       - WL / All: scan requests from whitelist only; any connection request.
 *       - All / WL: any scan request; connection requests from whitelist only.
 *       - WL / WL: both scan and connection requests from whitelist only.
 */
static esp_ble_adv_filter_t mapFilterPolicy(bool scanWL, bool connectWL) {
  if (scanWL && connectWL) {
    return ADV_FILTER_ALLOW_SCAN_WLST_CON_WLST;
  }
  if (scanWL) {
    return ADV_FILTER_ALLOW_SCAN_WLST_CON_ANY;
  }
  if (connectWL) {
    return ADV_FILTER_ALLOW_SCAN_ANY_CON_WLST;
  }
  return ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;
}

// --------------------------------------------------------------------------
// GAP event handler
// --------------------------------------------------------------------------

/**
 * @brief Handles Bluedroid GAP advertising data and start/stop completion events for the singleton instance.
 * @param event GAP event code.
 * @param param Event payload from the stack.
 */
void BLEAdvertising::Impl::handleGAP(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  auto *inst = s_instance;
  if (!inst) {
    return;
  }

  switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
      log_d("ADV_DATA_SET_COMPLETE status=%d", param->adv_data_cmpl.status);
      inst->advSync.give(param->adv_data_cmpl.status == ESP_BT_STATUS_SUCCESS ? BTStatus::OK : BTStatus::Fail);
      break;

    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
      log_d("SCAN_RSP_DATA_SET_COMPLETE status=%d", param->scan_rsp_data_cmpl.status);
      inst->advSync.give(param->scan_rsp_data_cmpl.status == ESP_BT_STATUS_SUCCESS ? BTStatus::OK : BTStatus::Fail);
      break;

    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
      log_d("ADV_DATA_RAW_SET_COMPLETE status=%d", param->adv_data_raw_cmpl.status);
      inst->advSync.give(param->adv_data_raw_cmpl.status == ESP_BT_STATUS_SUCCESS ? BTStatus::OK : BTStatus::Fail);
      break;

    case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
      log_d("SCAN_RSP_DATA_RAW_SET_COMPLETE status=%d", param->scan_rsp_data_raw_cmpl.status);
      inst->advSync.give(param->scan_rsp_data_raw_cmpl.status == ESP_BT_STATUS_SUCCESS ? BTStatus::OK : BTStatus::Fail);
      break;

    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
      log_d("ADV_START_COMPLETE status=%d", param->adv_start_cmpl.status);
      if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
        inst->isAdvertising = true;
      }
      inst->advSync.give(param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS ? BTStatus::OK : BTStatus::Fail);
#if BLE_VALIDATION_HOOKS
      if (BLEHooks.adv.injectStaleLegacyStopComplete.exchange(false, std::memory_order_acq_rel)
          && param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
        // Leftover ADV_STOP_COMPLETE after a successful start must not clear
        // isAdvertising: stop() would then no-op while the radio is up.
        esp_ble_gap_cb_param_t stale = {};
        stale.adv_stop_cmpl.status = ESP_BT_STATUS_SUCCESS;
        handleGAP(ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT, &stale);
        BLEHooks.adv.advertisingAfterStaleStop.store(inst->isAdvertising, std::memory_order_relaxed);
      }
#endif
      break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
    {
      log_d("ADV_STOP_COMPLETE status=%d", param->adv_stop_cmpl.status);
      const bool ok = param->adv_stop_cmpl.status == ESP_BT_STATUS_SUCCESS;
      const bool durationExpired = inst->durationExpiryStop.exchange(false);
      if (durationExpired) {
        if (ok) {
          inst->isAdvertising = false;
        }
        // Explicit stop is not session completion (duration / maxEvents), so it
        // must not dispatch onComplete. In particular, start() stops an active
        // session while holding mtx; trying to copy the callback here after
        // waking that waiter blocks the BTC task and prevents the following
        // advertising-data completion event from being delivered.
        if (ok) {
          dispatchOnComplete(inst, 0);
        }
      } else if (inst->giveIfAdvWait(AdvWait::LegacyStop, ok ? BTStatus::OK : BTStatus::Fail) && ok) {
        inst->isAdvertising = false;
      }
      break;
    }

#if BLE5_SUPPORTED
    // BLE5 extended advertising completion events feed the same advSync used by
    // the legacy setup path (operations are issued one at a time).
    case ESP_GAP_BLE_EXT_ADV_SET_PARAMS_COMPLETE_EVT:
      log_d("EXT_ADV_SET_PARAMS_COMPLETE status=%d", param->ext_adv_set_params.status);
      inst->advSync.give(param->ext_adv_set_params.status == ESP_BT_STATUS_SUCCESS ? BTStatus::OK : BTStatus::Fail);
      break;

    case ESP_GAP_BLE_EXT_ADV_DATA_SET_COMPLETE_EVT:
      log_d("EXT_ADV_DATA_SET_COMPLETE status=%d", param->ext_adv_data_set.status);
      inst->advSync.give(param->ext_adv_data_set.status == ESP_BT_STATUS_SUCCESS ? BTStatus::OK : BTStatus::Fail);
      break;

    case ESP_GAP_BLE_EXT_SCAN_RSP_DATA_SET_COMPLETE_EVT:
      log_d("EXT_SCAN_RSP_DATA_SET_COMPLETE status=%d", param->scan_rsp_set.status);
      inst->advSync.give(param->scan_rsp_set.status == ESP_BT_STATUS_SUCCESS ? BTStatus::OK : BTStatus::Fail);
      break;

    case ESP_GAP_BLE_EXT_ADV_START_COMPLETE_EVT:
    {
      log_d("EXT_ADV_START_COMPLETE status=%d", param->ext_adv_start.status);
      // Set the per-instance/legacy flag here on the BTC task, not on the
      // caller after its advSync wait: an ADV_TERMINATED arriving between the
      // give() and the caller-side assignment would be overwritten back to
      // "advertising" by that assignment.
      if (param->ext_adv_start.status == ESP_BT_STATUS_SUCCESS) {
        const uint8_t n = param->ext_adv_start.instance_num < EXT_ADV_NUM_SETS_MAX ? param->ext_adv_start.instance_num : EXT_ADV_NUM_SETS_MAX;
        for (uint8_t i = 0; i < n; i++) {
          const uint8_t h = param->ext_adv_start.instance[i];
          if (h == kLegacyInstance) {
            inst->isAdvertising = true;
          } else if (h < kMaxExtInstances) {
            inst->extInstances[h].isAdvertising = true;
          }
        }
      }
      inst->advSync.give(param->ext_adv_start.status == ESP_BT_STATUS_SUCCESS ? BTStatus::OK : BTStatus::Fail);
#if BLE_VALIDATION_HOOKS
      if (BLEHooks.adv.injectStaleExtStopComplete.exchange(false, std::memory_order_acq_rel)
          && param->ext_adv_start.status == ESP_BT_STATUS_SUCCESS) {
        // Leftover EXT_ADV_STOP_COMPLETE after a successful start must not
        // clear isAdvertising: stop() would then no-op while the radio is up.
        esp_ble_gap_cb_param_t stale = {};
        stale.ext_adv_stop.status = ESP_BT_STATUS_SUCCESS;
        stale.ext_adv_stop.instance_num = param->ext_adv_start.instance_num;
        memcpy(stale.ext_adv_stop.instance, param->ext_adv_start.instance, sizeof(stale.ext_adv_stop.instance));
        handleGAP(ESP_GAP_BLE_EXT_ADV_STOP_COMPLETE_EVT, &stale);
      }
#endif
      break;
    }

    case ESP_GAP_BLE_EXT_ADV_SET_CLEAR_COMPLETE_EVT:
      log_d("EXT_ADV_SET_CLEAR_COMPLETE status=%d", param->ext_adv_clear.status);
      inst->giveIfAdvWait(AdvWait::ExtClear, param->ext_adv_clear.status == ESP_BT_STATUS_SUCCESS ? BTStatus::OK : BTStatus::Fail);
      break;

    case ESP_GAP_BLE_EXT_ADV_STOP_COMPLETE_EVT:
    {
      log_d("EXT_ADV_STOP_COMPLETE status=%d", param->ext_adv_stop.status);
      const bool ok = param->ext_adv_stop.status == ESP_BT_STATUS_SUCCESS;
      // Only wipe flags when this complete matches an armed stop wait. A late
      // leftover stop after start() has already set isAdvertising would leave
      // the radio up while stop() no-ops on the stale false flag.
      if (inst->giveIfAdvWait(AdvWait::ExtStop, ok ? BTStatus::OK : BTStatus::Fail) && ok) {
        const uint8_t n = param->ext_adv_stop.instance_num < EXT_ADV_NUM_SETS_MAX ? param->ext_adv_stop.instance_num : EXT_ADV_NUM_SETS_MAX;
        for (uint8_t i = 0; i < n; i++) {
          const uint8_t h = param->ext_adv_stop.instance[i];
          if (h == kLegacyInstance) {
            inst->isAdvertising = false;
          } else if (h < kMaxExtInstances) {
            inst->extInstances[h].isAdvertising = false;
          }
        }
      }
      break;
    }

    case ESP_GAP_BLE_ADV_TERMINATED_EVT:
    {
      uint8_t handle = param->adv_terminate.adv_instance;
      if (handle == kLegacyInstance) {
        inst->isAdvertising = false;
      } else if (handle < kMaxExtInstances) {
        inst->extInstances[handle].isAdvertising = false;
      }
      // 0x3c = Advertising Timeout (duration expired), 0x43 = Limit Reached (maxEvents expired).
      if (param->adv_terminate.status == 0x3c || param->adv_terminate.status == 0x43) {
        dispatchOnComplete(inst, (handle == kLegacyInstance) ? 0 : handle);
      }
      break;
    }
#if BLE_PERIODIC_ADV_SUPPORTED
    case ESP_GAP_BLE_PERIODIC_ADV_SET_PARAMS_COMPLETE_EVT:
      log_d("PERIODIC_ADV_SET_PARAMS_COMPLETE status=%d", param->peroid_adv_set_params.status);
      inst->advSync.give(param->peroid_adv_set_params.status == ESP_BT_STATUS_SUCCESS ? BTStatus::OK : BTStatus::Fail);
      break;
    case ESP_GAP_BLE_PERIODIC_ADV_DATA_SET_COMPLETE_EVT:
      log_d("PERIODIC_ADV_DATA_SET_COMPLETE status=%d", param->period_adv_data_set.status);
      inst->advSync.give(param->period_adv_data_set.status == ESP_BT_STATUS_SUCCESS ? BTStatus::OK : BTStatus::Fail);
      break;
    case ESP_GAP_BLE_PERIODIC_ADV_START_COMPLETE_EVT:
      log_d("PERIODIC_ADV_START_COMPLETE status=%d", param->period_adv_start.status);
      if (param->period_adv_start.instance < kMaxExtInstances) {
        inst->extInstances[param->period_adv_start.instance].periodicAdvRunning = (param->period_adv_start.status == ESP_BT_STATUS_SUCCESS);
      }
      inst->advSync.give(param->period_adv_start.status == ESP_BT_STATUS_SUCCESS ? BTStatus::OK : BTStatus::Fail);
#if BLE_VALIDATION_HOOKS
      if (BLEHooks.adv.injectStalePeriodicStopComplete.exchange(false, std::memory_order_acq_rel)
          && param->period_adv_start.status == ESP_BT_STATUS_SUCCESS) {
        esp_ble_gap_cb_param_t stale = {};
        stale.period_adv_stop.status = ESP_BT_STATUS_SUCCESS;
        stale.period_adv_stop.instance = param->period_adv_start.instance;
        handleGAP(ESP_GAP_BLE_PERIODIC_ADV_STOP_COMPLETE_EVT, &stale);
        if (param->period_adv_start.instance < kMaxExtInstances) {
          BLEHooks.adv.periodicRunningAfterStaleStop.store(
            inst->extInstances[param->period_adv_start.instance].periodicAdvRunning.load(std::memory_order_relaxed),
            std::memory_order_relaxed
          );
        }
      }
#endif
      break;
    case ESP_GAP_BLE_PERIODIC_ADV_STOP_COMPLETE_EVT:
    {
      log_d("PERIODIC_ADV_STOP_COMPLETE status=%d", param->period_adv_stop.status);
      const bool ok = param->period_adv_stop.status == ESP_BT_STATUS_SUCCESS;
      if (inst->giveIfAdvWait(AdvWait::PeriodicStop, ok ? BTStatus::OK : BTStatus::Fail) && ok
          && param->period_adv_stop.instance < kMaxExtInstances) {
        inst->extInstances[param->period_adv_stop.instance].periodicAdvRunning = false;
      }
      break;
    }
#endif /* BLE_PERIODIC_ADV_SUPPORTED */
#endif /* BLE5_SUPPORTED */

    default: break;
  }
}

void BLEAdvertising::Impl::armAdvWait(AdvWait event) {
  advWait.store(event, std::memory_order_release);
}

bool BLEAdvertising::Impl::giveIfAdvWait(AdvWait event, BTStatus status) {
  AdvWait expected = event;
  if (!advWait.compare_exchange_strong(expected, AdvWait::None, std::memory_order_acq_rel)) {
    return false;
  }
  advSync.give(status);
  return true;
}

BTStatus BLEAdvertising::Impl::finishAdvWait() {
  BTStatus st = advSync.wait(ADV_SYNC_TIMEOUT_MS);
  advWait.store(AdvWait::None, std::memory_order_release);
  return st;
}

// --------------------------------------------------------------------------
// Configuration setters
// --------------------------------------------------------------------------

void BLEAdvertising::setName(const String &name) {
  BLE_CHECK_IMPL();
  impl.name = name;
  if (name.length() > 0) {
    esp_ble_gap_set_device_name(name.c_str());
  }
}

void BLEAdvertising::setScanResponse(bool enable) {
  BLE_CHECK_IMPL();
  impl.scanResp = enable;
}

void BLEAdvertising::setType(BLEAdvType type) {
  BLE_CHECK_IMPL();
  impl.advType = type;
}

void BLEAdvertising::setInterval(uint16_t minMs, uint16_t maxMs) {
  BLE_CHECK_IMPL();
  impl.minInterval = bleMsToUnits0625(minMs);
  impl.maxInterval = bleMsToUnits0625(maxMs);
}

void BLEAdvertising::setMinPreferred(uint16_t val) {
  BLE_CHECK_IMPL();
  impl.minPreferred = val;
}

void BLEAdvertising::setMaxPreferred(uint16_t val) {
  BLE_CHECK_IMPL();
  impl.maxPreferred = val;
}

void BLEAdvertising::setTxPower(bool include) {
  BLE_CHECK_IMPL();
  impl.includeTxPower = include;
}

void BLEAdvertising::setAppearance(uint16_t appearance) {
  BLE_CHECK_IMPL();
  impl.appearance = appearance;
}

void BLEAdvertising::setScanFilter(bool scanRequestWhitelistOnly, bool connectWhitelistOnly) {
  BLE_CHECK_IMPL();
  impl.scanRequestWhitelistOnly = scanRequestWhitelistOnly;
  impl.connectWhitelistOnly = connectWhitelistOnly;
}

void BLEAdvertising::reset() {
  BLE_CHECK_IMPL();
#if BLE_GATT_SERVER_SUPPORTED
  ReAdvGuard reAdv;
#endif
  BLEAdmissionGuard admission(impl.nativeAdmission);
  if (admission && (impl.durationTimer || impl.isAdvertising)) {
    (void)stopBackend();
  }
  impl.serviceUUIDs.clear();
  if (impl.name.length() > 0) {
    impl.name = "";
    if (admission) {
      esp_ble_gap_set_device_name(BLE.getDeviceName().c_str());
    }
  }
  impl.scanResp = true;
  impl.advType = BLEAdvType::ConnectableScannable;
  impl.directedPeer = BTAddress();
  impl.minInterval = 0x20;
  impl.maxInterval = 0x40;
  impl.minPreferred = 0;
  impl.maxPreferred = 0;
  impl.includeTxPower = false;
  impl.appearance = 0;
  impl.scanRequestWhitelistOnly = false;
  impl.connectWhitelistOnly = false;
  impl.customAdvData = false;
  impl.customScanRespData = false;
  impl.rawAdvData.clear();
  impl.rawScanRespData.clear();
#if BLE5_SUPPORTED
  if (admission) {
    // Stop every instance from this already-admitted reset() without
    // re-entering the gate through public stopExtended/stopPeriodicAdv.
    impl.stopAllExtendedThenClear();
  }
  for (uint8_t i = 0; i < Impl::kMaxExtInstances; i++) {
    impl.extInstances[i].reset();
  }
#endif
  // Closed-gate reset() must still clear software flags (DESIGN.md lifecycle).
  // BLE4.2 has no BLE5 block; stopBackend() may also have failed to wait.
  impl.isAdvertising = false;
}

// --------------------------------------------------------------------------
// Custom advertisement / scan response data
// --------------------------------------------------------------------------

void BLEAdvertising::setAdvertisementData(const BLEAdvertisementData &data) {
  BLE_CHECK_IMPL();
  impl.rawAdvData.assign(data.data(), data.data() + data.length());
  impl.customAdvData = true;
}

void BLEAdvertising::setScanResponseData(const BLEAdvertisementData &data) {
  BLE_CHECK_IMPL();
  impl.rawScanRespData.assign(data.data(), data.data() + data.length());
  impl.customScanRespData = true;
}

// --------------------------------------------------------------------------
// start()
// --------------------------------------------------------------------------

BTStatus BLEAdvertising::start(uint32_t durationMs) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
#if BLE_GATT_SERVER_SUPPORTED
  ReAdvGuard reAdv;
#endif
  BLEAdmissionGuard admission(impl.nativeAdmission);
  if (!admission) {
    return BTStatus::InvalidState;
  }
  if (!bleGapDurationMsValid(durationMs)) {
    return BTStatus::InvalidParam;
  }
  if (impl.advType == BLEAdvType::DirectedHighDuty && !bleHighDutyDirectedDurationMsValid(durationMs)) {
    return BTStatus::InvalidParam;
  }
  if (bleAdvTypeIsDirected(impl.advType) && impl.directedPeer.isZero()) {
    return BTStatus::InvalidParam;
  }

  BLELockGuard lock(impl.mtx);

#if BLE5_SUPPORTED
  // On BLE5-capable Bluedroid the public "legacy" advertising runs over the
  // extended API with a legacy PDU on the reserved instance.
  return impl.startLegacyViaExtAdv(durationMs);
#else
  if (impl.durationTimer || impl.isAdvertising) {
    BTStatus stopStatus = stopBackend();
    if (stopStatus != BTStatus::OK) {
      return stopStatus;
    }
  }
  log_d("start: customAdv=%d, customScanResp=%d, scanResp=%d", impl.customAdvData, impl.customScanRespData, impl.scanResp);

  BTStatus result = BTStatus::OK;

  // --- Configure advertising data ---
  if (impl.customAdvData) {
    // Raw advertising data supplied by user
    impl.advSync.take();
    esp_err_t err = esp_ble_gap_config_adv_data_raw(impl.rawAdvData.data(), impl.rawAdvData.size());
    if (err != ESP_OK) {
      log_e("esp_ble_gap_config_adv_data_raw: %s", esp_err_to_name(err));
      impl.advSync.give(BTStatus::Fail);
      result = BTStatus::Fail;
      return result;
    }
    result = impl.advSync.wait(ADV_SYNC_TIMEOUT_MS);
    if (!result) {
      log_e("Timeout waiting for raw adv data set");
      return result;
    }

    // Raw scan response if provided; otherwise clear any prior scan response.
    impl.advSync.take();
    if (impl.customScanRespData) {
      err = esp_ble_gap_config_scan_rsp_data_raw(impl.rawScanRespData.data(), impl.rawScanRespData.size());
    } else {
      // IDF accepts (NULL, 0): only (NULL, nonzero) is ESP_ERR_INVALID_ARG.
      err = esp_ble_gap_config_scan_rsp_data_raw(nullptr, 0);
    }
    if (err != ESP_OK) {
      log_e("esp_ble_gap_config_scan_rsp_data_raw: %s", esp_err_to_name(err));
      impl.advSync.give(BTStatus::Fail);
      result = BTStatus::Fail;
      return result;
    }
    result = impl.advSync.wait(ADV_SYNC_TIMEOUT_MS);
    if (!result) {
      log_e("Timeout waiting for raw scan resp data set");
      return result;
    }
  } else {
    // Serialize the AD payload ourselves and hand raw bytes to the controller,
    // instead of letting Bluedroid rebuild it via esp_ble_adv_data_t (which
    // force-expands every service UUID to 128 bits). This produces the compact
    // 16/32-bit UUID encoding and the exact same byte layout as the NimBLE and
    // Bluedroid extended-advertising paths (CSS Part A §1.2: the Local Name is
    // carried in the scan response when scannable, otherwise in the primary AD).
    const bool scannable = (impl.advType == BLEAdvType::ConnectableScannable || impl.advType == BLEAdvType::ScannableUndirected);
    String advName = impl.name.length() > 0 ? impl.name : BLE.getDeviceName();
    const bool nameInScanRsp = impl.scanResp && scannable;

    BLEAdvertisementData advData = BLEAdvDataBuilder::buildLegacyAdvData(
      advName, impl.serviceUUIDs, impl.appearance, impl.includeTxPower, BLE.getPower(), nameInScanRsp, impl.minPreferred, impl.maxPreferred
    );
    impl.advSync.take();
    esp_err_t err = esp_ble_gap_config_adv_data_raw(const_cast<uint8_t *>(advData.data()), advData.length());
    if (err != ESP_OK) {
      log_e("esp_ble_gap_config_adv_data_raw: %s", esp_err_to_name(err));
      impl.advSync.give(BTStatus::Fail);
      result = BTStatus::Fail;
      return result;
    }
    result = impl.advSync.wait(ADV_SYNC_TIMEOUT_MS);
    if (!result) {
      log_e("Timeout waiting for adv data set");
      return result;
    }

    // Scan response is only valid for scannable PDUs; clear any prior scan response when disabled.
    BLEAdvertisementData scanRespData;
    impl.advSync.take();
    if (nameInScanRsp) {
      scanRespData = BLEAdvDataBuilder::buildLegacyScanRespData(advName);
      err = esp_ble_gap_config_scan_rsp_data_raw(const_cast<uint8_t *>(scanRespData.data()), scanRespData.length());
    } else {
      // IDF accepts (NULL, 0): only (NULL, nonzero) is ESP_ERR_INVALID_ARG.
      err = esp_ble_gap_config_scan_rsp_data_raw(nullptr, 0);
    }
    if (err != ESP_OK) {
      log_e("esp_ble_gap_config_scan_rsp_data_raw (scan resp): %s", esp_err_to_name(err));
      impl.advSync.give(BTStatus::Fail);
      result = BTStatus::Fail;
      return result;
    }
    result = impl.advSync.wait(ADV_SYNC_TIMEOUT_MS);
    if (!result) {
      log_e("Timeout waiting for scan resp data set");
      return result;
    }
  }

  {
    // --- Build advertising parameters ---
    esp_ble_adv_params_t advParams = {};
    advParams.adv_int_min = impl.minInterval;
    advParams.adv_int_max = impl.maxInterval;
    advParams.adv_type = mapAdvType(impl.advType);
    advParams.own_addr_type = static_cast<esp_ble_addr_type_t>(BLE.getOwnAddressType());
    advParams.channel_map = ADV_CHNL_ALL;
    advParams.adv_filter_policy = mapFilterPolicy(impl.scanRequestWhitelistOnly, impl.connectWhitelistOnly);
    if (bleAdvTypeIsDirected(impl.advType)) {
      impl.directedPeer.toEspBdAddr(advParams.peer_addr);
      advParams.peer_addr_type = static_cast<esp_ble_addr_type_t>(impl.directedPeer.type());
    } else {
      advParams.peer_addr_type = BLE_ADDR_TYPE_PUBLIC;
    }

    // --- Start advertising ---
    impl.advSync.take();
    esp_err_t err = esp_ble_gap_start_advertising(&advParams);
    if (err != ESP_OK) {
      log_e("esp_ble_gap_start_advertising: %s", esp_err_to_name(err));
      impl.advSync.give(BTStatus::Fail);
      result = BTStatus::Fail;
      return result;
    }
    result = impl.advSync.wait(ADV_SYNC_TIMEOUT_MS);
    if (!result) {
      log_e("Failed waiting for adv start (status=%d)", (int)impl.advSync.status());
      return result;
    }
  }

  log_d("Advertising started");
  if (durationMs != 0) {
    if (!impl.durationTimer) {
      esp_timer_create_args_t timerArgs = {};
      timerArgs.callback = BLEAdvertising::Impl::durationTimerCb;
      timerArgs.arg = &impl;
      timerArgs.name = "ble_adv_dur";
      timerArgs.dispatch_method = ESP_TIMER_TASK;
      esp_err_t timerErr = esp_timer_create(&timerArgs, &impl.durationTimer);
      if (timerErr != ESP_OK) {
        log_e("advertising duration timer create: %s", esp_err_to_name(timerErr));
        (void)stopBackend();
        return BTStatus::Fail;
      }
    }
    esp_timer_stop(impl.durationTimer);
    esp_err_t timerErr = esp_timer_start_once(impl.durationTimer, static_cast<uint64_t>(durationMs) * 1000ULL);
    if (timerErr != ESP_OK) {
      log_e("advertising duration timer start: %s", esp_err_to_name(timerErr));
      (void)stopBackend();
      return BTStatus::Fail;
    }
  }
  return result;
#endif /* BLE5_SUPPORTED */
}

// --------------------------------------------------------------------------
// stop()
// --------------------------------------------------------------------------

BTStatus BLEAdvertising::stop() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
#if BLE_GATT_SERVER_SUPPORTED
  ReAdvGuard reAdv;
#endif
  BLEAdmissionGuard admission(impl.nativeAdmission);
  if (!admission) {
    return BTStatus::OK;
  }
  return stopBackend();
}

BTStatus BLEAdvertising::stopBackend() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
#if BLE_GATT_SERVER_SUPPORTED
  // Invalidate without taking reAdvMtx: callers that must wait out an in-flight
  // restart take ReAdvGuard before admission. Taking the mutex here while the
  // admission gate is already held would deadlock the restart callback, which
  // holds reAdvMtx and then takes admission inside startAdvertising().
  bluedroidInvalidatePendingReAdvertise();
#endif

  BLELockGuard lock(impl.mtx);

  if (impl.durationTimer) {
    esp_err_t timerErr = esp_timer_stop_blocking(impl.durationTimer, portMAX_DELAY);
    if (timerErr != ESP_OK && timerErr != ESP_ERR_INVALID_STATE) {
      log_e("advertising duration timer stop: %s", esp_err_to_name(timerErr));
      return BTStatus::Fail;
    }
  }
  impl.advSync.take();
  const bool timerStopInFlight = impl.durationExpiryStop.exchange(false);
  if (!impl.isAdvertising && !timerStopInFlight) {
    impl.advSync.give(BTStatus::OK);
    return BTStatus::OK;
  }

#if BLE5_SUPPORTED
  uint8_t stopInstance = Impl::kLegacyInstance;
  impl.armAdvWait(Impl::AdvWait::ExtStop);
  esp_err_t err = esp_ble_gap_ext_adv_stop(1, &stopInstance);
#if BLE_VALIDATION_HOOKS
  if (BLEHooks.adv.injectNativeStopFail.exchange(false, std::memory_order_acq_rel)) {
    err = ESP_FAIL;
  }
#endif
  if (err != ESP_OK) {
    log_e("esp_ble_gap_ext_adv_stop: %s", esp_err_to_name(err));
    impl.armAdvWait(Impl::AdvWait::None);
    impl.advSync.give(BTStatus::Fail);
    return BTStatus::Fail;
  }
  BTStatus s = impl.finishAdvWait();
  if (!s) {
    log_e("Timeout waiting for ext adv stop");
    return s;
  }
  impl.isAdvertising = false;
  log_d("Advertising stopped");
  return BTStatus::OK;
#else
  impl.armAdvWait(Impl::AdvWait::LegacyStop);
  if (!timerStopInFlight) {
    esp_err_t err = esp_ble_gap_stop_advertising();
#if BLE_VALIDATION_HOOKS
    if (BLEHooks.adv.injectNativeStopFail.exchange(false, std::memory_order_acq_rel)) {
      err = ESP_FAIL;
    }
#endif
    if (err != ESP_OK) {
      log_e("esp_ble_gap_stop_advertising: %s", esp_err_to_name(err));
      impl.armAdvWait(Impl::AdvWait::None);
      impl.advSync.give(BTStatus::Fail);
      return BTStatus::Fail;
    }
  }
  BTStatus s = impl.finishAdvWait();
  if (!s) {
    log_e("Timeout waiting for adv stop");
    return s;
  }
  impl.isAdvertising = false;

  log_d("Advertising stopped");
  return BTStatus::OK;
#endif /* BLE5_SUPPORTED */
}

// --------------------------------------------------------------------------
// onComplete
// --------------------------------------------------------------------------

void BLEAdvertising::onComplete(CompleteHandler h) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.callbackMtx);
  impl.onCompleteCb = std::move(h);
}

void BLEAdvertising::resetCallbacks() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.callbackMtx);
  impl.onCompleteCb = nullptr;
}

// --------------------------------------------------------------------------
// BLE5 extended advertising (TX) -- Bluedroid
//
// Compiled only on BLE5-capable Bluedroid silicon (BLE5_SUPPORTED). Periodic
// advertising TX is compiled when BLE_PERIODIC_ADV_TX_SUPPORTED (BLE5 +
// CONFIG_BT_BLE_50_PERIODIC_ADV_EN).
// --------------------------------------------------------------------------

#if BLE5_SUPPORTED

// Extended (non-legacy) PDU property mask, assembled from the shared neutral
// property flags. An extended PDU cannot be both connectable and scannable
// (Vol 6, Part B, §4.4.2.4), which advTypeToExtAdvProps already enforces.
static esp_ble_ext_adv_type_mask_t extAdvTypeMask(BLEAdvType type) {
  const BLEExtAdvProps props = advTypeToExtAdvProps(type, /*legacyPdu=*/false);
  esp_ble_ext_adv_type_mask_t mask = ESP_BLE_GAP_SET_EXT_ADV_PROP_NONCONN_NONSCANNABLE_UNDIRECTED;
  if (props.connectable) {
    mask |= ESP_BLE_GAP_SET_EXT_ADV_PROP_CONNECTABLE;
  }
  if (props.scannable) {
    mask |= ESP_BLE_GAP_SET_EXT_ADV_PROP_SCANNABLE;
  }
  if (props.highDutyDirected) {
    mask |= ESP_BLE_GAP_SET_EXT_ADV_PROP_HD_DIRECTED;
  } else if (props.directed) {
    mask |= ESP_BLE_GAP_SET_EXT_ADV_PROP_DIRECTED;
  }
  return mask;
}

// Legacy-PDU property mask (used by the reserved legacy set so legacy-only peers
// still discover us). Derived from the same neutral flags with legacy semantics.
static esp_ble_ext_adv_type_mask_t legacyExtAdvTypeMask(BLEAdvType type) {
  const BLEExtAdvProps props = advTypeToExtAdvProps(type, /*legacyPdu=*/true);
  if (props.directed) {
    return props.highDutyDirected ? ESP_BLE_GAP_SET_EXT_ADV_PROP_LEGACY_HD_DIR : ESP_BLE_GAP_SET_EXT_ADV_PROP_LEGACY_LD_DIR;
  }
  if (props.connectable) {  // legacy connectable undirected (ADV_IND) is always scannable
    return ESP_BLE_GAP_SET_EXT_ADV_PROP_LEGACY_IND;
  }
  if (props.scannable) {
    return ESP_BLE_GAP_SET_EXT_ADV_PROP_LEGACY_SCAN;
  }
  return ESP_BLE_GAP_SET_EXT_ADV_PROP_LEGACY_NONCONN;
}

BLEAdvertising::Impl::ExtInstance *BLEAdvertising::Impl::extInstanceAt(uint8_t instance) {
  // The top instance is reserved for the legacy start()/stop() path.
  if (instance >= kLegacyInstance) {
    return nullptr;
  }
  ExtInstance *extInst = &extInstances[instance];
  if (!extInst->slotInitialized) {
    extInst->slotInitialized = true;
    extInst->params.type = ESP_BLE_GAP_SET_EXT_ADV_PROP_CONNECTABLE;
    extInst->params.interval_min = 0x30;  // 30 ms in 0.625 ms units
    extInst->params.interval_max = 0x60;  // 60 ms
    extInst->params.channel_map = ADV_CHNL_ALL;
    extInst->params.own_addr_type = static_cast<esp_ble_addr_type_t>(BLE.getOwnAddressType());
    extInst->params.filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;
    extInst->params.primary_phy = ESP_BLE_GAP_PRI_PHY_1M;
    extInst->params.secondary_phy = ESP_BLE_GAP_PHY_1M;
    extInst->params.tx_power = EXT_ADV_TX_PWR_NO_PREFERENCE;
    extInst->params.sid = 0;
#if BLE_PERIODIC_ADV_SUPPORTED
    // Default periodic interval 80 ms (units of 1.25 ms); properties=0 omits TX power.
    extInst->periodicParams.interval_min = 64;
    extInst->periodicParams.interval_max = 64;
    extInst->periodicParams.properties = 0;
#endif
  }
  return extInst;
}

BTStatus BLEAdvertising::Impl::ensureExtConfigured(uint8_t instance) {
  auto *extInst = extInstanceAt(instance);
  if (!extInst) {
    return BTStatus::InvalidParam;
  }
  if (extInst->extAdvRegistered) {
    return BTStatus::OK;
  }
  advSync.take();
  esp_err_t err = esp_ble_gap_ext_adv_set_params(instance, &extInst->params);
  if (err != ESP_OK) {
    log_e("esp_ble_gap_ext_adv_set_params inst %u: %s", instance, esp_err_to_name(err));
    advSync.give(BTStatus::Fail);
    return BTStatus::Fail;
  }
  BTStatus st = advSync.wait(ADV_SYNC_TIMEOUT_MS);
  if (st) {
    extInst->extAdvRegistered = true;
  }
  return st;
}

BTStatus BLEAdvertising::Impl::startLegacyViaExtAdv(uint32_t durationMs) {
  if (isAdvertising) {
    uint8_t stopInstance = kLegacyInstance;
    advSync.take();
    armAdvWait(AdvWait::ExtStop);
    if (esp_ble_gap_ext_adv_stop(1, &stopInstance) == ESP_OK) {
      (void)finishAdvWait();
    } else {
      armAdvWait(AdvWait::None);
      advSync.give(BTStatus::Fail);
    }
    isAdvertising = false;
  }

  esp_ble_gap_ext_adv_params_t params = {};
  params.type = legacyExtAdvTypeMask(advType);
  params.interval_min = minInterval;
  params.interval_max = maxInterval;
  params.channel_map = ADV_CHNL_ALL;
  params.own_addr_type = static_cast<esp_ble_addr_type_t>(BLE.getOwnAddressType());
  if (bleAdvTypeIsDirected(advType)) {
    directedPeer.toEspBdAddr(params.peer_addr);
    params.peer_addr_type = static_cast<esp_ble_addr_type_t>(directedPeer.type());
  } else {
    params.peer_addr_type = BLE_ADDR_TYPE_PUBLIC;
  }
  params.filter_policy = mapFilterPolicy(scanRequestWhitelistOnly, connectWhitelistOnly);
  params.primary_phy = ESP_BLE_GAP_PRI_PHY_1M;  // legacy PDUs are 1M only
  params.secondary_phy = ESP_BLE_GAP_PHY_1M;
  params.sid = 0;
  params.tx_power = EXT_ADV_TX_PWR_NO_PREFERENCE;
  params.scan_req_notif = false;

  esp_err_t err;
  // ADV_DIRECT_IND carries no AD or SCAN_RSP (Vol 6, Part B, §2.3.1.2). Drop
  // leftover payload before Set_Ext_Adv_Params: high-duty directed with data
  // is Invalid HCI Command Parameters (Vol 4, Part E, §7.8.53).
  if (bleAdvTypeIsDirected(advType)) {
    advSync.take();
    err = esp_ble_gap_config_ext_adv_data_raw(kLegacyInstance, 0, nullptr);
    if (err == ESP_OK) {
      (void)advSync.wait(ADV_SYNC_TIMEOUT_MS);
    } else {
      advSync.give(BTStatus::Fail);
    }
    advSync.take();
    err = esp_ble_gap_config_ext_scan_rsp_data_raw(kLegacyInstance, 0, nullptr);
    if (err == ESP_OK) {
      (void)advSync.wait(ADV_SYNC_TIMEOUT_MS);
    } else {
      advSync.give(BTStatus::Fail);
    }
  }

  advSync.take();
  err = esp_ble_gap_ext_adv_set_params(kLegacyInstance, &params);
  if (err != ESP_OK) {
    log_e("legacy ext_adv_set_params: %s", esp_err_to_name(err));
    advSync.give(BTStatus::Fail);
    return BTStatus::Fail;
  }
  BTStatus st = advSync.wait(ADV_SYNC_TIMEOUT_MS);
  if (!st) {
    return st;
  }

  const bool scannable = (advType == BLEAdvType::ConnectableScannable || advType == BLEAdvType::ScannableUndirected);
  String advName = name.length() > 0 ? name : BLE.getDeviceName();
  const bool nameInScanRsp = scanResp && scannable;

  // ADV_DIRECT_IND carries only AdvA and InitA (Vol 6, Part B, §2.3.1.2).
  if (!bleAdvTypeIsDirected(advType)) {
  {
    // Point directly at the source bytes (a member vector for custom data, or a
    // locally built payload) instead of copying into a temporary; the payload
    // stays alive across the config call and its completion wait.
    BLEAdvertisementData built;
    const uint8_t *advPtr = rawAdvData.data();
    size_t advLen = rawAdvData.size();
    if (!customAdvData) {
      built =
        BLEAdvDataBuilder::buildLegacyAdvData(advName, serviceUUIDs, appearance, includeTxPower, BLE.getPower(), nameInScanRsp, minPreferred, maxPreferred);
      advPtr = built.data();
      advLen = built.length();
    }
    advSync.take();
    err = esp_ble_gap_config_ext_adv_data_raw(kLegacyInstance, advLen, advPtr);
    if (err != ESP_OK) {
      log_e("legacy config_ext_adv_data_raw: %s", esp_err_to_name(err));
      advSync.give(BTStatus::Fail);
      return BTStatus::Fail;
    }
    st = advSync.wait(ADV_SYNC_TIMEOUT_MS);
    if (!st) {
      return st;
    }
  }

  if (scannable && scanResp) {
    BLEAdvertisementData built;
    const uint8_t *rspPtr = rawScanRespData.data();
    size_t rspLen = rawScanRespData.size();
    if (!customScanRespData) {
      built = BLEAdvDataBuilder::buildLegacyScanRespData(advName);
      rspPtr = built.data();
      rspLen = built.length();
    }
    advSync.take();
    err = esp_ble_gap_config_ext_scan_rsp_data_raw(kLegacyInstance, rspLen, rspPtr);
    if (err != ESP_OK) {
      log_e("legacy config_ext_scan_rsp_data_raw: %s", esp_err_to_name(err));
      advSync.give(BTStatus::Fail);
      return BTStatus::Fail;
    }
    st = advSync.wait(ADV_SYNC_TIMEOUT_MS);
    if (!st) {
      return st;
    }
  } else {
    // Clearing a prior scan response is required when scanResp is now off;
    // otherwise the controller keeps radiating the last SCAN_RSP.
    advSync.take();
    err = esp_ble_gap_config_ext_scan_rsp_data_raw(kLegacyInstance, 0, nullptr);
    if (err != ESP_OK) {
      log_e("legacy clear ext scan rsp: %s", esp_err_to_name(err));
      advSync.give(BTStatus::Fail);
      return BTStatus::Fail;
    }
    st = advSync.wait(ADV_SYNC_TIMEOUT_MS);
    if (!st) {
      return st;
    }
  }
  }

  esp_ble_gap_ext_adv_t startSet = {};
  startSet.instance = kLegacyInstance;
  startSet.duration = bleGapDurationMsTo10ms(durationMs);
  startSet.max_events = 0;
  advSync.take();
  err = esp_ble_gap_ext_adv_start(1, &startSet);
  if (err != ESP_OK) {
    log_e("legacy ext_adv_start: %s", esp_err_to_name(err));
    advSync.give(BTStatus::Fail);
    return BTStatus::Fail;
  }
  // isAdvertising is set by the EXT_ADV_START_COMPLETE handler (BTC task).
  return advSync.wait(ADV_SYNC_TIMEOUT_MS);
}

void BLEAdvertising::Impl::stopAllExtendedThenClear() {
  for (uint8_t i = 0; i < kMaxExtInstances; i++) {
#if BLE_PERIODIC_ADV_SUPPORTED
    if (extInstances[i].periodicAdvRunning.load()) {
      advSync.take();
      armAdvWait(AdvWait::PeriodicStop);
      esp_err_t perr = esp_ble_gap_periodic_adv_stop(i);
      if (perr != ESP_OK) {
        log_e("periodic_adv_stop inst %u: %s", i, esp_err_to_name(perr));
        armAdvWait(AdvWait::None);
        advSync.give(BTStatus::Fail);
        return;
      }
      if (!finishAdvWait()) {
        log_e("periodic_adv_stop inst %u wait failed", i);
        return;
      }
    }
#endif
    const bool running = (i == kLegacyInstance) ? isAdvertising.load() : extInstances[i].isAdvertising.load();
    if (!running) {
      continue;
    }
    uint8_t inst = i;
    advSync.take();
    armAdvWait(AdvWait::ExtStop);
    esp_err_t err = esp_ble_gap_ext_adv_stop(1, &inst);
#if BLE_VALIDATION_HOOKS
    if (BLEHooks.adv.injectExtStopFail.exchange(false, std::memory_order_acq_rel)) {
      err = ESP_FAIL;
    }
#endif
    if (err != ESP_OK) {
      log_e("ext_adv_stop inst %u: %s", i, esp_err_to_name(err));
      armAdvWait(AdvWait::None);
      advSync.give(BTStatus::Fail);
      return;
    }
    if (!finishAdvWait()) {
      log_e("ext_adv_stop inst %u wait failed", i);
      return;
    }
  }
  advSync.take();
  armAdvWait(AdvWait::ExtClear);
  esp_err_t clearErr = esp_ble_gap_ext_adv_set_clear();
  if (clearErr != ESP_OK) {
    log_e("esp_ble_gap_ext_adv_set_clear: %s", esp_err_to_name(clearErr));
    armAdvWait(AdvWait::None);
    advSync.give(BTStatus::Fail);
    return;
  }
  (void)finishAdvWait();
}

void BLEAdvertising::setExtType(uint8_t instance, BLEAdvType type) {
  BLE_CHECK_IMPL();
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    log_e("setExtType: instance %u out of range", instance);
    return;
  }
  extInst->params.type = extAdvTypeMask(type);
  extInst->extAdvRegistered = false;
}

void BLEAdvertising::setExtPhy(uint8_t instance, BLEPhy primaryPhy, BLEPhy secondaryPhy) {
  BLE_CHECK_IMPL();
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    log_e("setExtPhy: instance %u out of range", instance);
    return;
  }
  // BLEPhy enum values match ESP_BLE_GAP_PHY_* codes (1M=1, 2M=2, Coded=3).
  extInst->params.primary_phy = static_cast<esp_ble_gap_pri_phy_t>(primaryPhy);
  extInst->params.secondary_phy = static_cast<esp_ble_gap_phy_t>(secondaryPhy);
  extInst->extAdvRegistered = false;
}

void BLEAdvertising::setExtTxPower(uint8_t instance, int8_t txPower) {
  BLE_CHECK_IMPL();
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    log_e("setExtTxPower: instance %u out of range", instance);
    return;
  }
  extInst->params.tx_power = txPower;
  extInst->extAdvRegistered = false;
}

void BLEAdvertising::setExtInterval(uint8_t instance, uint16_t minInterval, uint16_t maxInterval) {
  BLE_CHECK_IMPL();
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    log_e("setExtInterval: instance %u out of range", instance);
    return;
  }
  extInst->params.interval_min = minInterval;
  extInst->params.interval_max = maxInterval;
  extInst->extAdvRegistered = false;
}

void BLEAdvertising::setExtChannelMap(uint8_t instance, uint8_t channelMap) {
  BLE_CHECK_IMPL();
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    log_e("setExtChannelMap: instance %u out of range", instance);
    return;
  }
  extInst->params.channel_map = static_cast<esp_ble_adv_channel_t>(channelMap);
  extInst->extAdvRegistered = false;
}

void BLEAdvertising::setExtSID(uint8_t instance, uint8_t sid) {
  BLE_CHECK_IMPL();
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    log_e("setExtSID: instance %u out of range", instance);
    return;
  }
  extInst->params.sid = sid;
  extInst->extAdvRegistered = false;
}

void BLEAdvertising::setExtAnonymous(uint8_t instance, bool anonymous) {
  BLE_CHECK_IMPL();
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    log_e("setExtAnonymous: instance %u out of range", instance);
    return;
  }
  if (anonymous) {
    extInst->params.type |= ESP_BLE_GAP_SET_EXT_ADV_PROP_ANON_ADV;
  } else {
    extInst->params.type &= ~ESP_BLE_GAP_SET_EXT_ADV_PROP_ANON_ADV;
  }
  extInst->extAdvRegistered = false;
}

void BLEAdvertising::setExtIncludeTxPower(uint8_t instance, bool include) {
  BLE_CHECK_IMPL();
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    log_e("setExtIncludeTxPower: instance %u out of range", instance);
    return;
  }
  if (include) {
    extInst->params.type |= ESP_BLE_GAP_SET_EXT_ADV_PROP_INCLUDE_TX_PWR;
  } else {
    extInst->params.type &= ~ESP_BLE_GAP_SET_EXT_ADV_PROP_INCLUDE_TX_PWR;
  }
  extInst->extAdvRegistered = false;
}

void BLEAdvertising::setExtScanReqNotify(uint8_t instance, bool enable) {
  BLE_CHECK_IMPL();
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    log_e("setExtScanReqNotify: instance %u out of range", instance);
    return;
  }
  extInst->params.scan_req_notif = enable;
  extInst->extAdvRegistered = false;
}

BTStatus BLEAdvertising::setExtAdvertisementData(uint8_t instance, const BLEAdvertisementData &data) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BTStatus st = impl.ensureExtConfigured(instance);
  if (!st) {
    return st;
  }
  impl.advSync.take();
  esp_err_t err = esp_ble_gap_config_ext_adv_data_raw(instance, data.length(), data.data());
  if (err != ESP_OK) {
    log_e("config_ext_adv_data_raw inst %u: %s", instance, esp_err_to_name(err));
    impl.advSync.give(BTStatus::Fail);
    return BTStatus::Fail;
  }
  return impl.advSync.wait(ADV_SYNC_TIMEOUT_MS);
}

BTStatus BLEAdvertising::setExtScanResponseData(uint8_t instance, const BLEAdvertisementData &data) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  // A scan-response payload can only be staged on an already-configured set, so
  // apply the instance's accumulated params first.
  BTStatus st = impl.ensureExtConfigured(instance);
  if (!st) {
    return st;
  }
  impl.advSync.take();
  esp_err_t err = esp_ble_gap_config_ext_scan_rsp_data_raw(instance, data.length(), data.data());
  if (err != ESP_OK) {
    log_e("config_ext_scan_rsp_data_raw inst %u: %s", instance, esp_err_to_name(err));
    impl.advSync.give(BTStatus::Fail);
    return BTStatus::Fail;
  }
  return impl.advSync.wait(ADV_SYNC_TIMEOUT_MS);
}

BTStatus BLEAdvertising::setExtInstanceAddress(uint8_t instance, const BTAddress &addr) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BTStatus st = impl.ensureExtConfigured(instance);
  if (!st) {
    return st;
  }
  esp_bd_addr_t bd;
  addr.toEspBdAddr(bd);
  esp_err_t err = esp_ble_gap_ext_adv_set_rand_addr(instance, bd);
  if (err != ESP_OK) {
    log_e("ext_adv_set_rand_addr inst %u: %s", instance, esp_err_to_name(err));
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

BTStatus BLEAdvertising::startExtended(uint8_t instance, uint32_t durationMs, uint8_t maxEvents) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
#if BLE_GATT_SERVER_SUPPORTED
  ReAdvGuard reAdv;
#endif
  BLEAdmissionGuard admission(impl.nativeAdmission);
  if (!admission) {
    return BTStatus::InvalidState;
  }
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    return BTStatus::InvalidParam;
  }
  if (!bleGapDurationMsValid(durationMs)) {
    return BTStatus::InvalidParam;
  }
  if ((extInst->params.type & ESP_BLE_GAP_SET_EXT_ADV_PROP_HD_DIRECTED) && !bleHighDutyDirectedDurationMsValid(durationMs)) {
    return BTStatus::InvalidParam;
  }
  // Ensure params are applied even if the caller started without setting data.
  BTStatus st = impl.ensureExtConfigured(instance);
  if (!st) {
    return st;
  }
  esp_ble_gap_ext_adv_t startSet = {};
  startSet.instance = instance;
  startSet.duration = bleGapDurationMsTo10ms(durationMs);
  startSet.max_events = maxEvents;
  impl.advSync.take();
  esp_err_t err = esp_ble_gap_ext_adv_start(1, &startSet);
  if (err != ESP_OK) {
    log_e("ext_adv_start inst %u: %s", instance, esp_err_to_name(err));
    impl.advSync.give(BTStatus::Fail);
    return BTStatus::Fail;
  }
  // extInst->isAdvertising is set by the EXT_ADV_START_COMPLETE handler.
  return impl.advSync.wait(ADV_SYNC_TIMEOUT_MS);
}

BTStatus BLEAdvertising::stopExtended(uint8_t instance) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
#if BLE_GATT_SERVER_SUPPORTED
  ReAdvGuard reAdv;
#endif
  BLEAdmissionGuard admission(impl.nativeAdmission);
  if (!admission) {
    return BTStatus::OK;
  }
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    return BTStatus::InvalidParam;
  }
  uint8_t inst = instance;
  impl.advSync.take();
  impl.armAdvWait(Impl::AdvWait::ExtStop);
  esp_err_t err = esp_ble_gap_ext_adv_stop(1, &inst);
  if (err != ESP_OK) {
    log_e("ext_adv_stop inst %u: %s", instance, esp_err_to_name(err));
    impl.armAdvWait(Impl::AdvWait::None);
    impl.advSync.give(BTStatus::Fail);
    extInst->isAdvertising = false;
    return BTStatus::Fail;
  }
  BTStatus st = impl.finishAdvWait();
  extInst->isAdvertising = false;
  return st;
}

BTStatus BLEAdvertising::removeExtended(uint8_t instance) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  esp_err_t err = esp_ble_gap_ext_adv_set_remove(instance);
  if (err != ESP_OK) {
    log_e("ext_adv_set_remove inst %u: %s", instance, esp_err_to_name(err));
    return BTStatus::Fail;
  }
  if (instance < Impl::kMaxExtInstances) {
    impl.extInstances[instance].reset();
  }
  return BTStatus::OK;
}

BTStatus BLEAdvertising::clearExtended() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
#if BLE_GATT_SERVER_SUPPORTED
  ReAdvGuard reAdv;
#endif
  BLEAdmissionGuard admission(impl.nativeAdmission);
  if (!admission) {
    return BTStatus::OK;
  }
  impl.advSync.take();
  impl.armAdvWait(Impl::AdvWait::ExtClear);
  esp_err_t err = esp_ble_gap_ext_adv_set_clear();
  if (err != ESP_OK) {
    log_e("ext_adv_set_clear: %s", esp_err_to_name(err));
    impl.armAdvWait(Impl::AdvWait::None);
    impl.advSync.give(BTStatus::Fail);
    return BTStatus::Fail;
  }
  BTStatus st = impl.finishAdvWait();
  if (!st) {
    return st;
  }
  impl.isAdvertising = false;
  for (uint8_t i = 0; i < Impl::kMaxExtInstances; i++) {
    impl.extInstances[i].reset();
  }
  return st;
}

#if BLE_PERIODIC_ADV_SUPPORTED

BTStatus BLEAdvertising::Impl::ensurePeriodicConfigured(uint8_t instance) {
  auto *extInst = extInstanceAt(instance);
  if (!extInst) {
    return BTStatus::InvalidParam;
  }
  if (extInst->periodicAdvRegistered) {
    return BTStatus::OK;
  }
  advSync.take();
  esp_err_t err = esp_ble_gap_periodic_adv_set_params(instance, &extInst->periodicParams);
  if (err != ESP_OK) {
    log_e("esp_ble_gap_periodic_adv_set_params inst %u: %s", instance, esp_err_to_name(err));
    advSync.give(BTStatus::Fail);
    return BTStatus::Fail;
  }
  BTStatus st = advSync.wait(ADV_SYNC_TIMEOUT_MS);
  if (st) {
    extInst->periodicAdvRegistered = true;
  }
  return st;
}

void BLEAdvertising::setPeriodicAdvInterval(uint8_t instance, uint16_t minInterval, uint16_t maxInterval) {
  BLE_CHECK_IMPL();
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    log_e("setPeriodicAdvInterval: instance %u out of range", instance);
    return;
  }
  extInst->periodicParams.interval_min = minInterval;
  extInst->periodicParams.interval_max = maxInterval;
  extInst->periodicAdvRegistered = false;
}

void BLEAdvertising::setPeriodicAdvTxPower(uint8_t instance, bool include) {
  BLE_CHECK_IMPL();
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    log_e("setPeriodicAdvTxPower: instance %u out of range", instance);
    return;
  }
  // BT Core Spec: Include TxPower is bit 6 of the periodic advertising properties.
  if (include) {
    extInst->periodicParams.properties |= 0x40;
  } else {
    extInst->periodicParams.properties &= static_cast<uint16_t>(~0x40);
  }
  extInst->periodicAdvRegistered = false;
}

BTStatus BLEAdvertising::setPeriodicAdvData(uint8_t instance, const BLEAdvertisementData &data) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    return BTStatus::InvalidParam;
  }
  BTStatus st = impl.ensureExtConfigured(instance);
  if (!st) {
    return st;
  }
  st = impl.ensurePeriodicConfigured(instance);
  if (!st) {
    return st;
  }
  impl.advSync.take();
#if defined(CONFIG_BT_BLE_FEAT_PERIODIC_ADV_ENH)
  esp_err_t err = esp_ble_gap_config_periodic_adv_data_raw(instance, data.length(), data.data(), false);
#else
  esp_err_t err = esp_ble_gap_config_periodic_adv_data_raw(instance, data.length(), data.data());
#endif
  if (err != ESP_OK) {
    log_e("esp_ble_gap_config_periodic_adv_data_raw inst %u: %s", instance, esp_err_to_name(err));
    impl.advSync.give(BTStatus::Fail);
    return BTStatus::Fail;
  }
  return impl.advSync.wait(ADV_SYNC_TIMEOUT_MS);
}

BTStatus BLEAdvertising::startPeriodicAdv(uint8_t instance) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
#if BLE_GATT_SERVER_SUPPORTED
  ReAdvGuard reAdv;
#endif
  BLEAdmissionGuard admission(impl.nativeAdmission);
  if (!admission) {
    return BTStatus::InvalidState;
  }
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    return BTStatus::InvalidParam;
  }
  BTStatus st = impl.ensureExtConfigured(instance);
  if (!st) {
    return st;
  }
  st = impl.ensurePeriodicConfigured(instance);
  if (!st) {
    return st;
  }
  impl.advSync.take();
#if defined(CONFIG_BT_BLE_FEAT_PERIODIC_ADV_ENH)
  esp_err_t err = esp_ble_gap_periodic_adv_start(instance, false);
#else
  esp_err_t err = esp_ble_gap_periodic_adv_start(instance);
#endif
  if (err != ESP_OK) {
    log_e("esp_ble_gap_periodic_adv_start inst %u: %s", instance, esp_err_to_name(err));
    impl.advSync.give(BTStatus::Fail);
    return BTStatus::Fail;
  }
  return impl.advSync.wait(ADV_SYNC_TIMEOUT_MS);
}

BTStatus BLEAdvertising::stopPeriodicAdv(uint8_t instance) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
#if BLE_GATT_SERVER_SUPPORTED
  ReAdvGuard reAdv;
#endif
  BLEAdmissionGuard admission(impl.nativeAdmission);
  if (!admission) {
    return BTStatus::OK;
  }
  impl.advSync.take();
  impl.armAdvWait(Impl::AdvWait::PeriodicStop);
  esp_err_t err = esp_ble_gap_periodic_adv_stop(instance);
  if (err != ESP_OK) {
    log_e("esp_ble_gap_periodic_adv_stop inst %u: %s", instance, esp_err_to_name(err));
    impl.armAdvWait(Impl::AdvWait::None);
    impl.advSync.give(BTStatus::Fail);
    return BTStatus::Fail;
  }
  return impl.finishAdvWait();
}

#endif /* BLE_PERIODIC_ADV_SUPPORTED */

#endif /* BLE5_SUPPORTED */

void BLEAdvertising::prepareBackendStart() {
  if (_impl) {
    _impl->nativeAdmission.reopen();
  }
}

void BLEAdvertising::beginBackendShutdown() {
  if (!_impl) {
    return;
  }
  _impl->nativeAdmission.closeAndWait();
  (void)stopBackend();
#if BLE5_SUPPORTED
  // Do not trust software flags here: a closed-gate reset() can clear them
  // without issuing native stops. Unconditionally stop every instance and wait
  // before HCI_LE_Clear_Advertising_Sets (Vol 4, Part E, §7.8.60).
  _impl->stopAllExtendedThenClear();
#endif
  reset();
}

#else /* !BLE_ADVERTISING_SUPPORTED -- stubs */

#include "BLE.h"
#include "esp32-hal-log.h"

// Stubs for BLE_ADVERTISING_SUPPORTED == 0: log or no-op; start/stop return NotSupported.

void BLEAdvertising::setName(const String &) {
  log_w("Advertising not supported");
}

void BLEAdvertising::setScanResponse(bool) {
  log_w("Advertising not supported");
}

void BLEAdvertising::setType(BLEAdvType) {
  log_w("Advertising not supported");
}

void BLEAdvertising::setInterval(uint16_t, uint16_t) {
  log_w("Advertising not supported");
}

void BLEAdvertising::setMinPreferred(uint16_t) {
  log_w("Advertising not supported");
}

void BLEAdvertising::setMaxPreferred(uint16_t) {
  log_w("Advertising not supported");
}

void BLEAdvertising::setTxPower(bool) {
  log_w("Advertising not supported");
}

void BLEAdvertising::setAppearance(uint16_t) {
  log_w("Advertising not supported");
}

void BLEAdvertising::setScanFilter(bool, bool) {
  log_w("Advertising not supported");
}

void BLEAdvertising::reset() {}

void BLEAdvertising::prepareBackendStart() {}

void BLEAdvertising::beginBackendShutdown() {}

BTStatus BLEAdvertising::stopBackend() {
  return BTStatus::NotSupported;
}

void BLEAdvertising::setAdvertisementData(const BLEAdvertisementData &) {
  log_w("Advertising not supported");
}

void BLEAdvertising::setScanResponseData(const BLEAdvertisementData &) {
  log_w("Advertising not supported");
}

BTStatus BLEAdvertising::start(uint32_t) {
  log_w("Advertising not supported");
  return BTStatus::NotSupported;
}

BTStatus BLEAdvertising::stop() {
  log_w("Advertising not supported");
  return BTStatus::NotSupported;
}

void BLEAdvertising::onComplete(CompleteHandler) {}

void BLEAdvertising::resetCallbacks() {}

#endif /* BLE_ADVERTISING_SUPPORTED */

// BLE5 extended + periodic advertising TX are implemented above under BLE5_SUPPORTED /
// BLE_PERIODIC_ADV_SUPPORTED. When those guards are off, the shared NotSupported
// fallbacks in BLEAdvertising.cpp provide the public entry points.

#endif /* BLE_BLUEDROID */
