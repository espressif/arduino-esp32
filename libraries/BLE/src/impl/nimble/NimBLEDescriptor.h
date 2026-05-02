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

#include "BLECharacteristic.h"
#include "BLEDescriptor.h"
#include "NimBLEService.h"
#include "NimBLEConnInfo.h"

#include "NimBLEUUID.h"
#include <host/ble_hs.h>
#include <host/ble_gatt.h>
#include <host/ble_att.h>
#include "impl/BLEMutex.h"
#include <vector>
#include <memory>

struct BLEDescriptor::Impl : std::enable_shared_from_this<BLEDescriptor::Impl> {
  BLEUUID uuid;
  uint16_t handle = 0;
  std::vector<uint8_t> value;
  BLEDescriptor::ReadHandler onReadCb = nullptr;
  BLEDescriptor::WriteHandler onWriteCb = nullptr;
  BLECharacteristic::Impl *chr = nullptr;
  BLEPermission permissions{};  // stored alongside attFlags so the cross-
                                // backend validator can reason about the
                                // original BLEPermission bits rather than
                                // the NimBLE-native att flags
  uint8_t attFlags = 0;
  ble_uuid_any_t nimbleUUID{};
  SemaphoreHandle_t mtx = xSemaphoreCreateRecursiveMutex();

  ~Impl() {
    if (mtx) {
      vSemaphoreDelete(mtx);
    }
  }
};

#endif /* BLE_NIMBLE */
