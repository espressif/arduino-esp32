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
 * @file
 * @brief Stack-agnostic shared (common) implementation state for the remote
 *        (client-side) GATT hierarchy: @c BLERemoteService::Impl,
 *        @c BLERemoteCharacteristic::Impl and @c BLERemoteDescriptor::Impl. Must
 *        stay free of any NimBLE/Bluedroid or native BT header.
 *
 * Both backends discovered the same remote-attribute layout (identity, handle
 * ranges, owning parent, discovered child caches, cached values, notify
 * callback), so it lives here once. Each backend's @c *::Impl inherits the
 * matching base and adds only its stack-specific discovery plumbing (connection
 * handle, sync objects, GATT callbacks).
 */

#include "impl/common/BLEGuards.h"
#if BLE_ENABLED

#include "BLERemoteService.h"
#include "BLERemoteCharacteristic.h"
#include "BLERemoteDescriptor.h"
#include "BLEClient.h"
#include "BLEUUID.h"

#include <vector>
#include <memory>
#include <stdint.h>

/**
 * @brief Shared, stack-agnostic state for a remote (client-side) GATT service.
 */
struct BLERemoteServiceImplCommon : std::enable_shared_from_this<BLERemoteService::Impl> {
  BLEUUID uuid;
  uint16_t startHandle = 0;
  uint16_t endHandle = 0;
  BLEClient::Impl *client = nullptr;
  std::vector<std::shared_ptr<BLERemoteCharacteristic::Impl>> characteristics;
  bool characteristicsDiscovered = false;
};

/**
 * @brief Shared, stack-agnostic state for a remote (client-side) GATT characteristic.
 *
 * Provides the @c enable_shared_from_this hook and the @c makeHandle helper so
 * backend code that is not a friend of @c BLERemoteCharacteristic can still build
 * a handle.
 */
struct BLERemoteCharacteristicImplCommon : std::enable_shared_from_this<BLERemoteCharacteristic::Impl> {
  BLEUUID uuid;
  uint16_t defHandle = 0;  // Characteristic definition handle
  uint16_t handle = 0;     // Characteristic value handle
  uint8_t properties = 0;  // Bitmask of BLEProperty (spec Vol 3 Part G 3.3.1.1)
  BLERemoteService::Impl *service = nullptr;
  std::vector<std::shared_ptr<BLERemoteDescriptor::Impl>> descriptors;
  bool descriptorsDiscovered = false;
  std::vector<uint8_t> value;  // Cached read/notified value
  BLERemoteCharacteristic::NotifyCallback onNotifyCb = nullptr;

  // Handle factory for backend code that is not a friend of BLERemoteCharacteristic:
  // mints a handle through the private ctor, which only this befriended ImplCommon can
  // call. Keeps the friend surface to one type per class.
  // See DESIGN.md "Handle construction: makeHandle vs. the private constructor".
  static BLERemoteCharacteristic makeHandle(const std::shared_ptr<BLERemoteCharacteristic::Impl> &impl) {
    return BLERemoteCharacteristic(impl);
  }
};

/**
 * @brief Shared, stack-agnostic state for a remote (client-side) GATT descriptor.
 */
struct BLERemoteDescriptorImplCommon {
  BLEUUID uuid;
  uint16_t handle = 0;
  BLERemoteCharacteristic::Impl *chr = nullptr;
  std::vector<uint8_t> value;
};

#endif /* BLE_ENABLED */
