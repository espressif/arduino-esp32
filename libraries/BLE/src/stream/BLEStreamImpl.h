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

#include "core/BLEGuards.h"
#if BLE_ENABLED

#include "stream/BLEStream.h"
#include "core/BLEMutex.h"
#include "cbuf.h"
#include <atomic>
#include <memory>
#include <vector>

struct BLEStream::Impl {
  struct Role {
    std::atomic<bool> starting{false};
    std::atomic<bool> ending{false};
    // Claims the one teardown executor. If callback-context task creation
    // fails, it is released while `ending` remains set so a later start can
    // recover the quarantined role without reopening begin/write admission.
    std::atomic<bool> endExecutorActive{false};

    virtual ~Role() = default;
    virtual BTStatus end() = 0;
    virtual void detachCallbacks() = 0;
    virtual size_t connectedCount() const = 0;
    virtual std::vector<BLEConnInfo> peers() const = 0;
    virtual size_t write(const uint8_t *buffer, size_t size) = 0;
    virtual size_t writeTo(const BLEConnInfo &peer, const uint8_t *buffer, size_t size) = 0;
  };

  StaticSemaphore_t roleMtxStorage{};
  SemaphoreHandle_t roleMtx;
  // Accessed from both sketch and BLE callback tasks. Short mutex-protected
  // snapshots keep the role alive without holding the admission lock while
  // backend operations or callbacks run.
  std::shared_ptr<Role> role;
  // Callback-context teardown is handed to a worker. These references form a
  // temporary self-retaining handoff so destruction cannot abandon native
  // resources if worker task allocation must be retried.
  std::shared_ptr<Impl> deferredSelf;
  std::shared_ptr<Role> deferredRole;
  cbuf rxBuf;

  ConnectHandler onConnectCb = nullptr;
  DisconnectHandler onDisconnectCb = nullptr;
  PeerDataHandler onPeerDataCb = nullptr;

  explicit Impl(size_t rxBufSize) : roleMtx(xSemaphoreCreateRecursiveMutexStatic(&roleMtxStorage)), rxBuf(rxBufSize) {}

  static bool startEndWorker(Impl *common);
  static bool claimEnd(const std::shared_ptr<Impl> &common, std::shared_ptr<Role> &role, bool &hostShuttingDown);
  static bool activateRole(const std::shared_ptr<Impl> &common, const std::shared_ptr<Role> &role);
  static bool finishRoleStart(const std::shared_ptr<Role> &role);
  static bool cancelRoleStart(const std::shared_ptr<Impl> &common, const std::shared_ptr<Role> &role);
  static void retainEnd(const std::shared_ptr<Impl> &common);
  static void completeEnd(const std::shared_ptr<Impl> &common, std::shared_ptr<Role> &role);
  static void quarantineEnd(const std::shared_ptr<Impl> &common, const std::shared_ptr<Role> &role);
  static bool recoverQuarantined();
  static void prepareHostShutdown();
  static void releaseQuarantined();
  static void endWorker(void *arg);

  struct RecoveryState;
  static RecoveryState &recoveryState();

  ~Impl() {
    if (roleMtx) {
      vSemaphoreDelete(roleMtx);
    }
  }

  std::shared_ptr<Role> loadRole() const {
    BLELockGuard lock(roleMtx);
    return role;
  }

  void storeRole(std::shared_ptr<Role> value) {
    BLELockGuard lock(roleMtx);
    role = std::move(value);
  }

  bool markRoleEnding(std::shared_ptr<Role> &out) {
    BLELockGuard lock(roleMtx);
    if (!role) {
      return false;
    }
    role->ending = true;
    if (role->starting) {
      return false;
    }
    bool expected = false;
    if (!role->endExecutorActive.compare_exchange_strong(expected, true)) {
      return false;
    }
    out = role;
    return true;
  }

  void clearRole(const std::shared_ptr<Role> &expected) {
    BLELockGuard lock(roleMtx);
    if (role == expected) {
      role.reset();
    }
  }

  ConnectHandler loadConnectHandler() {
    BLELockGuard lock(roleMtx);
    return onConnectCb;
  }

  DisconnectHandler loadDisconnectHandler() {
    BLELockGuard lock(roleMtx);
    return onDisconnectCb;
  }

  PeerDataHandler loadPeerDataHandler() {
    BLELockGuard lock(roleMtx);
    return onPeerDataCb;
  }

  void storeConnectHandler(ConnectHandler handler) {
    BLELockGuard lock(roleMtx);
    onConnectCb = std::move(handler);
  }

  void storeDisconnectHandler(DisconnectHandler handler) {
    BLELockGuard lock(roleMtx);
    onDisconnectCb = std::move(handler);
  }

  void storePeerDataHandler(PeerDataHandler handler) {
    BLELockGuard lock(roleMtx);
    onPeerDataCb = std::move(handler);
  }

  void clearHandlers() {
    BLELockGuard lock(roleMtx);
    onConnectCb = nullptr;
    onDisconnectCb = nullptr;
    onPeerDataCb = nullptr;
  }
};

#endif /* BLE_ENABLED */
