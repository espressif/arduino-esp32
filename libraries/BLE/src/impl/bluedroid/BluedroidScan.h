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

#include "impl/BLEGuards.h"
#if BLE_BLUEDROID

#include "BLEScan.h"
#include "impl/BLESync.h"
#include "impl/BLEMutex.h"

#include <esp_gap_ble_api.h>

struct BLEScan::Impl {
  uint16_t interval = 0x50;
  uint16_t window = 0x30;
  bool activeScan = true;
  bool filterDuplicates = true;
  bool scanning = false;
  BLEScanResults results;
  BLESync scanSync;
  SemaphoreHandle_t mtx = xSemaphoreCreateRecursiveMutex();

  BLEScan::ResultHandler onResultCb = nullptr;
  BLEScan::CompleteHandler onCompleteCb = nullptr;
  BLEScan::PeriodicSyncHandler periodicSyncCb = nullptr;
  BLEScan::PeriodicReportHandler periodicReportCb = nullptr;
  BLEScan::PeriodicLostHandler periodicLostCb = nullptr;

  ~Impl() {
    if (mtx) {
      vSemaphoreDelete(mtx);
    }
  }

  static Impl *s_instance;
  static void handleGAP(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
};

#endif /* BLE_BLUEDROID */
