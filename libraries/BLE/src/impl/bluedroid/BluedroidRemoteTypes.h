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

#include "BLEClient.h"
#include "BLERemoteService.h"
#include "BLERemoteCharacteristic.h"
#include "BLERemoteDescriptor.h"
#include "BLEUUID.h"

#include <vector>
#include <memory>
#include <stdint.h>

struct BLERemoteService::Impl : std::enable_shared_from_this<BLERemoteService::Impl> {
  BLEUUID uuid;
  uint16_t startHandle = 0;
  uint16_t endHandle = 0;
  BLEClient::Impl *client = nullptr;
  std::vector<std::shared_ptr<BLERemoteCharacteristic::Impl>> characteristics;
  bool characteristicsRetrieved = false;
};

struct BLERemoteCharacteristic::Impl : std::enable_shared_from_this<BLERemoteCharacteristic::Impl> {
  BLEUUID uuid;
  uint16_t handle = 0;     // Characteristic value handle
  uint16_t defHandle = 0;  // Characteristic definition handle
  uint8_t properties = 0;
  BLERemoteService::Impl *service = nullptr;
  std::vector<std::shared_ptr<BLERemoteDescriptor::Impl>> descriptors;
  bool descriptorsRetrieved = false;
  std::vector<uint8_t> value;  // Cached read value
  BLERemoteCharacteristic::NotifyCallback notifyCb = nullptr;
};

struct BLERemoteDescriptor::Impl {
  BLEUUID uuid;
  uint16_t handle = 0;
  BLERemoteCharacteristic::Impl *chr = nullptr;
  std::vector<uint8_t> value;
};

#endif /* BLE_BLUEDROID */
