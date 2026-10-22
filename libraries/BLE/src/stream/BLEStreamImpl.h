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

struct BLEStream::Impl {
  struct Role {
    std::atomic<bool> ending{false};

    virtual ~Role() = default;
    virtual void end() = 0;
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
  cbuf rxBuf;
  // Server mode can have multiple peers; client mode uses 0/1. BLE callbacks
  // update this from the stack task while sketches read it from their task.
  std::atomic<int> peerCount{0};

  ConnectHandler onConnectCb = nullptr;
  DisconnectHandler onDisconnectCb = nullptr;
  PeerDataHandler onPeerDataCb = nullptr;

  explicit Impl(size_t rxBufSize) : roleMtx(xSemaphoreCreateRecursiveMutexStatic(&roleMtxStorage)), rxBuf(rxBufSize) {}

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

  std::shared_ptr<Role> takeRole() {
    BLELockGuard lock(roleMtx);
    return std::move(role);
  }
};

#endif /* BLE_ENABLED */
