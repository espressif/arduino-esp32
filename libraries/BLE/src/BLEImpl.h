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
 * @file BLEImpl.h
 * @brief Shared, stack-agnostic base for @c BLEClass::Impl (@c BLEClassImplCommon).
 *
 * Holds the lifecycle state, mutex, hook critical section, and lazy role
 * begin/end hook slots shared across NimBLE and Bluedroid so @c BLE.cpp owns
 * the single @c begin() / @c end() orchestration and common shorthands.
 */

#include "core/BLEGuards.h"
#if BLE_ENABLED

#include "BLE.h"
#include "core/BLEMutex.h"
#include "freertos/task.h"

struct BLEClassImplCommon {
  using LifecycleHook = void (*)();

  uint16_t localMTU = 23;
  uint8_t ownAddrType = 0;
  bool ending = false;
  bool endRunning = false;
  bool endReleaseMemory = false;
  TaskHandle_t endTask = nullptr;
  StaticSemaphore_t lifecycleMtxStorage{};
  SemaphoreHandle_t lifecycleMtx = xSemaphoreCreateRecursiveMutexStatic(&lifecycleMtxStorage);
  portMUX_TYPE hookMux = portMUX_INITIALIZER_UNLOCKED;
  BLEClass::RawEventHandler customGapHandler = nullptr;

#if BLE_ADVERTISING_SUPPORTED
  LifecycleHook advBeginHook = nullptr;
  LifecycleHook advEndHook = nullptr;
#endif
#if BLE_SCANNING_SUPPORTED
  LifecycleHook scanBeginHook = nullptr;
  LifecycleHook scanEndHook = nullptr;
#endif
#if BLE_ISO_SUPPORTED
  LifecycleHook isoPreEndHook = nullptr;
#endif
#if BLE_GATT_SERVER_SUPPORTED
  LifecycleHook serverDeactivateHook = nullptr;
  LifecycleHook serverPostEndHook = nullptr;
#endif
  LifecycleHook streamPreEndHook = nullptr;
  LifecycleHook streamPostEndHook = nullptr;
#if BLE_SMP_SUPPORTED
  LifecycleHook securityBeginHook = nullptr;
#endif

  template<typename Hook> Hook loadHook(Hook &slot) {
    portENTER_CRITICAL(&hookMux);
    Hook hook = slot;
    portEXIT_CRITICAL(&hookMux);
    return hook;
  }

  template<typename Hook> void storeHook(Hook &slot, Hook hook) {
    portENTER_CRITICAL(&hookMux);
    slot = hook;
    portEXIT_CRITICAL(&hookMux);
  }

  void abortEnd() {
    BLELockGuard lifecycle(lifecycleMtx);
    endReleaseMemory = false;
    endTask = nullptr;
    endRunning = false;
    ending = false;
  }

  ~BLEClassImplCommon() {
    if (lifecycleMtx) {
      vSemaphoreDelete(lifecycleMtx);
    }
  }
};

#endif /* BLE_ENABLED */
