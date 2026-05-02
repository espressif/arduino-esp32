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

#include "impl/BLEGuards.h"
#if BLE_ENABLED

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "BTStatus.h"

/**
 * @brief Internal synchronization primitive for BLE operations.
 *
 * This class provides **task-to-task (or callback-to-task) signaling**, allowing
 * asynchronous BLE operations to be exposed as blocking calls.
 *
 * This is NOT a mutex and MUST NOT be used for mutual exclusion.
 * It does not protect shared data and provides no ownership semantics.
 *
 * Conceptually similar to:
 *   - std::condition_variable + state
 *   - a very small "future/promise"-like mechanism
 *
 * Each instance owns a private binary semaphore so that multiple BLESync
 * objects waiting on the same task cannot steal each other's notifications.
 *
 * @note Usage is strictly 1:1:
 *       - One task calls take() and then wait()
 *       - One producer (e.g., BLE callback) calls give()
 *
 * Threading contract:
 *   - take() and wait() must be called from the SAME task (the Arduino task).
 *   - give() may be called from ANY task or ISR context (e.g., NimBLE host
 *     task, Bluedroid BTC task).
 *   - If an API call fails AFTER take() but BEFORE wait(), the caller MUST
 *     call give(BTStatus::Fail) before returning. Omitting this will leave
 *     the sync in a taken state; while the next take() drains the stale
 *     semaphore, this is fragile and should not be relied upon.
 *
 * @note This is an internal class. Not part of the public API.
 */
class BLESync {
public:
  /**
   * @brief Construct a sync object with no semaphore until @ref take().
   */
  BLESync() : _sem(nullptr), _status(BTStatus::OK) {}

  /**
   * @brief Release the internal binary semaphore, if created.
   */
  ~BLESync() {
    if (_sem) {
      vSemaphoreDelete(_sem);
    }
  }

  BLESync(const BLESync &) = delete;
  BLESync &operator=(const BLESync &) = delete;

  /**
   * @brief Prepare for a blocking wait on the calling task.
   *
   * MUST be called before every wait().
   *
   * Effects:
   *   - Lazily creates the semaphore (first use)
   *   - Resets internal status to BTStatus::OK
   *   - Drains any stale signal from previous operations
   *
   * Must be called from the SAME task that will call wait().
   */
  void take();

  /**
   * @brief Block until give() is called or timeout elapses.
   *
   * This function blocks the calling task. Do not call from:
   *     - ISR context
   *     - BLE stack callbacks
   *
   * @param timeoutMs Maximum time to block in milliseconds, or @c portMAX_DELAY for no limit.
   *
   * @return
   *   - Status passed via give()
   *   - BTStatus::Timeout if timed out
   *   - BTStatus::InvalidState if take() was not called
   */
  BTStatus wait(uint32_t timeoutMs = portMAX_DELAY);

  /**
   * @brief Signal completion and wake the waiting task.
   *
   * Safe to call from:
   *   - BLE host callbacks
   *   - Different task context
   *
   * Must not perform heavy work or user callbacks here.
   * This should remain a lightweight signal operation.
   *
   * @param status  Outcome to return from the next @ref wait (or for @ref status).
   */
  void give(BTStatus status = BTStatus::OK);

  /**
   * @brief Read the last value passed to @ref give without waiting.
   * @return  Last status stored by @ref give, or the default if none yet.
   */
  BTStatus status() const {
    return _status;
  }

private:
  SemaphoreHandle_t _sem;
  BTStatus _status;
};

#endif /* BLE_ENABLED */
