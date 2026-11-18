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

#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * @brief Lock-free gate that keeps retained-handle native calls out of host
 *        teardown.
 *
 * Callers increment the in-flight count first, then sample the open flag.
 * Shutdown stores the flag closed and waits until in-flight reaches zero
 * before disabling the host. Reopen on the next @c BLE.begin().
 *
 * This is not a retain/dispatch registry: Bluedroid client snapshots,
 * terminal-close owners, and NimBLE scan EventContext retirement stay
 * per-stack.
 */
class BLEAdmissionGate {
public:
  explicit BLEAdmissionGate(bool initiallyOpen = true) : _open(initiallyOpen) {}

  bool enter() {
    _inFlight.fetch_add(1, std::memory_order_seq_cst);
    if (!_open.load(std::memory_order_seq_cst)) {
      _inFlight.fetch_sub(1, std::memory_order_seq_cst);
      return false;
    }
    return true;
  }

  void leave() {
    _inFlight.fetch_sub(1, std::memory_order_seq_cst);
  }

  void close() {
    _open.store(false, std::memory_order_seq_cst);
  }

  void waitUntilIdle() {
    while (_inFlight.load(std::memory_order_seq_cst) != 0) {
      vTaskDelay(1);
    }
  }

  void closeAndWait() {
    close();
    waitUntilIdle();
  }

  void reopen() {
    _open.store(true, std::memory_order_seq_cst);
  }

  bool isOpen() const {
    return _open.load(std::memory_order_seq_cst);
  }

private:
  std::atomic<bool> _open;
  std::atomic<uint32_t> _inFlight{0};
};

/**
 * @brief RAII enter/leave for @ref BLEAdmissionGate.
 */
class BLEAdmissionGuard {
public:
  explicit BLEAdmissionGuard(BLEAdmissionGate &gate) : _gate(&gate), _admitted(gate.enter()) {}

  ~BLEAdmissionGuard() {
    if (_admitted) {
      _gate->leave();
    }
  }

  explicit operator bool() const {
    return _admitted;
  }

  BLEAdmissionGuard(const BLEAdmissionGuard &) = delete;
  BLEAdmissionGuard &operator=(const BLEAdmissionGuard &) = delete;

private:
  BLEAdmissionGate *_gate;
  bool _admitted;
};
