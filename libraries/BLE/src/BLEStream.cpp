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

#include "impl/BLEGuards.h"
#if BLE_ENABLED

#include <Arduino.h>
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

/**
 * @brief Get the Nordic UART Service UUID (NUS).
 * @return 128-bit service UUID.
 */
BLEUUID BLEStream::nusServiceUUID() {
  return kNUS_ServiceUUID;
}

/**
 * @brief Get the NUS RX characteristic UUID (central writes to the peripheral).
 * @return 128-bit characteristic UUID.
 */
BLEUUID BLEStream::nusRxCharUUID() {
  return kNUS_RxCharUUID;
}

/**
 * @brief Get the NUS TX characteristic UUID (peripheral notifies the central).
 * @return 128-bit characteristic UUID.
 */
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
  volatile bool isConnected = false;

  ConnectHandler onConnectCb = nullptr;
  DisconnectHandler onDisconnectCb = nullptr;

  explicit Impl(size_t rxBufSize) : rxBuf(rxBufSize) {}
};

// --------------------------------------------------------------------------
// BLEStream lifecycle
// --------------------------------------------------------------------------

/**
 * @brief Construct a BLEStream with the given receive ring buffer size.
 * @param rxBufferSize Size in bytes of the internal receive buffer.
 */
BLEStream::BLEStream(size_t rxBufferSize) : _impl(new Impl(rxBufferSize)) {}

/**
 * @brief Destroy the BLEStream, calling end() and freeing the Impl.
 */
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

/**
 * @brief Start as a NUS server: initialize BLE, create the GATT server and NUS
 *        service, configure advertising, and begin accepting connections.
 * @param deviceName BLE device name used for BLE.begin() and advertising.
 * @return BTStatus::OK on success, BTStatus::InvalidState if already started,
 *         or a backend error status on failure.
 * @note Initializes BLE if not already done. Stops and reconfigures advertising
 *       to include the NUS service UUID. Enables auto-advertise on disconnect.
 */
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
    _impl->isConnected = true;
    if (_impl->onConnectCb) {
      _impl->onConnectCb(connInfo);
    }
  });

  _impl->server.onDisconnect([this](BLEServer, const BLEConnInfo &connInfo, uint8_t reason) {
    _impl->isConnected = false;
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
  _impl->rxChr.onWrite([this](BLECharacteristic chr, const BLEConnInfo &) {
    size_t len = 0;
    const uint8_t *data = chr.getValue(&len);
    if (data && len > 0) {
      _impl->rxBuf.write(reinterpret_cast<const char *>(data), len);
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

/**
 * @brief Start as a NUS client: connect to a remote NUS server, discover the
 *        service, and subscribe to TX notifications.
 * @param address BLE address of the remote NUS server.
 * @param timeoutMs Connection timeout in milliseconds.
 * @return BTStatus::OK on success, BTStatus::InvalidState if already started,
 *         BTStatus::NotFound if the NUS service or characteristics are missing,
 *         or a backend error status on connection failure.
 * @note Blocks until the connection completes or times out. Disconnects and
 *       returns an error if the remote device lacks the required NUS characteristics.
 */
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
    _impl->isConnected = true;
    if (_impl->onConnectCb) {
      _impl->onConnectCb(connInfo);
    }
  });

  _impl->client.onDisconnect([this](BLEClient, const BLEConnInfo &connInfo, uint8_t reason) {
    _impl->isConnected = false;
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

/**
 * @brief Shut down the stream: disconnect (client mode) and reset to idle state.
 */
void BLEStream::end() {
  if (!_impl) {
    return;
  }
  if (_impl->mode == Impl::Mode::Client && _impl->client) {
    _impl->client.disconnect();
  }
  _impl->isConnected = false;
  _impl->mode = Impl::Mode::None;
}

/**
 * @brief Check if a remote device is currently connected.
 * @return True if connected, false otherwise.
 */
bool BLEStream::connected() const {
  return _impl && _impl->isConnected;
}

// --------------------------------------------------------------------------
// Arduino Stream interface
// --------------------------------------------------------------------------

/**
 * @brief Return the number of bytes available to read from the receive buffer.
 * @return Number of bytes available, or 0 if not initialized.
 */
int BLEStream::available() {
  return _impl ? static_cast<int>(_impl->rxBuf.available()) : 0;
}

/**
 * @brief Read a single byte from the receive buffer.
 * @return The next byte (0-255), or -1 if no data is available.
 */
int BLEStream::read() {
  return _impl ? _impl->rxBuf.read() : -1;
}

/**
 * @brief Peek at the next byte without removing it from the buffer.
 * @return The next byte (0-255), or -1 if no data is available.
 */
int BLEStream::peek() {
  return _impl ? _impl->rxBuf.peek() : -1;
}

/**
 * @brief Write a single byte to the remote device.
 * @param c Byte to send.
 * @return 1 on success, 0 on failure or if not connected.
 */
size_t BLEStream::write(uint8_t c) {
  return write(&c, 1);
}

/**
 * @brief Write a buffer of bytes to the remote device, chunked at MTU-3.
 * @param buffer Pointer to the data to send.
 * @param size Number of bytes to send.
 * @return Number of bytes actually sent. Stops on the first failed chunk.
 * @note In server mode, data is sent via TX characteristic notifications.
 *       In client mode, data is written to the remote RX characteristic.
 *       Each chunk is limited to (MTU - 3) bytes to fit the ATT header.
 */
size_t BLEStream::write(const uint8_t *buffer, size_t size) {
  if (!_impl || !_impl->isConnected || size == 0) {
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

/**
 * @brief Flush pending writes (no-op for BLE; notifications are sent immediately).
 */
void BLEStream::flush() {
  // BLE notifications are sent immediately; no buffering to flush
}

/**
 * @brief Check whether the stream has been started (begin or beginClient).
 * @return true if the stream is in server or client mode.
 */
BLEStream::operator bool() const {
  return _impl && _impl->mode != Impl::Mode::None;
}

// --------------------------------------------------------------------------
// Callbacks
// --------------------------------------------------------------------------

/**
 * @brief Register a callback for connection events.
 * @param handler Function to call when a peer connects.
 */
void BLEStream::onConnect(ConnectHandler handler) {
  if (_impl) {
    _impl->onConnectCb = handler;
  }
}

/**
 * @brief Register a callback for disconnection events.
 * @param handler Function to call when the peer disconnects.
 */
void BLEStream::onDisconnect(DisconnectHandler handler) {
  if (_impl) {
    _impl->onDisconnectCb = handler;
  }
}

/**
 * @brief Remove all registered connect/disconnect callbacks.
 */
void BLEStream::resetCallbacks() {
  if (!_impl) {
    return;
  }
  _impl->onConnectCb = nullptr;
  _impl->onDisconnectCb = nullptr;
}

#endif /* BLE_ENABLED */
