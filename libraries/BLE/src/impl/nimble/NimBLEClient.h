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

#include "impl/common/BLEGuards.h"
#if BLE_NIMBLE

#include "impl/common/BLEClientImpl.h"
#include "NimBLEConnInfo.h"
#include "impl/common/BLESync.h"

#include <host/ble_hs.h>
#include <host/ble_gap.h>
#include <host/ble_gatt.h>
#include <vector>
#include <memory>

/**
 * @brief NimBLE GATT-client implementation (@c BLEClient::Impl).
 *
 * Defined in @c impl/nimble/, so everything here is NimBLE-specific; it inherits the
 * stack-agnostic @c BLEClientImplCommon, giving one uniform @c impl.member type.
 */
struct BLEClient::Impl : BLEClientImplCommon {
  uint16_t connHandle = BLE_HS_CONN_HANDLE_NONE;
  uint16_t preferredMTU = 0;
  // NimBLE can surface peer identity twice per connection (IDENTITY_RESOLVED and
  // ENC_CHANGE); this latches after the first onIdentity dispatch so the callback
  // fires at most once per connection. Reset on each CONNECT event. Guarded by mtx.
  bool identityDispatched = false;
  BLESync connectSync;
  // Lets the blocking connect() wait for the initial ATT MTU exchange to finish
  // so getMTU() is accurate as soon as connect() returns (ordering contract in
  // DESIGN.md). Signalled by mtuExchangeCb.
  BLESync mtuSync;

  // Prevent destruction while NimBLE holds our raw pointer.
  // Set before ble_gap_connect(); cleared in gapEventHandler after the
  // terminal event (DISCONNECT or CONNECT-failure) has been fully dispatched.
  std::shared_ptr<BLEClient::Impl> nimbleRef;

  struct RemoteServiceEntry {
    BLEUUID uuid;
    uint16_t startHandle;
    uint16_t endHandle;
  };
  std::vector<RemoteServiceEntry> discoveredServices;
  bool discovering = false;
  BLESync discoverSync;

  static int gapEventHandler(struct ble_gap_event *event, void *arg);
  static int serviceDiscoveryCb(uint16_t connHandle, const struct ble_gatt_error *error, const struct ble_gatt_svc *service, void *arg);
  static int mtuExchangeCb(uint16_t connHandle, const struct ble_gatt_error *error, uint16_t mtu, void *arg);
};

#endif /* BLE_NIMBLE */
