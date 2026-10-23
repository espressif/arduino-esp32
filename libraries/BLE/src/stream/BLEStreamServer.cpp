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

#if defined(BLE_STREAM_VALIDATION_HOOKS)
extern "C" {
extern std::atomic<int> bleStreamTestStartFailures;
extern std::atomic<int> bleStreamTestRemoveFailures;
extern std::atomic<int> bleStreamTestDisconnectRaces;
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

struct BLEStreamServerRole final : BLEStream::Impl::Role {
  BLEStream::Impl &common;
  BLEServer server;
  BLEService service;
  BLECharacteristic txChr;
  BLECharacteristic rxChr;

  explicit BLEStreamServerRole(BLEStream::Impl &impl) : common(impl) {}

  BTStatus end() override {
    ending = true;
    BTStatus result = BTStatus::OK;
    bool disconnectTerminal = true;
    auto retainFailure = [&result](BTStatus status) {
      if (result && !status) {
        result = status;
      }
    };
    if (server) {
      // Close the restart gate before stopping advertising. Deferred backend
      // restart callbacks take the same server mutex and re-check this flag.
      server.advertiseOnDisconnect(false);
    }
    if (BLE.isInitialized()) {
      BLEAdvertising adv = BLE.getAdvertising();
      if (adv) {
        retainFailure(adv.stop());
      }
    }
    if (server) {
      // A BLE ACL is shared by every service, so server-mode shutdown
      // disconnects all peers while preserving unrelated local services.
      for (const auto &conn : server.getConnections()) {
        BTStatus status = server.disconnect(conn.getHandle());
#if defined(BLE_STREAM_VALIDATION_HOOKS)
        if (consumeValidationFailure(bleStreamTestDisconnectRaces)) {
          status = BTStatus::NotConnected;
        }
#endif
        // A peer may close between getConnections() and disconnect(). In that
        // case the requested terminal state is already reached.
        if (status != BTStatus::NotConnected) {
          retainFailure(status);
          if (!status) {
            disconnectTerminal = false;
          }
        }
      }
    }
    if (!disconnectTerminal) {
      // Preserve the native callbacks and GATT tree while the role is retained
      // for retry. A late real disconnect must still update peer state and
      // reach the user's onDisconnect handler.
      return result;
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
      if (service && service.getServer()) {
#if defined(BLE_STREAM_VALIDATION_HOOKS)
        if (consumeValidationFailure(bleStreamTestRemoveFailures)) {
          retainFailure(BTStatus::Busy);
        } else
#endif
        {
          retainFailure(server.removeService(service));
        }
      }
    }
    return result;
  }

  void detachCallbacks() override {
    if (rxChr) {
      rxChr.resetCallbacks();
    }
    if (txChr) {
      txChr.resetCallbacks();
    }
    if (server) {
      server.onConnect(nullptr);
      server.onDisconnect(nullptr);
      server.advertiseOnDisconnect(false);
    }
  }

  size_t connectedCount() const override {
    return server ? server.getConnectedCount() : 0;
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
  if (!Impl::activateRole(_impl, role)) {
    return BTStatus::Busy;
  }
  auto cancelStart = [&](BTStatus status) {
    if (Impl::cancelRoleStart(_impl, role)) {
      return status;
    }
    end();
    return BTStatus(BTStatus::Busy);
  };
  auto abortRegisteredStart = [&](BTStatus status) {
    BTStatus removeStatus;
#if defined(BLE_STREAM_VALIDATION_HOOKS)
    if (consumeValidationFailure(bleStreamTestRemoveFailures)) {
      removeStatus = BTStatus::Busy;
    } else
#endif
    {
      removeStatus = role->server.removeService(role->service);
    }
    if (removeStatus) {
      return cancelStart(status);
    }
    // Keep the role registered and close admission until normal teardown or
    // recovery can remove the partially registered native service.
    Impl::finishRoleStart(role);
    end();
    return status;
  };
  role->service = role->server.createService(serviceUuid);
  if (!role->service) {
    return cancelStart(BTStatus::Fail);
  }

  role->txChr = role->service.createCharacteristic(
    nusTxCharUUID(), BLEProperty::Read | BLEProperty::Notify, BLEPermission::ReadOpen
  );
  role->rxChr = role->service.createCharacteristic(
    nusRxCharUUID(), BLEProperty::Write | BLEProperty::WriteNR, BLEPermission::WriteOpen
  );
  if (!role->txChr || !role->rxChr) {
    return abortRegisteredStart(BTStatus::Fail);
  }

  BLEAdvertising advertising = BLE.getAdvertising();
  advertising.stop();
  BTStatus status = role->server.start();
  if (!status) {
    return abortRegisteredStart(status);
  }
#if defined(BLE_STREAM_VALIDATION_HOOKS)
  if (consumeValidationFailure(bleStreamTestStartFailures)) {
    return abortRegisteredStart(BTStatus::Fail);
  }
#endif

  auto &active = *role;
  std::weak_ptr<Impl> common = _impl;
  active.server.onConnect([common](BLEServer server, const BLEConnInfo &connInfo) {
    auto impl = common.lock();
    if (!impl) {
      return;
    }
    impl->peerCount.store(static_cast<int>(server.getConnections().size()), std::memory_order_relaxed);
    auto callback = impl->loadConnectHandler();
    if (callback) {
      callback(connInfo);
    }
  });
  active.server.onDisconnect([common](BLEServer server, const BLEConnInfo &connInfo, uint8_t reason) {
    auto impl = common.lock();
    if (!impl) {
      return;
    }
    impl->peerCount.store(static_cast<int>(server.getConnections().size()), std::memory_order_relaxed);
    auto callback = impl->loadDisconnectHandler();
    if (callback) {
      callback(connInfo, reason);
    }
  });
  active.server.advertiseOnDisconnect(true);
  active.rxChr.onWrite([common](BLECharacteristic chr, const BLEConnInfo &connInfo) {
    auto impl = common.lock();
    if (!impl) {
      return;
    }
    size_t len = 0;
    const uint8_t *data = chr.getValue(&len);
    if (data && len > 0) {
      impl->rxBuf.write(reinterpret_cast<const char *>(data), len);
      auto callback = impl->loadPeerDataHandler();
      if (callback) {
        callback(connInfo, data, len);
      }
    }
  });

  advertising.reset();
  advertising.setName(deviceName);
  advertising.addServiceUUID(serviceUuid);
  status = advertising.start();
  if (!status) {
    Impl::finishRoleStart(role);
    end();
    return status;
  }
  if (!Impl::finishRoleStart(role)) {
    end();
    return BTStatus::Busy;
  }
  return BTStatus::OK;
}

#endif /* BLE_ENABLED */
