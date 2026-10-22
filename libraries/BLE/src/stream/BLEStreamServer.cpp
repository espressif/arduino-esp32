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
#include "advertising/BLEAdvertising.h"
#include "gatt/BLECharacteristic.h"
#include "server/BLEServer.h"
#include <algorithm>

struct BLEStreamServerRole final : BLEStream::Impl::Role {
  BLEStream::Impl &common;
  BLEServer server;
  BLEService service;
  BLECharacteristic txChr;
  BLECharacteristic rxChr;

  explicit BLEStreamServerRole(BLEStream::Impl &impl) : common(impl) {}

  void end() override {
    ending = true;
    if (server) {
      // Close the restart gate before stopping advertising. Deferred backend
      // restart callbacks take the same server mutex and re-check this flag.
      server.advertiseOnDisconnect(false);
    }
    if (BLE.isInitialized()) {
      BLEAdvertising adv = BLE.getAdvertising();
      if (adv) {
        (void)adv.stop();
      }
    }
    if (server) {
      // A BLE ACL is shared by every service, so server-mode shutdown
      // disconnects all peers while preserving unrelated local services.
      for (const auto &conn : server.getConnections()) {
        (void)server.disconnect(conn.getHandle());
      }
    }
    if (rxChr) {
      rxChr.resetCallbacks();
    }
    if (txChr) {
      txChr.resetCallbacks();
    }
    if (server) {
      server.onConnect(nullptr);
      server.onDisconnect(nullptr);
      if (service) {
        (void)server.removeService(service);
      }
    }
  }

  std::vector<BLEConnInfo> peers() const override {
    return server ? server.getConnections() : std::vector<BLEConnInfo>{};
  }

  size_t write(const uint8_t *buffer, size_t size) override {
    uint16_t mtu = 0;
    if (server && txChr) {
      for (uint16_t handle : txChr.getSubscribedConnections()) {
        uint16_t peerMtu = server.getPeerMTU(handle);
        if (peerMtu > 0 && (mtu == 0 || peerMtu < mtu)) {
          mtu = peerMtu;
        }
      }
    }
    if (mtu == 0 && server) {
      for (const auto &peer : server.getConnections()) {
        uint16_t peerMtu = peer.getMTU();
        if (peerMtu > 0 && (mtu == 0 || peerMtu < mtu)) {
          mtu = peerMtu;
        }
      }
    }
    if (mtu == 0) {
      mtu = BLE.getMTU();
    }
    return writeChunks(buffer, size, mtu, false, 0);
  }

  size_t writeTo(const BLEConnInfo &peer, const uint8_t *buffer, size_t size) override {
    uint16_t mtu = server ? server.getPeerMTU(peer.getHandle()) : 0;
    if (mtu == 0) {
      mtu = peer.getMTU();
    }
    if (mtu == 0) {
      mtu = BLE.getMTU();
    }
    return writeChunks(buffer, size, mtu, true, peer.getHandle());
  }

private:
  size_t writeChunks(const uint8_t *buffer, size_t size, uint16_t mtu, bool targeted, uint16_t connHandle) {
    const uint16_t chunkSize = (mtu > 3) ? (mtu - 3) : 20;
    size_t sent = 0;
    while (sent < size) {
      size_t len = std::min<size_t>(size - sent, chunkSize);
      txChr.setValue(buffer + sent, len);
      BTStatus status = targeted ? txChr.notify(connHandle) : txChr.notify();
      if (!status) {
        break;
      }
      sent += len;
    }
    return sent;
  }
};

BTStatus BLEStream::begin(const String &deviceName) {
  if (!_impl || _impl->loadRole()) {
    return BTStatus::InvalidState;
  }
  if (!BLE.isInitialized()) {
    BTStatus status = BLE.begin(deviceName);
    if (!status) {
      return status;
    }
  }

  auto role = std::make_shared<BLEStreamServerRole>(*_impl);
  role->server = BLE.createServer();
  if (!role->server) {
    return BTStatus::Fail;
  }

  const BLEUUID serviceUuid = nusServiceUUID();
  if (role->server.getService(serviceUuid)) {
    return BTStatus::Busy;
  }
  role->service = role->server.createService(serviceUuid);
  if (!role->service) {
    return BTStatus::Fail;
  }

  role->txChr = role->service.createCharacteristic(
    nusTxCharUUID(), BLEProperty::Read | BLEProperty::Notify, BLEPermission::ReadOpen
  );
  role->rxChr = role->service.createCharacteristic(
    nusRxCharUUID(), BLEProperty::Write | BLEProperty::WriteNR, BLEPermission::WriteOpen
  );
  if (!role->txChr || !role->rxChr) {
    (void)role->server.removeService(role->service);
    return BTStatus::Fail;
  }

  BLEAdvertising advertising = BLE.getAdvertising();
  advertising.stop();
  BTStatus status = role->server.start();
  if (!status) {
    (void)role->server.removeService(role->service);
    return status;
  }

  _impl->storeRole(role);
  auto &active = *role;
  active.server.onConnect([this](BLEServer, const BLEConnInfo &connInfo) {
    _impl->peerCount.fetch_add(1, std::memory_order_relaxed);
    if (_impl->onConnectCb) {
      _impl->onConnectCb(connInfo);
    }
  });
  active.server.onDisconnect([this](BLEServer, const BLEConnInfo &connInfo, uint8_t reason) {
    if (_impl->peerCount.load(std::memory_order_relaxed) > 0) {
      _impl->peerCount.fetch_sub(1, std::memory_order_relaxed);
    }
    if (_impl->onDisconnectCb) {
      _impl->onDisconnectCb(connInfo, reason);
    }
  });
  active.server.advertiseOnDisconnect(true);
  active.rxChr.onWrite([this](BLECharacteristic chr, const BLEConnInfo &connInfo) {
    size_t len = 0;
    const uint8_t *data = chr.getValue(&len);
    if (data && len > 0) {
      _impl->rxBuf.write(reinterpret_cast<const char *>(data), len);
      if (_impl->onPeerDataCb) {
        _impl->onPeerDataCb(connInfo, data, len);
      }
    }
  });

  advertising.reset();
  advertising.setName(deviceName);
  advertising.addServiceUUID(serviceUuid);
  status = advertising.start();
  if (!status) {
    end();
    return status;
  }
  return BTStatus::OK;
}

#endif /* BLE_ENABLED */
