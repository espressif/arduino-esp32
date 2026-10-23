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

#pragma once

#include "core/BLEGuards.h"
#if BLE_BLUEDROID

#include "scan/BLEScan.h"
#include "core/BLESync.h"
#include "core/BLEMutex.h"

#include <esp_gap_ble_api.h>
#include <esp_timer.h>

#include <atomic>
#include <vector>
#include <algorithm>

// BLEScan is entirely Bluedroid-specific (no cross-backend shared state), so
// BLEScan::Impl is defined directly here (a .bluedroid.* file) with no common base.
struct BLEScan::Impl {
  // 0x30 = 48 units x 0.625 ms = 30 ms. Interval == window (continuous scan),
  // matching NimBLE's BLE_GAP_SCAN_FAST_INTERVAL_MIN / BLE_GAP_SCAN_FAST_WINDOW
  // defaults so both backends discover with the same default timing.
  uint16_t interval = 0x30;
  uint16_t window = 0x30;
  bool activeScan = true;
  bool filterDuplicates = true;
  // Set on the user task (startScan/stopScan) and cleared on the BTC host task
  // (scan-complete/stop GAP events); atomic so the public isScanning() getter
  // and the start/stop guards observe it without locking across callbacks.
  std::atomic<bool> isScanning{false};
  // Closes native GAP admission before BLE.end() disables the host. The
  // publisher count lets teardown wait for calls that crossed the gate.
  std::atomic<bool> nativeAdmissionOpen{true};
  std::atomic<uint32_t> nativePublishers{0};
  BLEScan::Results results;
  std::vector<std::pair<BTAddress, uint8_t>> duplicateKeys;
  BLESync paramSync{"scanParamSync"};
  BLESync scanSync{"scanSync"};
  BLESync stopSync{"scanStopSync"};
  SemaphoreHandle_t mtx = xSemaphoreCreateRecursiveMutex();

  // Legacy Bluedroid scanning takes whole seconds; a sub-second or fractional
  // duration is run as a continuous scan plus this one-shot timer (created
  // lazily by start()). timerExpiryStop is set by the timer so the resulting
  // SCAN_STOP_COMPLETE dispatches onComplete like a natural duration expiry.
  esp_timer_handle_t durationTimer = nullptr;
  std::atomic<bool> timerExpiryStop{false};

  BLEScan::ResultHandler onResultCb = nullptr;
  BLEScan::CompleteHandler onCompleteCb = nullptr;
  BLEScan::PeriodicSyncHandler periodicSyncCb = nullptr;
  BLEScan::PeriodicReportHandler periodicReportCb = nullptr;
  BLEScan::PeriodicLostHandler periodicLostCb = nullptr;

#if BLE_PERIODIC_ADV_SUPPORTED
  // Active periodic sync handles for terminateAllPeriodicSyncs() on BLE.end().
  std::vector<uint16_t> periodicSyncs;
#endif

  ~Impl() {
    if (durationTimer) {
      esp_timer_stop(durationTimer);
      esp_timer_delete(durationTimer);
    }
    if (mtx) {
      vSemaphoreDelete(mtx);
    }
  }

  static void durationTimerCb(void *arg);

  bool enterNativeCall() {
    if (!nativeAdmissionOpen.load(std::memory_order_seq_cst)) {
      return false;
    }
    nativePublishers.fetch_add(1, std::memory_order_seq_cst);
    if (!nativeAdmissionOpen.load(std::memory_order_seq_cst)) {
      nativePublishers.fetch_sub(1, std::memory_order_seq_cst);
      return false;
    }
    return true;
  }

  void leaveNativeCall() {
    nativePublishers.fetch_sub(1, std::memory_order_seq_cst);
  }

  static BLEScan::Impl *s_instance;
  static void handleGAP(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
};

#endif /* BLE_BLUEDROID */
