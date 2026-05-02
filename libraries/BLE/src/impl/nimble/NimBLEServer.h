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

#include <host/ble_gap.h>
#include "impl/BLEMutex.h"
#include <vector>
#include <memory>

struct BLEServer::Impl : std::enable_shared_from_this<BLEServer::Impl> {
  bool started = false;
  bool advertiseOnDisconnect = true;

  std::vector<std::shared_ptr<BLEService::Impl>> services;

  BLEServer::ConnectHandler onConnectCb = nullptr;
  BLEServer::DisconnectHandler onDisconnectCb = nullptr;
  BLEServer::MtuChangedHandler onMtuChangedCb = nullptr;
  BLEServer::ConnParamsHandler onConnParamsCb = nullptr;
  BLEServer::IdentityHandler onIdentityCb = nullptr;

  std::vector<std::pair<uint16_t, BLEConnInfo>> connections;
  SemaphoreHandle_t mtx = xSemaphoreCreateRecursiveMutex();

  ~Impl() {
    if (mtx) {
      vSemaphoreDelete(mtx);
    }
  }

  void connSet(uint16_t connHandle, const BLEConnInfo &connInfo);
  void connErase(uint16_t connHandle);
  BLEConnInfo *connFind(uint16_t connHandle);

  static int gapEventHandler(struct ble_gap_event *event, void *arg);
  static BLEServer makeHandle(Impl *impl);
};

#endif /* BLE_NIMBLE */
