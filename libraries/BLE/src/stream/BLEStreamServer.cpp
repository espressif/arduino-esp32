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
 * @file BLEStreamServer.cpp
 * @brief Peripheral (server) NUS role for `BLEStream` (`begin()`).
 */

#include "core/BLEGuards.h"
#if BLE_ENABLED

#include "stream/BLEStreamImpl.h"
#include "BLE.h"
#include "advertising/BLEAdvertising.h"
#include "gatt/BLECharacteristic.h"
#include "server/BLEServer.h"
#include <algorithm>

#include "core/BLEValidationHooks.h"

namespace {

void retainFirstFailure(BTStatus &result, BTStatus status) {
  if (result && !status) {
    result = status;
  }
}

}  // namespace

// ==========================================================================
// 1. Server role state & GATT callback handlers
// ==========================================================================

struct BLEStreamServerRole final : BLEStream::Impl::Role {
  BLEStream::Impl &common;
  BLEServer server;
  BLEService service;
  BLECharacteristic txChr;
  BLECharacteristic rxChr;

  explicit BLEStreamServerRole(BLEStream::Impl &impl) : common(impl) {}

  static void onServerConnect(const std::weak_ptr<BLEStream::Impl> &common, const BLEConnInfo &connInfo) {
    auto impl = common.lock();
    if (!impl) {
      return;
    }
    if (auto callback = impl->loadConnectHandler()) {
      callback(connInfo);
    }
  }

  static void onServerDisconnect(const std::weak_ptr<BLEStream::Impl> &common, const BLEConnInfo &connInfo, uint8_t reason) {
    auto impl = common.lock();
    if (!impl) {
      return;
    }
    if (auto callback = impl->loadDisconnectHandler()) {
      callback(connInfo, reason);
    }
  }

  static void onRxCharacteristicWrite(const std::weak_ptr<BLEStream::Impl> &common, BLECharacteristic &chr, const BLEConnInfo &connInfo) {
    auto impl = common.lock();
    if (!impl) {
      return;
    }
    size_t len = 0;
    const uint8_t *data = chr.getValue(&len);
    if (data && len > 0) {
      if (auto callback = impl->loadPeerDataHandler()) {
        callback(connInfo, data, len);
      }
      impl->rxBuf.write(reinterpret_cast<const char *>(data), len);
    }
  }

  static BTStatus
    cancelStart(BLEStream &stream, const std::shared_ptr<BLEStream::Impl> &impl, const std::shared_ptr<BLEStreamServerRole> &role, BTStatus status) {
    if (BLEStream::Impl::cancelRoleStart(impl, role)) {
      return status;
    }
    stream.end();
    return BTStatus(BTStatus::Busy);
  }

  static BTStatus
    abortRegisteredStart(BLEStream &stream, const std::shared_ptr<BLEStream::Impl> &impl, const std::shared_ptr<BLEStreamServerRole> &role, BTStatus status) {
    BTStatus removeStatus;
#if BLE_VALIDATION_HOOKS
    if (BLEValidationHooks::consumeFailure(BLEHooks.stream.removeFailures)) {
      removeStatus = BTStatus::Busy;
    } else
#endif
    {
      removeStatus = role->server.removeService(role->service);
    }
    if (removeStatus) {
      return cancelStart(stream, impl, role, status);
    }
    // Keep the role registered and close admission until normal teardown or
    // recovery can remove the partially registered native service.
    BLEStream::Impl::finishRoleStart(role);
    stream.end();
    return status;
  }

  BTStatus end() override {
    ending = true;
    BTStatus result = BTStatus::OK;
    bool disconnectTerminal = true;
    if (server) {
      // Close the restart gate before stopping advertising. Deferred backend
      // restart callbacks take the same server mutex and re-check this flag.
      server.advertiseOnDisconnect(false);
    }
    if (BLE.isInitialized()) {
      BLEAdvertising adv = BLE.getAdvertising();
      if (adv) {
        retainFirstFailure(result, adv.stop());
      }
    }
    if (server) {
      // A BLE ACL is shared by every service, so server-mode shutdown
      // disconnects all peers while preserving unrelated local services.
      for (const auto &conn : server.getConnections()) {
        BTStatus status = server.disconnect(conn.getHandle());
#if BLE_VALIDATION_HOOKS
        if (BLEValidationHooks::consumeFailure(BLEHooks.stream.disconnectRaces)) {
          status = BTStatus::NotConnected;
        }
#endif
        // A peer may close between getConnections() and disconnect(). In that
        // case the requested terminal state is already reached.
        if (status != BTStatus::NotConnected) {
          retainFirstFailure(result, status);
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
#if BLE_VALIDATION_HOOKS
        if (BLEValidationHooks::consumeFailure(BLEHooks.stream.removeFailures)) {
          retainFirstFailure(result, BTStatus::Busy);
        } else
#endif
        {
          retainFirstFailure(result, server.removeService(service));
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

// ==========================================================================
// 2. Public BLEStream::begin() entry point
// ==========================================================================

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
  role->service = role->server.createService(serviceUuid);
  if (!role->service) {
    return BLEStreamServerRole::cancelStart(*this, _impl, role, BTStatus::Fail);
  }

  role->txChr = role->service.createCharacteristic(nusTxCharUUID(), BLEProperty::Read | BLEProperty::Notify, BLEPermission::ReadOpen);
  role->rxChr = role->service.createCharacteristic(nusRxCharUUID(), BLEProperty::Write | BLEProperty::WriteNR, BLEPermission::WriteOpen);
  if (!role->txChr || !role->rxChr) {
    return BLEStreamServerRole::abortRegisteredStart(*this, _impl, role, BTStatus::Fail);
  }

  BLEAdvertising advertising = BLE.getAdvertising();
  advertising.stop();
  BTStatus status = role->server.start();
  if (!status) {
    return BLEStreamServerRole::abortRegisteredStart(*this, _impl, role, status);
  }
#if BLE_VALIDATION_HOOKS
  if (BLEValidationHooks::consumeFailure(BLEHooks.stream.startFailures)) {
    return BLEStreamServerRole::abortRegisteredStart(*this, _impl, role, BTStatus::Fail);
  }
#endif

  auto &active = *role;
  std::weak_ptr<Impl> common = _impl;
  active.server.onConnect([common](BLEServer, const BLEConnInfo &connInfo) {
    BLEStreamServerRole::onServerConnect(common, connInfo);
  });
  active.server.onDisconnect([common](BLEServer, const BLEConnInfo &connInfo, uint8_t reason) {
    BLEStreamServerRole::onServerDisconnect(common, connInfo, reason);
  });
  active.server.advertiseOnDisconnect(true);
  active.rxChr.onWrite([common](BLECharacteristic chr, const BLEConnInfo &connInfo) {
    BLEStreamServerRole::onRxCharacteristicWrite(common, chr, connInfo);
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
