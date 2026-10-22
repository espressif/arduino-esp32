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

struct BLEStreamClientRole final : BLEStream::Impl::Role {
  BLEStream::Impl &common;
  BLEClient client;
  BLERemoteCharacteristic remoteTx;
  BLERemoteCharacteristic remoteRx;

  explicit BLEStreamClientRole(BLEStream::Impl &impl) : common(impl) {}

  void end() override {
    ending = true;
    if (remoteTx) {
      (void)remoteTx.unsubscribe();
    }
    if (client) {
      client.resetCallbacks();
      (void)client.disconnect();
    }
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

  _impl->storeRole(role);
  auto &active = *role;
  active.client.onConnect([this](BLEClient, const BLEConnInfo &connInfo) {
    _impl->peerCount = 1;
    if (_impl->onConnectCb) {
      _impl->onConnectCb(connInfo);
    }
  });
  active.client.onDisconnect([this](BLEClient, const BLEConnInfo &connInfo, uint8_t reason) {
    _impl->peerCount = 0;
    if (_impl->onDisconnectCb) {
      _impl->onDisconnectCb(connInfo, reason);
    }
  });

  BTStatus status = active.client.connect(address, timeoutMs);
  if (!status) {
    end();
    return status;
  }

  BLERemoteService service = active.client.getService(nusServiceUUID());
  if (!service) {
    end();
    return BTStatus::NotFound;
  }
  active.remoteTx = service.getCharacteristic(nusTxCharUUID());
  active.remoteRx = service.getCharacteristic(nusRxCharUUID());
  if (!active.remoteTx || !active.remoteRx) {
    end();
    return BTStatus::NotFound;
  }

  status = active.remoteTx.subscribe(
    BLESubscription::Notify,
    [this](BLERemoteCharacteristic, const uint8_t *data, size_t len, BLERemoteCharacteristic::Delivery) {
      if (data && len > 0) {
        _impl->rxBuf.write(reinterpret_cast<const char *>(data), len);
      }
    }
  );
  if (!status) {
    end();
    return status;
  }
  return BTStatus::OK;
}

#endif /* BLE_ENABLED */
