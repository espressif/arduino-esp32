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

#include "BLEClient.h"
#include "NimBLEConnInfo.h"
#include "impl/BLESync.h"

#include <host/ble_hs.h>
#include <host/ble_gap.h>
#include <host/ble_gatt.h>
#include "impl/BLEMutex.h"
#include <vector>
#include <memory>

struct BLEClient::Impl : std::enable_shared_from_this<BLEClient::Impl> {
  uint16_t connHandle = BLE_HS_CONN_HANDLE_NONE;
  BTAddress peerAddress;
  bool connected = false;
  uint16_t preferredMTU = 0;
  BLESync connectSync;
  SemaphoreHandle_t mtx = xSemaphoreCreateRecursiveMutex();

  // Prevent destruction while NimBLE holds our raw pointer.
  // Set before ble_gap_connect(); cleared in gapEventHandler after the
  // terminal event (DISCONNECT or CONNECT-failure) has been fully dispatched.
  std::shared_ptr<Impl> nimbleRef;

  ~Impl() {
    if (mtx) {
      vSemaphoreDelete(mtx);
    }
  }

  BLEClient::ConnectHandler onConnectCb = nullptr;
  BLEClient::DisconnectHandler onDisconnectCb = nullptr;
  BLEClient::ConnectFailHandler onConnectFailCb = nullptr;
  BLEClient::MtuChangedHandler onMtuChangedCb = nullptr;
  BLEClient::ConnParamsReqHandler onConnParamsReqCb = nullptr;
  BLEClient::IdentityHandler onIdentityCb = nullptr;

  struct RemoteServiceEntry {
    BLEUUID uuid;
    uint16_t startHandle;
    uint16_t endHandle;
  };
  std::vector<RemoteServiceEntry> discoveredServices;
  bool discovering = false;
  BLESync discoverSync;

  static int gapEventHandler(struct ble_gap_event *event, void *arg);
  static BLEClient makeHandle(Impl *impl);
  static int serviceDiscoveryCb(uint16_t connHandle, const struct ble_gatt_error *error, const struct ble_gatt_svc *service, void *arg);
  static int mtuExchangeCb(uint16_t connHandle, const struct ble_gatt_error *error, uint16_t mtu, void *arg);
};

#endif /* BLE_NIMBLE */
