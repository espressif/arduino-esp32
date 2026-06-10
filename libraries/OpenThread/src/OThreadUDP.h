// Copyright 2024 Espressif Systems (Shanghai) PTE LTD
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

#pragma once
#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if SOC_IEEE802154_SUPPORTED
#if CONFIG_OPENTHREAD_ENABLED

#include <Arduino.h>
#include <Udp.h>
#include <IPAddress.h>

#include <openthread/udp.h>
#include <openthread/ip6.h>
#include <openthread/message.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "OThread.h"

// Maximum payload size (bytes) of a single received UDP datagram that this
// class will queue. Datagrams larger than this are truncated. Can be
// overridden at build time.
#ifndef OT_UDP_MAX_PACKET_SIZE
#define OT_UDP_MAX_PACKET_SIZE 512
#endif

// Number of received datagrams that may sit in the RX queue between
// parsePacket() calls. Older datagrams are dropped when the queue is full.
#ifndef OT_UDP_RX_QUEUE_DEPTH
#define OT_UDP_RX_QUEUE_DEPTH 4
#endif

// IPv6 "any" address (::), suitable for binding to all interfaces on a port.
// IPAddress(IPv6) already constructs an all-zero IPv6 address; this alias
// exists for code readability.
extern const IPAddress OT_IN6ADDR_ANY;

class OThreadUDP : public UDP {
public:
  OThreadUDP();
  ~OThreadUDP();

  // True when the underlying otUdpSocket is open.
  operator bool() const {
    return _open;
  }

  // ---- Arduino UDP interface ------------------------------------------
  uint8_t begin(IPAddress addr, uint16_t port);
  uint8_t begin(uint16_t port);
  uint8_t beginMulticast(IPAddress group, uint16_t port);
  void stop();

  int beginPacket(IPAddress ip, uint16_t port);
  int beginPacket(const char *host, uint16_t port);  // IPv6 textual form
  int endPacket();
  size_t write(uint8_t b);
  size_t write(const uint8_t *buf, size_t size);

  int parsePacket();
  int available();
  int read();
  int read(unsigned char *buf, size_t len);
  int read(char *buf, size_t len);
  int peek();
  void flush();

  IPAddress remoteIP();
  uint16_t remotePort();

private:
  // One queued datagram, owned by the RX queue.
  struct RxPacket {
    uint16_t len;
    uint16_t peerPort;
    uint8_t peerAddr[16];
    uint8_t data[OT_UDP_MAX_PACKET_SIZE];
  };

  otUdpSocket _sock;
  bool _open;

  // Outgoing message under construction between beginPacket / endPacket.
  otMessage *_txMessage;
  otIp6Address _txPeerAddr;
  uint16_t _txPeerPort;

  // RX queue is allocated lazily on begin() (each RxPacket is ~520 bytes,
  // so we do not want to pay for it unless the user actually opens a socket).
  QueueHandle_t _rxQueue;

  // Current packet being read via read()/peek()/available().
  RxPacket _current;
  uint16_t _currentOffset;
  bool _hasCurrent;

  // Multicast group joined via beginMulticast(), if any. Used by stop()
  // to unsubscribe symmetrically.
  bool _hasMulticast;
  otIp6Address _multicastGroup;

  static void udpReceiveCallback(void *aContext, otMessage *aMessage, const otMessageInfo *aMessageInfo);
  bool ensureRxQueue();
};

#endif /* CONFIG_OPENTHREAD_ENABLED */
#endif /* SOC_IEEE802154_SUPPORTED */
