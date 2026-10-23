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

#if defined(BLE_STREAM_VALIDATION_HOOKS)
extern "C" {
extern std::atomic<int> bleStreamTestDisconnectRaces;
extern std::atomic<bool> bleStreamTestHoldFinalRoleRelease;
extern std::atomic<bool> bleStreamTestFinalRoleReleaseReached;
}

static bool consumeValidationFailure(std::atomic<int> &remaining) {
  int value = remaining.load(std::memory_order_relaxed);
  while (value > 0) {
    if (remaining.compare_exchange_weak(value, value - 1, std::memory_order_relaxed)) {
      return true;
    }
  }
  return false;
}
#endif

struct BLEStreamClientRole final : BLEStream::Impl::Role {
  BLEStream::Impl &common;
  BLEClient client;
  BLERemoteCharacteristic remoteTx;
  BLERemoteCharacteristic remoteRx;

  explicit BLEStreamClientRole(BLEStream::Impl &impl) : common(impl) {}

#if defined(BLE_STREAM_VALIDATION_HOOKS)
  ~BLEStreamClientRole() override {
    bleStreamTestFinalRoleReleaseReached.store(true, std::memory_order_release);
    while (bleStreamTestHoldFinalRoleRelease.load(std::memory_order_acquire)) {
      vTaskDelay(1);
    }
  }
#endif

  BTStatus end() override {
    ending = true;
    BTStatus result = BTStatus::OK;
    bool terminal = true;
    if (client) {
      if (client.isConnected()) {
        result = client.disconnect();
#if defined(BLE_STREAM_VALIDATION_HOOKS)
        if (consumeValidationFailure(bleStreamTestDisconnectRaces)) {
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
    if (terminal) {
      remoteTx = BLERemoteCharacteristic();
      remoteRx = BLERemoteCharacteristic();
    }
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
    if (client && common.peerCount > 0) {
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
  if (!_impl->ensureEndRetryTimer()) {
    return BTStatus::NoMemory;
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
    impl->peerCount = 1;
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
    impl->peerCount = 0;
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

  status = active.remoteTx.subscribe(
    BLESubscription::Notify,
    [common](BLERemoteCharacteristic, const uint8_t *data, size_t len, BLERemoteCharacteristic::Delivery) {
      auto impl = common.lock();
      if (impl && data && len > 0) {
        impl->rxBuf.write(reinterpret_cast<const char *>(data), len);
      }
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
