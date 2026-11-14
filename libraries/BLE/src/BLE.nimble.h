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
 * @file BLE.nimble.h
 * @brief NimBLE backend state for BLEClass (BLEClass::Impl).
 */

#include "core/BLEGuards.h"
#if BLE_NIMBLE

#include "BLEImpl.h"
#include <host/ble_gap.h>
#include <host/ble_hs.h>
#include <host/ble_att.h>

struct ble_store_status_event;

/**
 * @brief NimBLE backend state for @ref BLEClass (@c BLEClass::Impl).
 *
 * Inherits @ref BLEClassImplCommon for shared lifecycle state and hook slots;
 * as a nested member of @c BLEClass, its static callbacks reach
 * @c BLE._impl / @c BLE._deviceName directly.
 */
struct BLEClass::Impl : BLEClassImplCommon {
  using SecurityOverflowHook = bool (*)(const BTAddress &);

  bool synced = false;
  ble_gap_event_listener gapListener{};
#if BLE_ADVERTISING_SUPPORTED
  BLEAdvertising::Impl *adv = nullptr;
#endif
#if BLE_SCANNING_SUPPORTED
  BLEScan::Impl *scan = nullptr;
  LifecycleHook scanHostStoppedHook = nullptr;
#endif
#if BLE_GATT_SERVER_SUPPORTED
  LifecycleHook serverHostStoppedHook = nullptr;
#endif
#if BLE_L2CAP_SUPPORTED
  LifecycleHook l2capBeginHook = nullptr;
  LifecycleHook l2capPreEndHook = nullptr;
  LifecycleHook l2capHostStoppedHook = nullptr;
  LifecycleHook l2capPostEndHook = nullptr;
#endif
#if BLE_SMP_SUPPORTED
  SecurityOverflowHook securityOverflowHook = nullptr;
#endif

  BTStatus backendBegin(const String &deviceName);
  void backendAfterHooks();
  void backendFinishBegin();
  void backendPreAdvScanEnd();
  void backendPreSessionClose();
  void backendStopAndDeinitHost();
  void backendFinalizeEnd(bool releaseMemory);

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

int nimbleForwardServerGapEvent(struct ble_gap_event *event);
int nimbleForwardSecurityGapEvent(struct ble_gap_event *event);
bool nimbleIsHostTask();

#endif /* BLE_NIMBLE */
