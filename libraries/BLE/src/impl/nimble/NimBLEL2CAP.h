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
#if BLE_L2CAP_SUPPORTED && BLE_NIMBLE

#include "BLEL2CAP.h"
#include "impl/BLEMutex.h"
#include <host/ble_l2cap.h>
#include <os/os_mbuf.h>
#include <vector>

struct BLEL2CAPChannel::Impl {
  struct ble_l2cap_chan *chan = nullptr;
  uint16_t psm = 0;
  uint16_t mtu = 0;
  uint16_t connHandle = BLE_HS_CONN_HANDLE_NONE;
  bool connected = false;

  BLEL2CAPChannel::DataHandler onDataCb = nullptr;
  BLEL2CAPChannel::DisconnectHandler onDisconnectCb = nullptr;

  SemaphoreHandle_t mtx = xSemaphoreCreateRecursiveMutex();

  ~Impl() {
    if (mtx) {
      vSemaphoreDelete(mtx);
    }
  }
};

struct BLEL2CAPServer::Impl {
  uint16_t psm = 0;
  uint16_t mtu = 0;
  struct os_mbuf_pool sduPool {};
  struct os_mempool sduMemPool {};
  uint8_t *sduMem = nullptr;

  BLEL2CAPServer::AcceptHandler onAcceptCb = nullptr;
  BLEL2CAPServer::DataHandler onDataCb = nullptr;
  BLEL2CAPServer::DisconnectHandler onDisconnectCb = nullptr;

  std::vector<std::shared_ptr<BLEL2CAPChannel::Impl>> channels;
  SemaphoreHandle_t mtx = xSemaphoreCreateRecursiveMutex();

  ~Impl() {
    if (sduMem) {
      free(sduMem);
    }
    if (mtx) {
      vSemaphoreDelete(mtx);
    }
  }

  static int l2capEvent(struct ble_l2cap_event *event, void *arg);
  struct os_mbuf *allocSdu();
};

#endif /* BLE_L2CAP_SUPPORTED && BLE_NIMBLE */
