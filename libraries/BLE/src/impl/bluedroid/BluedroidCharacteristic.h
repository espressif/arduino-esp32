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

#include "BLECharacteristic.h"
#include "BLEService.h"
#include "BluedroidDescriptor.h"
#include "impl/BLEMutex.h"
#include <vector>

struct BLECharacteristic::Impl : std::enable_shared_from_this<BLECharacteristic::Impl> {
  BLEUUID uuid;
  uint16_t handle = 0;
  BLEProperty properties = static_cast<BLEProperty>(0);
  BLEPermission permissions = static_cast<BLEPermission>(0);
  std::vector<uint8_t> value;
  SemaphoreHandle_t valueMtx = xSemaphoreCreateRecursiveMutex();

  ~Impl() {
    if (valueMtx) {
      vSemaphoreDelete(valueMtx);
    }
  }

  std::vector<std::shared_ptr<BLEDescriptor::Impl>> descriptors;
  BLEService::Impl *service = nullptr;

  BLECharacteristic::ReadHandler onReadCb = nullptr;
  BLECharacteristic::WriteHandler onWriteCb = nullptr;
  BLECharacteristic::NotifyHandler onNotifyCb = nullptr;
  BLECharacteristic::SubscribeHandler onSubscribeCb = nullptr;
  BLECharacteristic::StatusHandler onStatusCb = nullptr;

  // Protected by the owning server's mtx (impl->mtx). Both reads (in
  // notify/indicate) and writes (in CCCD write handler / disconnect cleanup)
  // must hold that lock.
  std::vector<std::pair<uint16_t, uint16_t>> subscribers;
};

#endif /* BLE_BLUEDROID */
