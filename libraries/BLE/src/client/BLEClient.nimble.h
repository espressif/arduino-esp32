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
#if BLE_NIMBLE

#include "client/BLEClientImpl.h"
#include "remote/BLERemoteService.h"
#include "types/BLEConnInfo.nimble.h"
#include "core/BLESync.h"

#include <host/ble_hs.h>
#include <host/ble_gap.h>
#include <host/ble_gatt.h>
#include <vector>
#include <memory>

/**
 * @brief NimBLE GATT-client implementation (@c BLEClient::Impl).
 *
 * Defined in a @c .nimble.* file, so everything here is NimBLE-specific; it inherits the
 * stack-agnostic @c BLEClientImplCommon, giving one uniform @c impl.member type.
 */
struct BLEClient::Impl : BLEClientImplCommon {
  uint16_t connHandle = BLE_HS_CONN_HANDLE_NONE;
  uint16_t preferredMTU = 0;
  // NimBLE can surface peer identity twice per connection (IDENTITY_RESOLVED and
  // ENC_CHANGE); this latches after the first onIdentity dispatch so the callback
  // fires at most once per connection. Reset on each CONNECT event. Guarded by mtx.
  bool identityDispatched = false;
  // A peripheral can start the ATT MTU exchange before NimBLE delivers
  // BLE_GAP_EVENT_CONNECT (CONNECT is deferred behind the LL feature/version
  // reads). Latch the negotiated value here and replay it from the CONNECT
  // handler after onConnect so onMtuChanged is not lost. Guarded by mtx.
  uint16_t pendingMtuValue = 0;

  // Prevent destruction while NimBLE holds our raw pointer.
  // Set before ble_gap_connect(); moved off this member before unblocking the
  // waiter so a reconnect cannot have its keepalive wiped by the dying handler.
  std::shared_ptr<BLEClient::Impl> nimbleRef;

  bool discovering = false;
  BLESync authSync{"clientAuthSync"};

  static int gapEventHandler(struct ble_gap_event *event, void *arg);
  static int serviceDiscoveryCb(uint16_t connHandle, const struct ble_gatt_error *error, const struct ble_gatt_svc *service, void *arg);
  static int mtuExchangeCb(uint16_t connHandle, const struct ble_gatt_error *error, uint16_t mtu, void *arg);

  /**
   * @brief Handle a BLE_GAP_EVENT_MTU for this client.
   *
   * Dispatches onMtuChanged when the event belongs to the live connection, or
   * latches the value when a peer-initiated exchange completed before CONNECT
   * was delivered (the CONNECT handler replays it after onConnect).
   */
  void handleMtuEvent(uint16_t handle, uint16_t value);
};

#endif /* BLE_NIMBLE */
