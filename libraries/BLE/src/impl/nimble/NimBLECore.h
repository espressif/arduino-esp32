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

/**
 * @file NimBLECore.h
 * @brief NimBLE backend state for BLEClass (BLEClass::Impl).
 */

#include "impl/common/BLEGuards.h"
#if BLE_NIMBLE

#include "BLE.h"
#include <host/ble_gap.h>
#include <host/ble_hs.h>
#include <host/ble_att.h>

struct ble_store_status_event;

/**
 * @brief NimBLE backend state for @ref BLEClass (@c BLEClass::Impl).
 *
 * BLEClass is entirely NimBLE-specific (no cross-backend shared state), so this
 * has no common base. As a nested member of BLEClass, its static callbacks reach
 * BLE._impl / BLE._deviceName directly.
 */
struct BLEClass::Impl {
  bool synced = false;
  uint16_t localMTU = BLE_ATT_MTU_DFLT;
  uint8_t ownAddrType = BLE_OWN_ADDR_PUBLIC;
  ble_gap_event_listener gapListener{};
  BLEClass::RawEventHandler customGapHandler = nullptr;

  /** @brief FreeRTOS task body that runs the NimBLE host until the port is stopped. */
  static void hostTask(void *param);

  static void onReset(int reason);
  static void onSync();

  /**
   * @brief Handles bond-store-full events; may ask security to evict a peer before default store rotation.
   * @param event NimBLE store status event.
   * @param arg Opaque callback context from NimBLE.
   * @return 0 to consume overflow handling after bond notification, or the result of `ble_store_util_status_rr`.
   */
  static int onStoreStatus(struct ble_store_status_event *event, void *arg);
};

#endif /* BLE_NIMBLE */
