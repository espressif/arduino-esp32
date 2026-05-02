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

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/**
 * @brief RAII lock guard for a FreeRTOS recursive mutex.
 *
 * Provides **mutual exclusion** for shared data using scope-based locking.
 *
 * This is NOT a signaling primitive:
 *   - It cannot be used to wait for events
 *   - It does not coordinate between tasks beyond exclusive access
 *
 * Equivalent to std::lock_guard<std::recursive_mutex>, but avoids the
 * flash overhead of <mutex>.
 *
 * @note Uses a recursive mutex:
 *       The same task can safely acquire the lock multiple times.
 *       This is required for BLE code paths that may re-enter.
 */
class BLELockGuard {
public:

  /**
   * @brief Acquire the mutex, blocking indefinitely until available.
   *
   * This will block the calling task.
   * Avoid holding the lock across:
   *   - Long operations
   *   - BLE stack calls that may indirectly re-enter
   *
   * @param m  Recursive mutex handle, or @c NULL to no-op.
   * @note  The lock is always released in the destructor even if @p m was NULL.
   */
  explicit BLELockGuard(SemaphoreHandle_t m) : _mtx(m) {
    if (_mtx) {
      xSemaphoreTakeRecursive(_mtx, portMAX_DELAY);
    }
  }

  /**
   * @brief Release the mutex once.
   *
   * Automatically called at scope exit (including exceptions / early returns).
   */
  ~BLELockGuard() {
    if (_mtx) {
      xSemaphoreGiveRecursive(_mtx);
    }
  }

  BLELockGuard(const BLELockGuard &) = delete;
  BLELockGuard &operator=(const BLELockGuard &) = delete;

private:
  SemaphoreHandle_t _mtx;
};
