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

#include "impl/common/BLEGuards.h"
#if BLE_ENABLED

#include <Arduino.h>
#include <atomic>
#include "BLEStream.h"
#include "BLE.h"
#include "BLEAdvertising.h"
#include "BLEScan.h"
#include "BLERemoteService.h"
#include "cbuf.h"

// Nordic UART Service UUIDs
static const BLEUUID kNUS_ServiceUUID("6e400001-b5a3-f393-e0a9-e50e24dcca9e");
static const BLEUUID kNUS_RxCharUUID("6e400002-b5a3-f393-e0a9-e50e24dcca9e");
static const BLEUUID kNUS_TxCharUUID("6e400003-b5a3-f393-e0a9-e50e24dcca9e");

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
// Impl
// --------------------------------------------------------------------------

struct BLEStream::Impl {
  enum class Mode {
    None,
    Server,
    Client
  } mode = Mode::None;

  // Server mode
  BLEServer server;
  BLECharacteristic txChr;  // Server TX (Notify) -> client receives
  BLECharacteristic rxChr;  // Server RX (Write)  -> client sends

  // Client mode
  BLEClient client;
  BLERemoteCharacteristic remoteTx;  // Remote TX (subscribe for notifications)
  BLERemoteCharacteristic remoteRx;  // Remote RX (write to send data)

  cbuf rxBuf;
  // Number of currently connected peers. In server mode multiple centrals can be
  // connected at once (writes are notified to all subscribers, RX is merged into
  // rxBuf); a plain bool would drop to "disconnected" when any one peer left while
  // others were still connected. Written from the BLE event task in the
  // connect/disconnect callbacks and read from the user task, so it is atomic to
  // give well-defined concurrent access. Client mode uses 0/1.
  std::atomic<int> peerCount{0};

  ConnectHandler onConnectCb = nullptr;
  DisconnectHandler onDisconnectCb = nullptr;
  PeerDataHandler onPeerDataCb = nullptr;

  explicit Impl(size_t rxBufSize) : rxBuf(rxBufSize) {}
};

// --------------------------------------------------------------------------
// BLEStream lifecycle
// --------------------------------------------------------------------------

BLEStream::BLEStream(size_t rxBufferSize) : _impl(new Impl(rxBufferSize)) {}

BLEStream::~BLEStream() {
  end();
  // The lambdas registered with _impl->client capture `this`.  Clear them
  // before deleting _impl so the BLEClient::Impl (which may outlive us via
  // nimbleRef) never invokes a callback on a destroyed BLEStream.
  if (_impl && _impl->client) {
    _impl->client.resetCallbacks();
  }
  delete _impl;
}

BTStatus BLEStream::begin(const String &deviceName) {
  if (!_impl || _impl->mode != Impl::Mode::None) {
    return BTStatus::InvalidState;
  }

  // Initialize BLE if not already
  if (!BLE.isInitialized()) {
    BTStatus s = BLE.begin(deviceName);
    if (!s) {
      return s;
    }
  }

  _impl->server = BLE.createServer();
  if (!_impl->server) {
    return BTStatus::Fail;
  }

  _impl->server.onConnect([this](BLEServer, const BLEConnInfo &connInfo) {
    _impl->peerCount.fetch_add(1, std::memory_order_relaxed);
    if (_impl->onConnectCb) {
      _impl->onConnectCb(connInfo);
    }
  });

  _impl->server.onDisconnect([this](BLEServer, const BLEConnInfo &connInfo, uint8_t reason) {
    if (_impl->peerCount.load(std::memory_order_relaxed) > 0) {
      _impl->peerCount.fetch_sub(1, std::memory_order_relaxed);
    }
    if (_impl->onDisconnectCb) {
      _impl->onDisconnectCb(connInfo, reason);
    }
  });

  _impl->server.advertiseOnDisconnect(true);

  BLEService svc = _impl->server.createService(kNUS_ServiceUUID);

  // TX characteristic: server notifies, client may also Read. Nordic's NUS
  // spec defines TX as notify-only, but BLEStream is a generic Arduino
  // Stream wrapper that frequently talks to generic BLE tooling
  // (nRF Connect, LightBlue, Web Bluetooth demos, etc.) — those clients
  // commonly issue an ATT_READ on discovery to populate their UI. Allowing
  // Read returns the most recently notified bytes, which is the closest
  // meaningful "current value" and keeps generic clients happy.
  _impl->txChr = svc.createCharacteristic(kNUS_TxCharUUID, BLEProperty::Read | BLEProperty::Notify, BLEPermissions::OpenRead);

  // RX characteristic: client writes, server receives. Open access to match
  // the common deployed NUS behavior; add encryption with
  // BLEPermissions::EncryptedWrite if the profile calls for it.
  _impl->rxChr = svc.createCharacteristic(kNUS_RxCharUUID, BLEProperty::Write | BLEProperty::WriteNR, BLEPermissions::OpenWrite);
  _impl->rxChr.onWrite([this](BLECharacteristic chr, const BLEConnInfo &connInfo) {
    size_t len = 0;
    const uint8_t *data = chr.getValue(&len);
    if (data && len > 0) {
      _impl->rxBuf.write(reinterpret_cast<const char *>(data), len);
      if (_impl->onPeerDataCb) {
        _impl->onPeerDataCb(connInfo, data, len);
      }
    }
  });

  // Stop advertising before (re-)starting the server so the GATT table is
  // mutable (NimBLE requires no active GAP procedures during registration).
  BLEAdvertising adv = BLE.getAdvertising();
  adv.stop();

  BTStatus srvStatus = _impl->server.start();
  if (!srvStatus) {
    return srvStatus;
  }

  // Reconfigure advertising to include the NUS service UUID.
  adv.reset();
  adv.setName(deviceName);
  adv.addServiceUUID(kNUS_ServiceUUID);
  BTStatus advStatus = adv.start();
  if (!advStatus) {
    return advStatus;
  }

  _impl->mode = Impl::Mode::Server;
  return BTStatus::OK;
}

BTStatus BLEStream::beginClient(const BTAddress &address, uint32_t timeoutMs) {
  if (!_impl || _impl->mode != Impl::Mode::None) {
    return BTStatus::InvalidState;
  }

  // Initialize BLE if not already
  if (!BLE.isInitialized()) {
    BTStatus s = BLE.begin("");
    if (!s) {
      return s;
    }
  }

  _impl->client = BLE.createClient();
  if (!_impl->client) {
    return BTStatus::Fail;
  }

  _impl->client.onConnect([this](BLEClient, const BLEConnInfo &connInfo) {
    _impl->peerCount = 1;
    if (_impl->onConnectCb) {
      _impl->onConnectCb(connInfo);
    }
  });

  _impl->client.onDisconnect([this](BLEClient, const BLEConnInfo &connInfo, uint8_t reason) {
    _impl->peerCount = 0;
    if (_impl->onDisconnectCb) {
      _impl->onDisconnectCb(connInfo, reason);
    }
  });

  BTStatus s = _impl->client.connect(address, timeoutMs);
  if (!s) {
    return s;
  }

  // Discover NUS service
  BLERemoteService svc = _impl->client.getService(kNUS_ServiceUUID);
  if (!svc) {
    _impl->client.disconnect();
    return BTStatus::NotFound;
  }

  _impl->remoteTx = svc.getCharacteristic(kNUS_TxCharUUID);
  _impl->remoteRx = svc.getCharacteristic(kNUS_RxCharUUID);

  if (!_impl->remoteTx || !_impl->remoteRx) {
    _impl->client.disconnect();
    return BTStatus::NotFound;
  }

  // Subscribe to TX notifications (data from server -> us)
  _impl->remoteTx.subscribe(true, [this](BLERemoteCharacteristic, const uint8_t *data, size_t len, bool) {
    if (data && len > 0) {
      _impl->rxBuf.write(reinterpret_cast<const char *>(data), len);
    }
  });

  _impl->mode = Impl::Mode::Client;
  return BTStatus::OK;
}

void BLEStream::end() {
  if (!_impl) {
    return;
  }
  if (_impl->mode == Impl::Mode::Client && _impl->client) {
    _impl->client.disconnect();
  }
  _impl->peerCount = 0;
  _impl->mode = Impl::Mode::None;
}

bool BLEStream::connected() const {
  return _impl && _impl->peerCount > 0;
}

size_t BLEStream::peerCount() const {
  if (!_impl || _impl->peerCount < 0) {
    return 0;
  }
  return static_cast<size_t>(_impl->peerCount);
}

std::vector<BLEConnInfo> BLEStream::peers() const {
  if (!_impl) {
    return {};
  }
  if (_impl->mode == Impl::Mode::Server && _impl->server) {
    return _impl->server.getConnections();
  }
  if (_impl->mode == Impl::Mode::Client && _impl->client && _impl->peerCount > 0) {
    return {_impl->client.getConnInfo()};
  }
  return {};
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
  if (!_impl || _impl->peerCount <= 0 || size == 0) {
    return 0;
  }

  if (_impl->mode == Impl::Mode::Server) {
    // Server sends via TX characteristic notifications
    // Chunk at MTU - 3 (ATT header overhead)
    uint16_t mtu = BLE.getMTU();
    uint16_t chunkSize = (mtu > 3) ? (mtu - 3) : 20;
    size_t sent = 0;

    while (sent < size) {
      size_t len = size - sent;
      if (len > chunkSize) {
        len = chunkSize;
      }

      _impl->txChr.setValue(buffer + sent, len);
      BTStatus s = _impl->txChr.notify();
      if (!s) {
        break;
      }
      sent += len;
    }
    return sent;
  } else if (_impl->mode == Impl::Mode::Client) {
    // Client sends via RX characteristic writes
    uint16_t mtu = _impl->client.getMTU();
    uint16_t chunkSize = (mtu > 3) ? (mtu - 3) : 20;
    size_t sent = 0;

    while (sent < size) {
      size_t len = size - sent;
      if (len > chunkSize) {
        len = chunkSize;
      }

      BTStatus s = _impl->remoteRx.writeValue(buffer + sent, len, false);
      if (!s) {
        break;
      }
      sent += len;
    }
    return sent;
  }

  return 0;
}

size_t BLEStream::writeTo(const BLEConnInfo &peer, const uint8_t *buffer, size_t size) {
  if (!_impl || _impl->peerCount <= 0 || !buffer || size == 0) {
    return 0;
  }

  if (_impl->mode == Impl::Mode::Server) {
    uint16_t mtu = BLE.getMTU();
    uint16_t chunkSize = (mtu > 3) ? (mtu - 3) : 20;
    size_t sent = 0;

    while (sent < size) {
      size_t len = size - sent;
      if (len > chunkSize) {
        len = chunkSize;
      }

      _impl->txChr.setValue(buffer + sent, len);
      BTStatus s = _impl->txChr.notify(peer.getHandle());
      if (!s) {
        break;
      }
      sent += len;
    }
    return sent;
  }

  if (_impl->mode == Impl::Mode::Client && _impl->client) {
    BLEConnInfo conn = _impl->client.getConnInfo();
    if (conn.getHandle() != peer.getHandle()) {
      return 0;
    }
    uint16_t mtu = _impl->client.getMTU();
    uint16_t chunkSize = (mtu > 3) ? (mtu - 3) : 20;
    size_t sent = 0;

    while (sent < size) {
      size_t len = size - sent;
      if (len > chunkSize) {
        len = chunkSize;
      }

      BTStatus s = _impl->remoteRx.writeValue(buffer + sent, len, false);
      if (!s) {
        break;
      }
      sent += len;
    }
    return sent;
  }

  return 0;
}

void BLEStream::flush() {
  // BLE notifications are sent immediately; no buffering to flush
}

BLEStream::operator bool() const {
  return _impl && _impl->mode != Impl::Mode::None;
}

// --------------------------------------------------------------------------
// Callbacks
// --------------------------------------------------------------------------

void BLEStream::onConnect(ConnectHandler handler) {
  if (_impl) {
    _impl->onConnectCb = handler;
  }
}

void BLEStream::onDisconnect(DisconnectHandler handler) {
  if (_impl) {
    _impl->onDisconnectCb = handler;
  }
}

void BLEStream::onPeerData(PeerDataHandler handler) {
  if (_impl) {
    _impl->onPeerDataCb = handler;
  }
}

void BLEStream::resetCallbacks() {
  if (!_impl) {
    return;
  }
  _impl->onConnectCb = nullptr;
  _impl->onDisconnectCb = nullptr;
  _impl->onPeerDataCb = nullptr;
}

#endif /* BLE_ENABLED */
