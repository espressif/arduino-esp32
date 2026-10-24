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
  auto abortStart = [&](BTStatus status) {
    Impl::finishRoleStart(role);
    end();
    return status;
  };
  auto &active = *role;
  std::weak_ptr<Impl> common = _impl;
  active.client.onConnect([common](BLEClient, const BLEConnInfo &connInfo) {
    auto impl = common.lock();
    if (!impl) {
      return;
    }
    auto callback = impl->loadConnectHandler();
    if (callback) {
      callback(connInfo);
    }
  });
  active.client.onDisconnect([common](BLEClient, const BLEConnInfo &connInfo, uint8_t reason) {
    auto impl = common.lock();
    if (!impl) {
      return;
    }
    auto callback = impl->loadDisconnectHandler();
    if (callback) {
      callback(connInfo, reason);
    }
  });

  BTStatus status = active.client.connect(address, timeoutMs);
  if (!status) {
    return abortStart(status);
  }

  BLERemoteService service = active.client.getService(nusServiceUUID());
  if (!service) {
    return abortStart(BTStatus::NotFound);
  }
  active.remoteTx = service.getCharacteristic(nusTxCharUUID());
  active.remoteRx = service.getCharacteristic(nusRxCharUUID());
  if (!active.remoteTx || !active.remoteRx) {
    return abortStart(BTStatus::NotFound);
  }

  BLEClient streamClient = active.client;
  status = active.remoteTx.subscribe(
    BLESubscription::Notify,
    [common, streamClient](BLERemoteCharacteristic, const uint8_t *data, size_t len, BLESubscription) {
      auto impl = common.lock();
      if (!impl || !data || len == 0) {
        return;
      }
      auto callback = impl->loadPeerDataHandler();
      if (callback) {
        callback(streamClient.getConnInfo(), data, len);
      }
      impl->rxBuf.write(reinterpret_cast<const char *>(data), len);
    }
  );
  if (!status) {
    return abortStart(status);
  }
  if (!Impl::finishRoleStart(role)) {
    end();
    return BTStatus::Busy;
  }
  return BTStatus::OK;
}

#endif /* BLE_ENABLED */
