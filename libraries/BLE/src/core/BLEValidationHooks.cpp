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

#include "core/BLEValidationHooks.h"
#if BLE_VALIDATION_HOOKS

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

BLEValidationHooks BLEHooks;

bool BLEValidationHooks::consumeFailure(std::atomic<int> &remaining) {
  int value = remaining.load(std::memory_order_relaxed);
  while (value > 0) {
    if (remaining.compare_exchange_weak(value, value - 1, std::memory_order_relaxed)) {
      return true;
    }
  }
  return false;
}

void BLEValidationHooks::holdWhile(const std::atomic<bool> &flag) {
  while (flag.load(std::memory_order_acquire)) {
    vTaskDelay(1);
  }
}

#endif /* BLE_VALIDATION_HOOKS */
