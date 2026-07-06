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

#pragma once

#include "impl/common/BLEGuards.h"
#if BLE_NIMBLE

#include "BLEScan.h"
#include "impl/common/BLESync.h"
#include "impl/common/BLEMutex.h"

#include <host/ble_gap.h>

#include <atomic>
#include <vector>

// BLEScan is entirely NimBLE-specific (no cross-backend shared state), so
// BLEScan::Impl is defined directly here in impl/nimble/ with no common base.
struct BLEScan::Impl {
  uint16_t interval = BLE_GAP_SCAN_FAST_INTERVAL_MIN;
  uint16_t window = BLE_GAP_SCAN_FAST_WINDOW;
  bool activeScan = true;
  bool filterDuplicates = true;
  // Set on the user task (startScan/stopScan) and cleared on the NimBLE host
  // task (DISC_COMPLETE); atomic so the public isScanning() getter and the
  // start/stop guards observe it without locking across callbacks.
  std::atomic<bool> isScanning{false};

  BLEScan::ResultHandler onResultCb = nullptr;
  BLEScan::CompleteHandler onCompleteCb = nullptr;

  BLEScan::Results results;
  BLESync scanSync;
  SemaphoreHandle_t mtx = xSemaphoreCreateRecursiveMutex();

  BLEScan::PeriodicSyncHandler periodicSyncCb = nullptr;
  BLEScan::PeriodicReportHandler periodicReportCb = nullptr;
  BLEScan::PeriodicLostHandler periodicLostCb = nullptr;

#if BLE5_SUPPORTED
  // Established periodic-sync handles the library is currently tracking so
  // BLE.end() can terminate them while the host is still enabled (see
  // BLEScan::terminateAllPeriodicSyncs). Populated on BLE_GAP_EVENT_PERIODIC_SYNC,
  // pruned on _SYNC_LOST and on an explicit terminatePeriodicSync(). Guarded by
  // the scan mutex (mtx).
  std::vector<uint16_t> periodicSyncs;
#endif

  ~Impl() {
    if (mtx) {
      vSemaphoreDelete(mtx);
    }
  }

  static int gapEventHandler(struct ble_gap_event *event, void *arg);

  BLEAdvertisedDevice parseDiscEvent(const struct ble_gap_disc_desc *disc);
#if BLE5_SUPPORTED
  BLEAdvertisedDevice parseExtDiscEvent(const struct ble_gap_ext_disc_desc *disc);
#endif
};

#endif /* BLE_NIMBLE */
