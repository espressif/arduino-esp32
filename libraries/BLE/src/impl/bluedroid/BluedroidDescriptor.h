/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 * Copyright 2017 Neil Kolban
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

#include "BLEDescriptor.h"
#include "BLECharacteristic.h"
#include "impl/BLEMutex.h"
#include <vector>

struct BLEDescriptor::Impl : std::enable_shared_from_this<BLEDescriptor::Impl> {
  BLEUUID uuid;
  uint16_t handle = 0;
  std::vector<uint8_t> value;
  BLECharacteristic::Impl *chr = nullptr;
  BLEPermission permissions{};  // stored so BluedroidServer::start() can
                                // translate to esp_gatt_perm_t at GATT
                                // registration time, and so the cross-
                                // backend validator can see the logical
                                // permissions
  SemaphoreHandle_t mtx = xSemaphoreCreateRecursiveMutex();

  ~Impl() {
    if (mtx) {
      vSemaphoreDelete(mtx);
    }
  }

  BLEDescriptor::ReadHandler onReadCb = nullptr;
  BLEDescriptor::WriteHandler onWriteCb = nullptr;
};
