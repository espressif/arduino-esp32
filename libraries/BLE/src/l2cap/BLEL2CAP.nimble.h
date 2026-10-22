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

#include "core/BLEGuards.h"
#if BLE_L2CAP_SUPPORTED && BLE_NIMBLE

#include "l2cap/BLEL2CAP.h"
#include "core/BLEMutex.h"
#include "core/BLESync.h"
#include <host/ble_hs.h>
#include <host/ble_l2cap.h>
#include <nimble/nimble_npl.h>
#include <os/os_mbuf.h>
#include <atomic>
#include <vector>

void nimbleReleaseL2CAPOperationEvent(BLEL2CAPChannel::Impl *impl);

struct BLEL2CAPChannel::Impl {
  enum class Operation : uint8_t {
    None,
    Write,
    GetMTU,
    Disconnect,
  };

  struct ble_l2cap_chan *chan = nullptr;
  uint16_t psm = 0;
  uint16_t mtu = 0;
  uint16_t connHandle = BLE_HS_CONN_HANDLE_NONE;
  bool connected = false;

  BLEL2CAPChannel::DataHandler onDataCb = nullptr;
  BLEL2CAPChannel::DisconnectHandler onDisconnectCb = nullptr;

  struct ble_npl_event operationEvent {};
  bool operationEventInited = false;
  bool operationHostRunning = false;
  Operation operation = Operation::None;
  std::atomic<bool> operationCancelled{false};
  const uint8_t *writeData = nullptr;
  size_t writeLen = 0;
  size_t writeOffset = 0;
  size_t writeChunkLen = 0;
  size_t writeChunkMax = 0;
  struct os_mbuf *writePendingMbuf = nullptr;
  uint16_t operationMTU = 0;

  BLESync operationSync{"l2capOperationSync"};
  BLESync connectSync{"l2capConnectSync"};

  SemaphoreHandle_t mtx = xSemaphoreCreateRecursiveMutex();
  SemaphoreHandle_t operationMtx = xSemaphoreCreateRecursiveMutex();

  ~Impl() {
    nimbleReleaseL2CAPOperationEvent(this);
    if (operationMtx) {
      vSemaphoreDelete(operationMtx);
    }
    if (mtx) {
      vSemaphoreDelete(mtx);
    }
  }
};

// BLEL2CAPServer is NimBLE-only (no cross-backend shared state), so
// BLEL2CAPServer::Impl is defined directly here (a .nimble.* file) with no common base.
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

/**
 * @brief Configure and register a NimBLE L2CAP CoC server on @p server.
 * @return true on success; false on allocation or stack registration failure.
 */
bool nimbleSetupL2CAPServer(const std::shared_ptr<BLEL2CAPServer::Impl> &server, uint16_t psm, uint16_t mtu);

/**
 * @brief Open an outgoing L2CAP CoC channel on @p chanImpl.
 * @return true on success; false on allocation or connect failure.
 */
bool nimbleSetupL2CAPChannel(const std::shared_ptr<BLEL2CAPChannel::Impl> &chanImpl, uint16_t connHandle, uint16_t psm, uint16_t mtu);
void nimbleStartL2CAPOperations();
void nimblePrepareL2CAPShutdown();
void nimbleResetL2CAPDeferredEvent();
void nimbleCleanupL2CAP();

#endif /* BLE_L2CAP_SUPPORTED && BLE_NIMBLE */
