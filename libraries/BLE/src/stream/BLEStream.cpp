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

#include "core/BLEGuards.h"
#if BLE_ENABLED

#include <Arduino.h>
#include "stream/BLEStream.h"
#include "stream/BLEStreamImpl.h"

// Nordic UART Service UUIDs (6e40000x-b5a3-f393-e0a9-e50e24dcca9e)
static constexpr BLEUUID kNUS_ServiceUUID = BLEUUID::from128(0x6e, 0x40, 0x00, 0x01, 0xb5, 0xa3, 0xf3, 0x93, 0xe0, 0xa9, 0xe5, 0x0e, 0x24, 0xdc, 0xca, 0x9e);
static constexpr BLEUUID kNUS_RxCharUUID = BLEUUID::from128(0x6e, 0x40, 0x00, 0x02, 0xb5, 0xa3, 0xf3, 0x93, 0xe0, 0xa9, 0xe5, 0x0e, 0x24, 0xdc, 0xca, 0x9e);
static constexpr BLEUUID kNUS_TxCharUUID = BLEUUID::from128(0x6e, 0x40, 0x00, 0x03, 0xb5, 0xa3, 0xf3, 0x93, 0xe0, 0xa9, 0xe5, 0x0e, 0x24, 0xdc, 0xca, 0x9e);

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
// BLEStream lifecycle
// --------------------------------------------------------------------------

BLEStream::BLEStream(size_t rxBufferSize) : _impl(new Impl(rxBufferSize)) {}

BLEStream::~BLEStream() {
  // end() already clears the callbacks this stream registered while it was
  // live (server: rxChr + server handlers, client: client handlers). Do not
  // clear them unconditionally here: after end() the server/client handles
  // may be shared with (or have been re-registered by) another owner, and
  // wiping their callbacks would break that owner.
  end();
  delete _impl;
}

void BLEStream::end() {
  if (!_impl) {
    return;
  }
  // Keep the role alive locally while callbacks run. A disconnect callback may
  // reenter end(); the nested call must observe no active role without deleting
  // the role whose end() method is still executing.
  auto role = _impl->takeRole();
  if (role) {
    role->end();
  }
  _impl->peerCount = 0;
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
  auto role = _impl->loadRole();
  return (role && !role->ending) ? role->peers() : std::vector<BLEConnInfo>{};
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
  if (!_impl || _impl->peerCount <= 0 || !buffer || size == 0) {
    return 0;
  }
  auto role = _impl->loadRole();
  return (role && !role->ending) ? role->write(buffer, size) : 0;
}

size_t BLEStream::writeTo(const BLEConnInfo &peer, const uint8_t *buffer, size_t size) {
  if (!_impl || _impl->peerCount <= 0 || !buffer || size == 0) {
    return 0;
  }
  auto role = _impl->loadRole();
  return (role && !role->ending) ? role->writeTo(peer, buffer, size) : 0;
}

void BLEStream::flush() {
  // BLE notifications are sent immediately; no buffering to flush
}

BLEStream::operator bool() const {
  return _impl && _impl->loadRole() != nullptr;
}

// --------------------------------------------------------------------------
// Callbacks
// --------------------------------------------------------------------------

void BLEStream::onConnect(ConnectHandler handler) {
  if (_impl) {
    _impl->onConnectCb = std::move(handler);
  }
}

void BLEStream::onDisconnect(DisconnectHandler handler) {
  if (_impl) {
    _impl->onDisconnectCb = std::move(handler);
  }
}

void BLEStream::onPeerData(PeerDataHandler handler) {
  if (_impl) {
    _impl->onPeerDataCb = std::move(handler);
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
