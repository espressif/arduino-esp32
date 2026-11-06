// Copyright 2026 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "OThreadUDP.h"
#if SOC_IEEE802154_SUPPORTED
#if CONFIG_OPENTHREAD_ENABLED

#include "esp_openthread.h"
#include "esp_openthread_lock.h"
#include <string.h>

const IPAddress OT_IN6ADDR_ANY(IPv6);

namespace {

// RAII OT API lock (mirror of the one in OThread.cpp, kept local to avoid
// cross-TU symbols).
struct OtLock {
  bool held;
  explicit OtLock(TickType_t ticks = portMAX_DELAY) {
    held = esp_openthread_lock_acquire(ticks);
  }
  ~OtLock() {
    if (held) {
      esp_openthread_lock_release();
    }
  }
  operator bool() const {
    return held;
  }
};

inline void ipToOt(const IPAddress &in, otIp6Address &out) {
  // IPAddress stores IPv6 bytes in network order in bytes[0..15].
  // We must use operator[] (public) since the raw buffer is private.
  for (int i = 0; i < 16; i++) {
    out.mFields.m8[i] = in[i];
  }
}

[[maybe_unused]]
inline IPAddress otToIp(const otIp6Address &in) {
  return IPAddress(IPv6, in.mFields.m8);
}

}  // namespace

OThreadUDP::OThreadUDP() : _open(false), _txMessage(nullptr), _txPeerPort(0), _rxQueue(nullptr), _currentOffset(0), _hasCurrent(false), _hasMulticast(false) {
  memset(&_sock, 0, sizeof(_sock));
  memset(&_txPeerAddr, 0, sizeof(_txPeerAddr));
  memset(&_current, 0, sizeof(_current));
  memset(&_multicastGroup, 0, sizeof(_multicastGroup));
}

OThreadUDP::~OThreadUDP() {
  // stop() attempts to close the underlying socket and detach the receive callback.
  // After stop(), the object considers itself closed and it is safe to release
  // local resources like the RX queue.
  stop();
  if (_rxQueue) {
    vQueueDelete(_rxQueue);
    _rxQueue = nullptr;
  }
}

bool OThreadUDP::ensureRxQueue() {
  if (_rxQueue) {
    return true;
  }
  _rxQueue = xQueueCreate(OT_UDP_RX_QUEUE_DEPTH, sizeof(RxPacket));
  if (_rxQueue == nullptr) {
    log_e("OThreadUDP: failed to create RX queue");
    return false;
  }
  return true;
}

void OThreadUDP::udpReceiveCallback(void *aContext, otMessage *aMessage, const otMessageInfo *aMessageInfo) {
  // Called from the OpenThread task with the API lock held. The whole
  // datagram must be drained synchronously: ownership of aMessage is NOT
  // transferred to us (caller will free it), so we just snapshot it.
  OThreadUDP *self = static_cast<OThreadUDP *>(aContext);
  if (self == nullptr || self->_rxQueue == nullptr) {
    log_w("OThreadUDP: receive callback ignored (invalid context or RX queue)");
    return;
  }

  RxPacket pkt;
  uint16_t len = otMessageGetLength(aMessage);
  if (len > OT_UDP_MAX_PACKET_SIZE) {
    log_w("OThreadUDP: datagram truncated from %u to %u bytes", static_cast<unsigned>(len), static_cast<unsigned>(OT_UDP_MAX_PACKET_SIZE));
  }
  uint16_t toCopy = (len > OT_UDP_MAX_PACKET_SIZE) ? OT_UDP_MAX_PACKET_SIZE : len;
  uint16_t got = otMessageRead(aMessage, 0, pkt.data, toCopy);
  pkt.len = got;
  pkt.peerPort = aMessageInfo->mPeerPort;
  memcpy(pkt.peerAddr, aMessageInfo->mPeerAddr.mFields.m8, 16);

  // If the queue is full we drop the oldest entry to make room. This keeps
  // parsePacket() returning fresh data rather than stalling on stale ones.
  if (xQueueSend(self->_rxQueue, &pkt, 0) != pdTRUE) {
    RxPacket discard;
    (void)xQueueReceive(self->_rxQueue, &discard, 0);
    (void)xQueueSend(self->_rxQueue, &pkt, 0);
    log_d("OThreadUDP: RX queue full; dropped oldest datagram (%u bytes)", static_cast<unsigned>(discard.len));
  }
}

uint8_t OThreadUDP::begin(IPAddress addr, uint16_t port) {
  if (addr.type() != IPv6) {
    log_e("OThreadUDP::begin: IPv6 address required");
    return 0;
  }
  if (_open) {
    stop();
  }
  if (!ensureRxQueue()) {
    return 0;
  }

  // Drop any datagrams left from a previous session so the new socket starts clean.
  xQueueReset(_rxQueue);
  _hasCurrent = false;
  _currentOffset = 0;

  otInstance *inst = OThread.getInstance();
  if (!inst) {
    log_w("OThreadUDP::begin: OpenThread not initialized");
    return 0;
  }

  OtLock lock;
  if (!lock) {
    log_w("OThreadUDP::begin: failed to acquire OpenThread lock");
    return 0;
  }
  memset(&_sock, 0, sizeof(_sock));
  otError err = otUdpOpen(inst, &_sock, udpReceiveCallback, this);
  if (err != OT_ERROR_NONE) {
    log_e("otUdpOpen failed: %d", err);
    return 0;
  }

  otSockAddr sa;
  memset(&sa, 0, sizeof(sa));
  ipToOt(addr, sa.mAddress);
  sa.mPort = port;

  err = otUdpBind(inst, &_sock, &sa, OT_NETIF_THREAD_INTERNAL);
  if (err != OT_ERROR_NONE) {
    log_e("otUdpBind failed: %d", err);
    otUdpClose(inst, &_sock);
    return 0;
  }

  _open = true;
  _hasCurrent = false;
  return 1;
}

uint8_t OThreadUDP::begin(uint16_t port) {
  return begin(OT_IN6ADDR_ANY, port);
}

uint8_t OThreadUDP::beginMulticast(IPAddress group, uint16_t port) {
  if (group.type() != IPv6 || group[0] != 0xFF) {
    log_e("OThreadUDP::beginMulticast: IPv6 multicast address required");
    return 0;
  }
  if (!begin(OT_IN6ADDR_ANY, port)) {
    return 0;
  }
  if (!OThread.subscribeMulticast(group)) {
    log_e("OThreadUDP::beginMulticast: subscribeMulticast failed for %s", group.toString().c_str());
    stop();
    return 0;
  }
  ipToOt(group, _multicastGroup);
  _hasMulticast = true;
  return 1;
}

void OThreadUDP::stop() {
  if (_txMessage) {
    for (int attempt = 0; attempt < 3 && _txMessage; ++attempt) {
      OtLock lock;
      if (lock) {
        otMessageFree(_txMessage);
        _txMessage = nullptr;
        break;
      }
      delay(10);
    }
    if (_txMessage) {
      log_e("OThreadUDP::stop: failed to acquire OpenThread lock; pending TX message not freed");
    }
  }
  if (!_open) {
    return;
  }
  if (_hasMulticast) {
    OThread.unsubscribeMulticast(otToIp(_multicastGroup));
    _hasMulticast = false;
  }
  otInstance *inst = OThread.getInstance();
  if (inst) {
    OtLock lock;
    if (lock) {
      if (otUdpIsOpen(inst, &_sock)) {
        otUdpClose(inst, &_sock);
      }
    } else {
      // esp_openthread_lock_acquire() failed with portMAX_DELAY. This only
      // happens when the OT mutex no longer exists (stack shut down), meaning
      // no OT task is running and the receive callback cannot fire. Fall
      // through to clear _open instead of returning early: an early return
      // leaves _open true, which causes the destructor to free _rxQueue while
      // the socket's aContext still points at this object.
      log_e("OThreadUDP::stop: failed to acquire OpenThread lock; socket not cleanly closed");
    }
  }
  _open = false;
  _hasCurrent = false;
}

// ---- TX -------------------------------------------------------------------

int OThreadUDP::beginPacket(IPAddress ip, uint16_t port) {
  if (!_open) {
    log_e("OThreadUDP::beginPacket: socket not open (call begin() first)");
    return 0;
  }
  if (ip.type() != IPv6) {
    log_e("OThreadUDP::beginPacket: IPv6 destination required");
    return 0;
  }
  otInstance *inst = OThread.getInstance();
  if (!inst) {
    log_w("OThreadUDP::beginPacket: OpenThread not initialized");
    return 0;
  }
  // Discard any half-built previous message
  if (_txMessage) {
    for (int attempt = 0; attempt < 3 && _txMessage; ++attempt) {
      OtLock lock;
      if (lock) {
        otMessageFree(_txMessage);
        _txMessage = nullptr;
        break;
      }
      delay(10);
    }
    if (_txMessage) {
      log_e("OThreadUDP::beginPacket: failed to acquire OpenThread lock; pending TX message not freed");
      return 0;
    }
  }

  OtLock lock;
  if (!lock) {
    log_e("OThreadUDP::beginPacket: failed to acquire OpenThread lock");
    return 0;
  }
  _txMessage = otUdpNewMessage(inst, nullptr);
  if (_txMessage == nullptr) {
    log_e("otUdpNewMessage returned NULL (no buffers?)");
    return 0;
  }
  ipToOt(ip, _txPeerAddr);
  _txPeerPort = port;
  return 1;
}

int OThreadUDP::beginPacket(const char *host, uint16_t port) {
  if (host == nullptr) {
    log_e("beginPacket: host is null");
    return 0;
  }
  IPAddress ip;
  if (!ip.fromString(host)) {
    log_e("beginPacket: cannot parse '%s' as IPv6", host);
    return 0;
  }
  return beginPacket(ip, port);
}

size_t OThreadUDP::write(uint8_t b) {
  return write(&b, 1);
}

size_t OThreadUDP::write(const uint8_t *buf, size_t size) {
  if (_txMessage == nullptr) {
    if (size > 0) {
      log_w("OThreadUDP::write: no packet in progress (call beginPacket() first)");
    }
    return 0;
  }
  if (size == 0 || buf == nullptr) {
    return 0;
  }
  if (size > UINT16_MAX) {
    log_e("OThreadUDP::write: payload too large (%u bytes)", (unsigned)size);
    return 0;
  }
  OtLock lock;
  if (!lock) {
    log_w("OThreadUDP::write: failed to acquire OpenThread lock");
    return 0;
  }
  otError err = otMessageAppend(_txMessage, buf, (uint16_t)size);
  if (err != OT_ERROR_NONE) {
    log_e("otMessageAppend failed: %d", err);
    return 0;
  }
  return size;
}

int OThreadUDP::endPacket() {
  if (_txMessage == nullptr) {
    log_w("OThreadUDP::endPacket: no packet in progress (call beginPacket() first)");
    return 0;
  }
  otInstance *inst = OThread.getInstance();
  if (!inst) {
    log_w("OThreadUDP::endPacket: OpenThread not initialized");
    for (int attempt = 0; attempt < 3 && _txMessage; ++attempt) {
      OtLock lock;
      if (lock) {
        otMessageFree(_txMessage);
        _txMessage = nullptr;
        break;
      }
      delay(10);
    }
    if (_txMessage) {
      log_e("OThreadUDP::endPacket: failed to acquire OpenThread lock; pending TX message not freed");
    }
    return 0;
  }

  otMessageInfo info;
  memset(&info, 0, sizeof(info));
  info.mPeerAddr = _txPeerAddr;
  info.mPeerPort = _txPeerPort;

  OtLock lock;
  if (!lock) {
    log_e("OThreadUDP::endPacket: failed to acquire OpenThread lock");
    return 0;
  }
  otError err = otUdpSend(inst, &_sock, _txMessage, &info);
  // On success otUdpSend takes ownership; on failure it does not, so we must
  // free it ourselves to avoid leaking buffers.
  if (err != OT_ERROR_NONE) {
    log_e("otUdpSend failed: %d", err);
    otMessageFree(_txMessage);
    _txMessage = nullptr;
    return 0;
  }
  _txMessage = nullptr;
  return 1;
}

// ---- RX -------------------------------------------------------------------

int OThreadUDP::parsePacket() {
  if (_rxQueue == nullptr) {
    return 0;
  }
  // Drop any partially-consumed previous packet
  _hasCurrent = false;
  _currentOffset = 0;
  if (xQueueReceive(_rxQueue, &_current, 0) != pdTRUE) {
    return 0;
  }
  _hasCurrent = true;
  return _current.len;
}

int OThreadUDP::available() {
  if (!_hasCurrent) {
    return 0;
  }
  return (int)_current.len - (int)_currentOffset;
}

int OThreadUDP::read() {
  if (!_hasCurrent || _currentOffset >= _current.len) {
    return -1;
  }
  return _current.data[_currentOffset++];
}

int OThreadUDP::read(unsigned char *buf, size_t len) {
  if (!_hasCurrent || buf == nullptr) {
    return 0;
  }
  size_t remaining = _current.len - _currentOffset;
  size_t n = (len > remaining) ? remaining : len;
  memcpy(buf, &_current.data[_currentOffset], n);
  _currentOffset += n;
  return (int)n;
}

int OThreadUDP::read(char *buf, size_t len) {
  return read(reinterpret_cast<unsigned char *>(buf), len);
}

int OThreadUDP::peek() {
  if (!_hasCurrent || _currentOffset >= _current.len) {
    return -1;
  }
  return _current.data[_currentOffset];
}

void OThreadUDP::flush() {
  // Discard remaining bytes of the current RX packet, matching the
  // semantics historically used by other ESP32 UDP implementations.
  _hasCurrent = false;
  _currentOffset = 0;
}

IPAddress OThreadUDP::remoteIP() {
  if (!_hasCurrent) {
    return IPAddress(IPv6);
  }
  return IPAddress(IPv6, _current.peerAddr);
}

uint16_t OThreadUDP::remotePort() {
  return _hasCurrent ? _current.peerPort : 0;
}

#endif /* CONFIG_OPENTHREAD_ENABLED */
#endif /* SOC_IEEE802154_SUPPORTED */
