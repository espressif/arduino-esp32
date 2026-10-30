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

#include "core/BLEGuards.h"
#if BLE_NIMBLE

#include "scan/BLEScan.h"
#include "core/BLESync.h"
#include "core/BLEMutex.h"
#include "core/BLEAdmission.h"

#include <host/ble_gap.h>

#include <atomic>
#include <vector>

// BLEScan is entirely NimBLE-specific (no cross-backend shared state), so
// BLEScan::Impl is defined directly here (a .nimble.* file) with no common base.
struct BLEScan::Impl {
  struct EventContext {
    BLEScan::Impl *impl;
    uint32_t generation;
    bool scanSession;
    bool retireByStop = false;  // Protected by the owning Impl::mtx.
  };

  uint16_t interval = BLE_GAP_SCAN_FAST_INTERVAL_MIN;
  uint16_t window = BLE_GAP_SCAN_FAST_WINDOW;
  bool activeScan = true;
  bool filterDuplicates = true;
  // Set on the user task (startScan/stopScan) and cleared on the NimBLE host
  // task (DISC_COMPLETE); atomic so the public isScanning() getter and the
  // start/stop guards observe it without locking across callbacks.
  std::atomic<bool> isScanning{false};
  uint32_t lastDurationMs = 0;
  // Absolute session deadline. A controller preemption resumes only for the
  // remaining interval instead of starting lastDurationMs again. Zero denotes
  // an unlimited scan. Protected by mtx.
  int64_t sessionDeadlineUs = 0;
  // Identifies the logical scan whose completion may update state and release
  // scanSync. Each native resume after preemption keeps the same generation;
  // a user stop/restart advances it. Protected by mtx.
  uint32_t activeGeneration = 0;
  EventContext *activeScanContext = nullptr;
  std::vector<EventContext *> retiredScanContexts;
  // stop() can receive BLE_HS_EALREADY while NimBLE has temporarily moved the
  // operation into its privacy-preemption slot. Native code still owns the
  // callback argument in that state, so keep it alive until DISC_COMPLETE or
  // until the host has fully stopped.
  std::vector<EventContext *> nativeHeldScanContexts;
  struct ble_npl_event retireScanEvent {};
  bool retireScanEventInited = false;
  BLEAdmissionGate nativeAdmission;
#if BLE5_SUPPORTED
  bool hasUncodedConfig = false;
  uint16_t uncodedInterval = 0;
  uint16_t uncodedWindow = 0;
  bool hasCodedConfig = false;
  uint16_t codedInterval = 0;
  uint16_t codedWindow = 0;
#endif

  BLEScan::ResultHandler onResultCb = nullptr;
  BLEScan::CompleteHandler onCompleteCb = nullptr;

  BLEScan::Results results;
  std::vector<std::pair<BTAddress, uint8_t>> duplicateKeys;
  BLESync scanSync{"scanSync"};
  SemaphoreHandle_t mtx = xSemaphoreCreateRecursiveMutex();
  EventContext periodicContext{this, 0, false};

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
