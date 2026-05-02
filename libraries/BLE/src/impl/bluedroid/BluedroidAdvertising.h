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

#include "BLEAdvertising.h"
#include "impl/BLESync.h"

#include <esp_gap_ble_api.h>
#include <freertos/semphr.h>
#include <vector>

struct BLEAdvertising::Impl {
  bool advertising = false;
  BLESync advSync;
  SemaphoreHandle_t mtx = xSemaphoreCreateRecursiveMutex();
  BLEAdvertising::CompleteHandler onCompleteCb = nullptr;

  ~Impl() {
    if (mtx) {
      vSemaphoreDelete(mtx);
    }
  }

  std::vector<BLEUUID> serviceUUIDs;
  String name;
  bool includeName = true;
  bool scanResp = true;
  BLEAdvType advType = BLEAdvType::ConnectableScannable;
  uint16_t minInterval = 0x20;
  uint16_t maxInterval = 0x40;
  uint16_t minPreferred = 0;
  uint16_t maxPreferred = 0;
  bool includeTxPower = false;
  uint16_t appearance = 0;
  bool scanRequestWhitelistOnly = false;
  bool connectWhitelistOnly = false;

  bool customAdvData = false;
  bool customScanRespData = false;
  std::vector<uint8_t> rawAdvData;
  std::vector<uint8_t> rawScanRespData;

  static Impl *s_instance;
  static void handleGAP(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
};

#endif /* BLE_BLUEDROID */
