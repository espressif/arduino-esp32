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

/**
 * @file BLEStreamClient.cpp
 * @brief Central (client) NUS role for `BLEStream` (`beginClient()`).
 */

#include "core/BLEGuards.h"
#if BLE_ENABLED

#include "stream/BLEStreamImpl.h"
#include "BLE.h"
#include "client/BLEClient.h"
#include "remote/BLERemoteService.h"
#include "remote/BLERemoteCharacteristic.h"
#include <algorithm>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "core/BLEValidationHooks.h"

// ==========================================================================
// 1. Client role state & GATT callback handlers
// ==========================================================================

struct BLEStreamClientRole final : BLEStream::Impl::Role {
  BLEStream::Impl &common;
  BLEClient client;
  BLERemoteCharacteristic remoteTx;
  BLERemoteCharacteristic remoteRx;

  explicit BLEStreamClientRole(BLEStream::Impl &impl) : common(impl) {}

#if BLE_VALIDATION_HOOKS
  ~BLEStreamClientRole() override {
    BLEHooks.stream.finalRoleReleaseReached.store(true, std::memory_order_release);
    BLEValidationHooks::holdWhile(BLEHooks.stream.holdFinalRoleRelease);
  }
#endif

  static void onClientConnect(const std::weak_ptr<BLEStream::Impl> &common, const BLEConnInfo &connInfo) {
    auto impl = common.lock();
    if (!impl) {
      return;
    }
    if (auto callback = impl->loadConnectHandler()) {
      callback(connInfo);
    }
  }

  static void onClientDisconnect(const std::weak_ptr<BLEStream::Impl> &common, const BLEConnInfo &connInfo, uint8_t reason) {
    auto impl = common.lock();
    if (!impl) {
      return;
    }
    if (auto callback = impl->loadDisconnectHandler()) {
      callback(connInfo, reason);
    }
  }

  static void onRemoteTxNotify(const std::weak_ptr<BLEStream::Impl> &common, const BLEClient &streamClient, const uint8_t *data, size_t len) {
    auto impl = common.lock();
    if (!impl || !data || len == 0) {
      return;
    }
    if (auto callback = impl->loadPeerDataHandler()) {
      callback(streamClient.getConnInfo(), data, len);
    }
    impl->rxBuf.write(reinterpret_cast<const char *>(data), len);
  }

  static BTStatus abortStart(BLEStream &stream, const std::shared_ptr<BLEStreamClientRole> &role, BTStatus status) {
    BLEStream::Impl::finishRoleStart(role);
    stream.end();
    return status;
  }

  BTStatus end() override {
    ending = true;
    BTStatus result = BTStatus::OK;
    bool terminal = true;
    if (client) {
      if (client.isConnected()) {
        result = client.disconnect();
#if BLE_VALIDATION_HOOKS
        if (BLEValidationHooks::consumeFailure(BLEHooks.stream.disconnectRaces)) {
          result = BTStatus::NotConnected;
        }
#endif
        // A peer may close the ACL after the state snapshot but before the
        // disconnect request. The requested terminal state is already reached.
        if (result == BTStatus::NotConnected) {
          result = BTStatus::OK;
        }
        terminal = (result == BTStatus::OK);
      }
      if (terminal) {
        client.resetCallbacks();
      }
    }
    // Leave remoteTx/remoteRx installed until Role destruction. writeChunks()
    // may already be iterating on another task that passed the ending check.
    return result;
  }

  void detachCallbacks() override {
    if (client) {
      client.resetCallbacks();
    }
  }

  size_t connectedCount() const override {
    return (client && client.isConnected()) ? 1 : 0;
  }

  std::vector<BLEConnInfo> peers() const override {
    if (client && client.isConnected()) {
      return {client.getConnInfo()};
    }
    return {};
  }

  size_t write(const uint8_t *buffer, size_t size) override {
    return writeChunks(buffer, size);
  }

  size_t writeTo(const BLEConnInfo &peer, const uint8_t *buffer, size_t size) override {
    if (!client || client.getConnInfo().getHandle() != peer.getHandle()) {
      return 0;
    }
    return writeChunks(buffer, size);
  }

private:
  size_t writeChunks(const uint8_t *buffer, size_t size) {
    const uint16_t mtu = client.getMTU();
    const uint16_t chunkSize = (mtu > 3) ? (mtu - 3) : 20;
    size_t sent = 0;
    while (sent < size) {
      size_t len = std::min<size_t>(size - sent, chunkSize);
      BTStatus status = remoteRx.writeValue(buffer + sent, len, false);
      if (!status) {
        break;
      }
      sent += len;
#if BLE_VALIDATION_HOOKS
      if (BLEHooks.stream.holdWriteChunks.load(std::memory_order_acquire)) {
        BLEHooks.stream.writeChunksEntered.store(true, std::memory_order_release);
        BLEValidationHooks::holdWhile(BLEHooks.stream.holdWriteChunks);
        if (!remoteRx) {
          BLEHooks.stream.writeSawClearedHandle.store(true, std::memory_order_relaxed);
        }
      }
#endif
    }
    return sent;
  }
};

// ==========================================================================
// 2. Public BLEStream::beginClient() entry point
// ==========================================================================

BTStatus BLEStream::beginClient(const BTAddress &address, uint32_t timeoutMs) {
  if (!BLE.setStreamEndHooks(Impl::prepareHostShutdown, Impl::releaseQuarantined)) {
    return BTStatus::Busy;
  }
  if (!Impl::recoverQuarantined()) {
    return BTStatus::Busy;
  }
  if (!_impl || _impl->loadRole()) {
    return BTStatus::InvalidState;
  }
  if (!BLE.isInitialized()) {
    BTStatus status = BLE.begin("");
    if (!status) {
      return status;
    }
  }

  auto role = std::make_shared<BLEStreamClientRole>(*_impl);
  role->client = BLE.createClient();
  if (!role->client) {
    return BTStatus::Fail;
  }

  if (!Impl::activateRole(_impl, role)) {
    return BTStatus::Busy;
  }
  auto &active = *role;
  std::weak_ptr<Impl> common = _impl;
  active.client.onConnect([common](BLEClient, const BLEConnInfo &connInfo) {
    BLEStreamClientRole::onClientConnect(common, connInfo);
  });
  active.client.onDisconnect([common](BLEClient, const BLEConnInfo &connInfo, uint8_t reason) {
    BLEStreamClientRole::onClientDisconnect(common, connInfo, reason);
  });

  BTStatus status = active.client.connect(address, timeoutMs);
  if (!status) {
    return BLEStreamClientRole::abortStart(*this, role, status);
  }

  BLERemoteService service = active.client.getService(nusServiceUUID());
  if (!service) {
    return BLEStreamClientRole::abortStart(*this, role, BTStatus::NotFound);
  }
  active.remoteTx = service.getCharacteristic(nusTxCharUUID());
  active.remoteRx = service.getCharacteristic(nusRxCharUUID());
  if (!active.remoteTx || !active.remoteRx) {
    return BLEStreamClientRole::abortStart(*this, role, BTStatus::NotFound);
  }

  BLEClient streamClient = active.client;
  status =
    active.remoteTx.subscribe(BLESubscription::Notify, [common, streamClient](BLERemoteCharacteristic, const uint8_t *data, size_t len, BLESubscription) {
      BLEStreamClientRole::onRemoteTxNotify(common, streamClient, data, len);
    });
  if (!status) {
    return BLEStreamClientRole::abortStart(*this, role, status);
  }
  if (!Impl::finishRoleStart(role)) {
    end();
    return BTStatus::Busy;
  }
  return BTStatus::OK;
}

#endif /* BLE_ENABLED */
