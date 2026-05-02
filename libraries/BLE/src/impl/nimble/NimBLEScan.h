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

#include "impl/BLEGuards.h"
#if BLE_NIMBLE

#include "BLEScan.h"
#include "impl/BLESync.h"
#include "impl/BLEMutex.h"

#include <host/ble_gap.h>

struct BLEScan::Impl {
  uint16_t interval = BLE_GAP_SCAN_FAST_INTERVAL_MIN;
  uint16_t window = BLE_GAP_SCAN_FAST_WINDOW;
  bool activeScan = true;
  bool filterDuplicates = true;
  bool scanning = false;

  BLEScan::ResultHandler onResultCb = nullptr;
  BLEScan::CompleteHandler onCompleteCb = nullptr;

  BLEScanResults results;
  BLESync scanSync;
  SemaphoreHandle_t mtx = xSemaphoreCreateRecursiveMutex();

  BLEScan::PeriodicSyncHandler periodicSyncCb = nullptr;
  BLEScan::PeriodicReportHandler periodicReportCb = nullptr;
  BLEScan::PeriodicLostHandler periodicLostCb = nullptr;

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
