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

#include "impl/common/BLEGuards.h"
#if BLE_BLUEDROID

#include "BLEScan.h"
#include "impl/common/BLESync.h"
#include "impl/common/BLEMutex.h"

#include <esp_gap_ble_api.h>

#include <atomic>
#include <vector>
#include <algorithm>

// BLEScan is entirely Bluedroid-specific (no cross-backend shared state), so
// BLEScan::Impl is defined directly here in impl/bluedroid/ with no common base.
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
  BLEScan::Results results;
  BLESync scanSync;
  SemaphoreHandle_t mtx = xSemaphoreCreateRecursiveMutex();

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
    if (mtx) {
      vSemaphoreDelete(mtx);
    }
  }

  static BLEScan::Impl *s_instance;
  static void handleGAP(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
};

#endif /* BLE_BLUEDROID */
