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
 * @brief Stack-agnostic shared (common) implementation state for the local GATT
 *        attribute hierarchy: @c BLEService::Impl, @c BLECharacteristic::Impl and
 *        @c BLEDescriptor::Impl. Must stay free of any NimBLE/Bluedroid or native
 *        BT header.
 */

#include "impl/common/BLEGuards.h"
#if BLE_ENABLED

#include "BLEService.h"
#include "BLEServer.h"
#include "BLECharacteristic.h"
#include "BLEDescriptor.h"
#include "impl/common/BLEMutex.h"
#include <vector>
#include <memory>
#include <utility>

/**
 * @brief Shared, stack-agnostic state for a GATT service.
 */
struct BLEServiceImplCommon : std::enable_shared_from_this<BLEService::Impl> {
  BLEUUID uuid;
  uint16_t handle = 0;
  uint8_t instId = 0;
  uint32_t numHandles = 15;
  bool started = false;
  BLEServer::Impl *server = nullptr;
  std::vector<std::shared_ptr<BLECharacteristic::Impl>> characteristics;
  std::vector<std::shared_ptr<BLEService::Impl>> includedServices;

  // Handle factory for backend code that is not a friend of BLEService: mints a
  // handle through the private ctor, which only this befriended ImplCommon can call.
  // Keeps the friend surface to one type per class.
  // See DESIGN.md "Handle construction: makeHandle vs. the private constructor".
  static BLEService makeHandle(const std::shared_ptr<BLEService::Impl> &impl) {
    return BLEService(impl);
  }
};

/**
 * @brief Shared, stack-agnostic state for a GATT characteristic.
 */
struct BLECharacteristicImplCommon : std::enable_shared_from_this<BLECharacteristic::Impl> {
  BLEUUID uuid;
  uint16_t handle = 0;
  BLEProperty properties = static_cast<BLEProperty>(0);
  BLEPermission permissions = static_cast<BLEPermission>(0);
  std::vector<uint8_t> value;
  SemaphoreHandle_t mtx = xSemaphoreCreateRecursiveMutex();

  std::vector<std::shared_ptr<BLEDescriptor::Impl>> descriptors;
  BLEService::Impl *service = nullptr;

  BLECharacteristic::ReadHandler onReadCb = nullptr;
  BLECharacteristic::WriteHandler onWriteCb = nullptr;
  BLECharacteristic::NotifyHandler onNotifyCb = nullptr;
  BLECharacteristic::SubscribeHandler onSubscribeCb = nullptr;
  BLECharacteristic::StatusHandler onStatusCb = nullptr;

  // Protected by the owning server's mtx. Both reads (notify/indicate) and
  // writes (subscribe/unsubscribe/disconnect cleanup) must hold that lock.
  std::vector<std::pair<uint16_t, uint16_t>> subscribers;

  ~BLECharacteristicImplCommon() {
    if (mtx) {
      vSemaphoreDelete(mtx);
    }
  }

  // Handle factory for backend code that is not a friend of BLECharacteristic: mints
  // a handle through the private ctor, which only this befriended ImplCommon can call.
  // Keeps the friend surface to one type per class.
  // See DESIGN.md "Handle construction: makeHandle vs. the private constructor".
  static BLECharacteristic makeHandle(const std::shared_ptr<BLECharacteristic::Impl> &impl) {
    return BLECharacteristic(impl);
  }
};

/**
 * @brief Shared, stack-agnostic state for a GATT descriptor.
 */
struct BLEDescriptorImplCommon : std::enable_shared_from_this<BLEDescriptor::Impl> {
  BLEUUID uuid;
  uint16_t handle = 0;
  std::vector<uint8_t> value;
  BLECharacteristic::Impl *chr = nullptr;
  BLEPermission permissions{};
  SemaphoreHandle_t mtx = xSemaphoreCreateRecursiveMutex();

  BLEDescriptor::ReadHandler onReadCb = nullptr;
  BLEDescriptor::WriteHandler onWriteCb = nullptr;

  ~BLEDescriptorImplCommon() {
    if (mtx) {
      vSemaphoreDelete(mtx);
    }
  }

  // Handle factory for backend code that is not a friend of BLEDescriptor: mints a
  // handle through the private ctor, which only this befriended ImplCommon can call.
  // Keeps the friend surface to one type per class.
  // See DESIGN.md "Handle construction: makeHandle vs. the private constructor".
  static BLEDescriptor makeHandle(const std::shared_ptr<BLEDescriptor::Impl> &impl) {
    return BLEDescriptor(impl);
  }
};

#endif /* BLE_ENABLED */
