/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 * Copyright 2020-2026 Ryan Powell <ryan@nable-embedded.io> and
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

#include "core/BLEGuards.h"
#if BLE_ENABLED

#include <Arduino.h>
#include "stream/BLEStream.h"
#include "stream/BLEStreamImpl.h"
#include "core/BLETask.h"
#include "core/BLEValidationHooks.h"
#include "esp32-hal-log.h"
#include <new>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Nordic UART Service UUIDs (6e40000x-b5a3-f393-e0a9-e50e24dcca9e)
static constexpr BLEUUID kNUS_ServiceUUID = BLEUUID::from128(0x6e, 0x40, 0x00, 0x01, 0xb5, 0xa3, 0xf3, 0x93, 0xe0, 0xa9, 0xe5, 0x0e, 0x24, 0xdc, 0xca, 0x9e);
static constexpr BLEUUID kNUS_RxCharUUID = BLEUUID::from128(0x6e, 0x40, 0x00, 0x02, 0xb5, 0xa3, 0xf3, 0x93, 0xe0, 0xa9, 0xe5, 0x0e, 0x24, 0xdc, 0xca, 0x9e);
static constexpr BLEUUID kNUS_TxCharUUID = BLEUUID::from128(0x6e, 0x40, 0x00, 0x03, 0xb5, 0xa3, 0xf3, 0x93, 0xe0, 0xa9, 0xe5, 0x0e, 0x24, 0xdc, 0xca, 0x9e);

BLEUUID BLEStream::nusServiceUUID() {
  return kNUS_ServiceUUID;
}

BLEUUID BLEStream::nusRxCharUUID() {
  return kNUS_RxCharUUID;
}

BLEUUID BLEStream::nusTxCharUUID() {
  return kNUS_TxCharUUID;
}

// --------------------------------------------------------------------------
// BLEStream lifecycle
// --------------------------------------------------------------------------

static constexpr uint8_t kEndRetryLimit = 4;

struct BLEStream::Impl::RecoveryState {
  StaticSemaphore_t mtxStorage{};
  SemaphoreHandle_t mtx = xSemaphoreCreateRecursiveMutexStatic(&mtxStorage);
  std::atomic<bool> hostShuttingDown{false};
  std::vector<std::shared_ptr<Impl>> entries;
};

BLEStream::Impl::RecoveryState &BLEStream::Impl::recoveryState() {
  // BLEClass's global destructor may invoke the registered post-end hook after
  // ordinary function statics have been destroyed. This tiny registry is
  // intentionally process-lifetime state; BLE.end() empties its entries.
  alignas(RecoveryState) static uint8_t storage[sizeof(RecoveryState)];
  static RecoveryState *state = new (storage) RecoveryState();
  return *state;
}

BLEStream::BLEStream(size_t rxBufferSize) : _impl(std::make_shared<Impl>(rxBufferSize)) {}

BLEStream::~BLEStream() {
  // end() already clears the callbacks this stream registered while it was
  // live (server: rxChr + server handlers, client: client handlers). Do not
  // clear them unconditionally here: after end() the server/client handles
  // may be shared with (or have been re-registered by) another owner, and
  // wiping their callbacks would break that owner.
  end();
  _impl.reset();
}

bool BLEStream::Impl::startEndWorker(Impl *common) {
  auto &state = recoveryState();
  BLELockGuard recoveryLock(state.mtx);
  if (state.hostShuttingDown.load(std::memory_order_relaxed)) {
    return false;
  }
#if BLE_VALIDATION_HOOKS
  if (BLEValidationHooks::consumeFailure(BLEHooks.stream.workerStartFailures)) {
    return false;
  }
#endif
  return xTaskCreate(endWorker, "ble-stream-end", 6144, common, 1, nullptr) == pdPASS;
}

bool BLEStream::Impl::claimEnd(
  const std::shared_ptr<Impl> &common, std::shared_ptr<Role> &role, bool &hostShuttingDown
) {
  if (!common) {
    return false;
  }
  auto &state = recoveryState();
  BLELockGuard recoveryLock(state.mtx);
  hostShuttingDown = state.hostShuttingDown.load(std::memory_order_relaxed);
  if (!common->markRoleEnding(role)) {
    return false;
  }
  for (const auto &entry : state.entries) {
    if (entry == common) {
      return true;
    }
  }
  state.entries.push_back(common);
  return true;
}

bool BLEStream::Impl::activateRole(const std::shared_ptr<Impl> &common, const std::shared_ptr<Role> &role) {
  if (!common || !role) {
    return false;
  }
  auto &state = recoveryState();
  BLELockGuard recoveryLock(state.mtx);
  if (state.hostShuttingDown.load(std::memory_order_relaxed)) {
    return false;
  }
  {
    BLELockGuard roleLock(common->roleMtx);
    if (common->role) {
      return false;
    }
    role->starting = true;
    common->role = role;
  }
  for (const auto &entry : state.entries) {
    if (entry == common) {
      return true;
    }
  }
  state.entries.push_back(common);
  return true;
}

bool BLEStream::Impl::finishRoleStart(const std::shared_ptr<Role> &role) {
  if (!role) {
    return false;
  }
  auto &state = recoveryState();
  BLELockGuard recoveryLock(state.mtx);
  role->starting = false;
  return !state.hostShuttingDown.load(std::memory_order_relaxed) && !role->ending.load(std::memory_order_acquire);
}

bool BLEStream::Impl::cancelRoleStart(
  const std::shared_ptr<Impl> &common, const std::shared_ptr<Role> &role
) {
  if (!common || !role) {
    return false;
  }
  auto &state = recoveryState();
  BLELockGuard recoveryLock(state.mtx);
  BLELockGuard roleLock(common->roleMtx);
  role->starting = false;
  if (state.hostShuttingDown.load(std::memory_order_relaxed) || role->ending.load(std::memory_order_acquire)
      || common->role != role) {
    return false;
  }
  common->role.reset();
  for (auto it = state.entries.begin(); it != state.entries.end(); ++it) {
    if (*it == common) {
      state.entries.erase(it);
      break;
    }
  }
  return true;
}

void BLEStream::Impl::retainEnd(const std::shared_ptr<Impl> &common) {
  if (!common) {
    return;
  }
  auto &state = recoveryState();
  BLELockGuard lock(state.mtx);
  for (const auto &entry : state.entries) {
    if (entry == common) {
      return;
    }
  }
  state.entries.push_back(common);
}

void BLEStream::Impl::completeEnd(
  const std::shared_ptr<Impl> &common, std::shared_ptr<Role> &role
) {
  if (!common || !role) {
    return;
  }
  auto &state = recoveryState();
  BLELockGuard recoveryLock(state.mtx);
  BLELockGuard roleLock(common->roleMtx);
  if (common->role != role) {
    return;
  }
  common->role.reset();
  if (common->deferredRole == role) {
    common->deferredRole.reset();
    common->deferredSelf.reset();
  }
  // Keep the registry gate closed while releasing the executor's final native
  // owner. In particular, a BlueDroid client Role destructor unregisters its
  // GATTC application; BLE.end() must not observe an empty registry and
  // deinitialize the host until that destructor has finished.
  role.reset();
  for (auto it = state.entries.begin(); it != state.entries.end(); ++it) {
    if (*it == common) {
      state.entries.erase(it);
      return;
    }
  }
}

void BLEStream::Impl::quarantineEnd(const std::shared_ptr<Impl> &common, const std::shared_ptr<Role> &role) {
  if (!common || !role) {
    return;
  }
  // The ending Impl is published before any executor starts, but retain again
  // defensively so every terminal handoff preserves cleanup ownership.
  retainEnd(common);
  role->detachCallbacks();
  role->endExecutorActive = false;
  BLELockGuard lock(common->roleMtx);
  if (common->deferredRole == role) {
    common->deferredRole.reset();
    common->deferredSelf.reset();
  }
}

bool BLEStream::Impl::recoverQuarantined() {
  auto &state = recoveryState();
  for (;;) {
    std::shared_ptr<Impl> common;
    std::shared_ptr<Role> role;
    bool endingPresent = false;
    {
      // Gate check and executor claim are one registry transaction. BLE.end()
      // either closes the gate first, or sees this active executor and waits
      // while the native host is still available.
      BLELockGuard recoveryLock(state.mtx);
      if (state.hostShuttingDown.load(std::memory_order_relaxed)) {
        return false;
      }
      for (auto it = state.entries.begin(); it != state.entries.end();) {
        common = *it;
        role = common ? common->loadRole() : nullptr;
        if (!common || !role) {
          it = state.entries.erase(it);
          common.reset();
          role.reset();
          continue;
        }
        if (!role->ending.load(std::memory_order_acquire)) {
          common.reset();
          role.reset();
          ++it;
          continue;
        }
        endingPresent = true;
        if (role->starting.load(std::memory_order_acquire)) {
          common.reset();
          role.reset();
          ++it;
          continue;
        }
        bool expected = false;
        if (role->endExecutorActive.compare_exchange_strong(expected, true)) {
          break;
        }
        common.reset();
        role.reset();
        ++it;
      }
      if (!common || !role) {
        return !endingPresent;
      }
    }

    BTStatus status;
#if BLE_VALIDATION_HOOKS
    status = BLEValidationHooks::consumeFailure(BLEHooks.stream.teardownFailures) ? BTStatus::Busy : role->end();
#else
    status = role->end();
#endif
    if (status) {
      completeEnd(common, role);
    } else {
      role->endExecutorActive = false;
      return false;
    }
  }
}

void BLEStream::Impl::prepareHostShutdown() {
  auto &state = recoveryState();
#if BLE_VALIDATION_HOOKS
  if (BLEHooks.stream.holdFinalRoleRelease.load(std::memory_order_acquire)) {
    BLEHooks.stream.hostShutdownEntered.store(true, std::memory_order_release);
  }
#endif
  std::vector<std::shared_ptr<Impl>> entries;
  {
    BLELockGuard recoveryLock(state.mtx);
    state.hostShuttingDown.store(true, std::memory_order_relaxed);
    entries = state.entries;
    for (const auto &common : entries) {
      auto role = common ? common->loadRole() : nullptr;
      if (role) {
        role->ending.store(true, std::memory_order_release);
      }
    }
  }
  // Native teardown workers may be waiting on stack completion, so let them
  // finish while the host is still running. Once shutdown is set they cannot
  // enqueue another worker and instead settle into quarantine.
  for (;;) {
    bool active = false;
    {
      BLELockGuard recoveryLock(state.mtx);
      entries = state.entries;
    }
    for (const auto &common : entries) {
      auto role = common ? common->loadRole() : nullptr;
      if (role
          && (role->starting.load(std::memory_order_acquire) || role->endExecutorActive.load(std::memory_order_acquire))) {
        active = true;
        break;
      }
    }
    if (!active) {
      break;
    }
    vTaskDelay(1);
  }
}

void BLEStream::Impl::releaseQuarantined() {
  auto &state = recoveryState();
  BLELockGuard recoveryLock(state.mtx);
  for (auto &common : state.entries) {
    auto role = common ? common->loadRole() : nullptr;
    if (!common || !role) {
      continue;
    }
    role->detachCallbacks();
    role->endExecutorActive = false;
    common->clearRole(role);
    BLELockGuard commonLock(common->roleMtx);
    common->deferredRole.reset();
    common->deferredSelf.reset();
  }
  state.entries.clear();
  state.hostShuttingDown.store(false, std::memory_order_release);
}

void BLEStream::Impl::endWorker(void *arg) {
  auto *raw = static_cast<Impl *>(arg);
  {
    // FreeRTOS task deletion does not unwind C++ locals. Keep every owning
    // shared_ptr inside this inner scope so its destructor runs before
    // vTaskDelete() terminates the task.
    std::shared_ptr<Impl> common;
    std::shared_ptr<Role> role;
    {
      BLELockGuard lock(raw ? raw->roleMtx : nullptr);
      if (raw) {
        common = raw->deferredSelf;
        role = raw->deferredRole;
      }
    }
    if (common && role) {
      BTStatus status = BTStatus::Fail;
      for (uint8_t retry = 0; retry < kEndRetryLimit; ++retry) {
        if (recoveryState().hostShuttingDown.load(std::memory_order_relaxed)) {
          quarantineEnd(common, role);
          break;
        }
#if BLE_VALIDATION_HOOKS
        status = BLEValidationHooks::consumeFailure(BLEHooks.stream.teardownFailures) ? BTStatus::Busy : role->end();
#else
        status = role->end();
#endif
        if (status) {
          completeEnd(common, role);
          break;
        }
        if (retry + 1 == kEndRetryLimit) {
          log_e("BLEStream: deferred teardown quarantined (%s)", status.toString());
          quarantineEnd(common, role);
          break;
        }
        vTaskDelay(pdMS_TO_TICKS(10u << retry));
      }
    }
  }
  vTaskDelete(nullptr);
}

void BLEStream::end() {
  if (!_impl) {
    return;
  }
  auto common = _impl;
  std::shared_ptr<Impl::Role> role;
  bool hostShuttingDown = false;
  if (!Impl::claimEnd(common, role, hostShuttingDown)) {
    return;
  }
  if (hostShuttingDown) {
    Impl::quarantineEnd(common, role);
    return;
  }

  // Service removal is blocking on Bluedroid and rebuilds the native table on
  // NimBLE. Neither operation may run on the stack callback task that must
  // deliver its completion. Retain both Impl and role until a worker finishes;
  // begin() remains closed because the ending role stays installed. If either
  // allocation fails, keep it closed and make end() retryable rather than
  // silently exposing a half-torn-down stream.
  if (bleInStackCallbackTask()) {
    {
      BLELockGuard lock(common->roleMtx);
      common->deferredSelf = common;
      common->deferredRole = role;
    }
    if (!Impl::startEndWorker(common.get())) {
      log_e("BLEStream: unable to defer callback-context teardown");
      Impl::quarantineEnd(common, role);
    }
    return;
  }

  BTStatus status = role->end();
  if (status) {
    Impl::completeEnd(common, role);
  } else {
    role->endExecutorActive = false;
    log_e("BLEStream: teardown failed (%s); call end() again", status.toString());
  }
}

bool BLEStream::connected() const {
  auto role = _impl ? _impl->loadRole() : nullptr;
  return role && !role->ending && role->connectedCount() > 0;
}

size_t BLEStream::peerCount() const {
  auto role = _impl ? _impl->loadRole() : nullptr;
  return (role && !role->ending) ? role->connectedCount() : 0;
}

std::vector<BLEConnInfo> BLEStream::peers() const {
  if (!_impl) {
    return {};
  }
  auto role = _impl->loadRole();
  return (role && !role->ending) ? role->peers() : std::vector<BLEConnInfo>{};
}

// --------------------------------------------------------------------------
// Arduino Stream interface
// --------------------------------------------------------------------------

int BLEStream::available() {
  return _impl ? static_cast<int>(_impl->rxBuf.available()) : 0;
}

int BLEStream::read() {
  return _impl ? _impl->rxBuf.read() : -1;
}

int BLEStream::peek() {
  return _impl ? _impl->rxBuf.peek() : -1;
}

size_t BLEStream::write(uint8_t c) {
  return write(&c, 1);
}

size_t BLEStream::write(const uint8_t *buffer, size_t size) {
  if (!_impl || !buffer || size == 0) {
    return 0;
  }
  auto role = _impl->loadRole();
  return (role && !role->ending && role->connectedCount() > 0) ? role->write(buffer, size) : 0;
}

size_t BLEStream::writeTo(const BLEConnInfo &peer, const uint8_t *buffer, size_t size) {
  if (!_impl || !buffer || size == 0) {
    return 0;
  }
  auto role = _impl->loadRole();
  return (role && !role->ending && role->connectedCount() > 0) ? role->writeTo(peer, buffer, size) : 0;
}

void BLEStream::flush() {
  // BLE notifications are sent immediately; no buffering to flush
}

BLEStream::operator bool() const {
  if (!_impl) {
    return false;
  }
  auto role = _impl->loadRole();
  return role && !role->ending;
}

// --------------------------------------------------------------------------
// Callbacks
// --------------------------------------------------------------------------

void BLEStream::onConnect(ConnectHandler handler) {
  if (_impl) {
    _impl->storeConnectHandler(std::move(handler));
  }
}

void BLEStream::onDisconnect(DisconnectHandler handler) {
  if (_impl) {
    _impl->storeDisconnectHandler(std::move(handler));
  }
}

void BLEStream::onPeerData(PeerDataHandler handler) {
  if (_impl) {
    _impl->storePeerDataHandler(std::move(handler));
  }
}

void BLEStream::resetCallbacks() {
  if (!_impl) {
    return;
  }
  _impl->clearHandlers();
}

#endif /* BLE_ENABLED */
