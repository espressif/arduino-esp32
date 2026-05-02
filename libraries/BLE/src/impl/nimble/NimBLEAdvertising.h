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

#include "BLEAdvertising.h"

#include <host/ble_gap.h>
#include "impl/BLEMutex.h"
#include <vector>

struct BLEAdvertising::Impl {
  bool advertising = false;
  bool scanResponseEnabled = true;
  bool includeTxPower = false;
  uint16_t appearance = 0;
  uint16_t minInterval = 0x20;
  uint16_t maxInterval = 0x40;
  uint16_t minPreferred = 0;
  uint16_t maxPreferred = 0;
  BLEAdvType advType = BLEAdvType::ConnectableScannable;
  uint8_t filterPolicy = 0;

  std::vector<BLEUUID> serviceUUIDs;
  String deviceName;
  BLEAdvertisementData customAdvData;
  BLEAdvertisementData customScanRspData;
  bool useCustomAdvData = false;
  bool useCustomScanRsp = false;

  BLEAdvertising::CompleteHandler onCompleteCb = nullptr;
  SemaphoreHandle_t mtx = xSemaphoreCreateRecursiveMutex();

  ~Impl() {
    if (mtx) {
      vSemaphoreDelete(mtx);
    }
  }

  static int gapEventCallback(struct ble_gap_event *event, void *arg);
};

#endif /* BLE_NIMBLE */
