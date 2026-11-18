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

/**
 * @file BLEAdvertisingImpl.h
 * @brief Shared, stack-agnostic base for @c BLEAdvertising::Impl (@c BLEAdvertisingImplCommon).
 */

#include "core/BLEGuards.h"
#if BLE_ENABLED

#include "advertising/BLEAdvertising.h"
#include "BTAddress.h"
#include "core/BLEAdmission.h"
#include "core/BLEMutex.h"
#include <atomic>
#include <vector>

struct BLEAdvertisingImplCommon {
  BLEAdmissionGate nativeAdmission;
  std::atomic<bool> isAdvertising{false};
  BLEAdvertising::CompleteHandler onCompleteCb = nullptr;
  SemaphoreHandle_t mtx = xSemaphoreCreateRecursiveMutex();

  std::vector<BLEUUID> serviceUUIDs;
  BLEAdvType advType = BLEAdvType::ConnectableScannable;
  BTAddress directedPeer;
  uint16_t minInterval = 0x20;
  uint16_t maxInterval = 0x40;
  uint16_t minPreferred = 0;
  uint16_t maxPreferred = 0;
  bool includeTxPower = false;
  uint16_t appearance = 0;

  ~BLEAdvertisingImplCommon() {
    if (mtx) {
      vSemaphoreDelete(mtx);
    }
  }
};

#endif /* BLE_ENABLED */
