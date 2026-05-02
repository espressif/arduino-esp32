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

#include "BLEServer.h"
#include "BLECharacteristic.h"
#include <host/ble_gatt.h>
#include <memory>
#include <vector>

struct BLEService::Impl : std::enable_shared_from_this<BLEService::Impl> {
  BLEUUID uuid;
  uint16_t handle = 0;
  uint8_t instId = 0;
  uint32_t numHandles = 15;
  bool started = false;
  BLEServer::Impl *server = nullptr;
  std::vector<std::shared_ptr<BLECharacteristic::Impl>> characteristics;
  std::vector<std::shared_ptr<BLEService::Impl>> includedServices;
  ble_uuid_any_t nimbleUUID{};
};

// Builds and registers user GATT services with NimBLE.
// Called from BLEServer::start() before ble_gatts_start().
// Defined in NimBLECharacteristic.cpp.
int nimbleRegisterGattServices(const std::vector<std::shared_ptr<BLEService::Impl>> &services);

#endif /* BLE_NIMBLE */
