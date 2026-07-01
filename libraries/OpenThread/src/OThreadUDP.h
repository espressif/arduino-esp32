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

/**
 * @def OT_UDP_MAX_PACKET_SIZE
 * @brief Maximum payload size (bytes) of a single received UDP datagram that
 *        this class will queue. Larger datagrams are truncated. Overridable at
 *        build time.
 */
#ifndef OT_UDP_MAX_PACKET_SIZE
#define OT_UDP_MAX_PACKET_SIZE 512
#endif

/**
 * @def OT_UDP_RX_QUEUE_DEPTH
 * @brief Number of received datagrams that may sit in the RX queue between
 *        parsePacket() calls. Older datagrams are dropped when the queue is
 *        full. Overridable at build time.
 */
#ifndef OT_UDP_RX_QUEUE_DEPTH
#define OT_UDP_RX_QUEUE_DEPTH 4
#endif

/**
 * @brief IPv6 "any" address (`::`), suitable for binding to all interfaces on a
 *        port. Equivalent to `IPAddress(IPv6)`; this alias exists for readability.
 */
extern const IPAddress OT_IN6ADDR_ANY;

/**
 * @brief Arduino `UDP` implementation backed directly by the `otUdpSocket` API.
 *
 * Sends and receives IPv6 UDP datagrams over the Thread mesh without going
 * through lwIP, making it the lightest UDP path on Thread. Usable as a drop-in
 * Arduino `UDP` object.
 */
class OThreadUDP : public UDP {
public:
  OThreadUDP();
  ~OThreadUDP();

  /** @brief true when the underlying otUdpSocket is open. */
  operator bool() const {
    return _open;
  }

  // ---- Arduino UDP interface ------------------------------------------

  /**
   * @brief Open a socket bound to a specific local address and port.
   * @param addr Local IPv6 address to bind to.
   * @param port Local UDP port.
   * @return 1 on success, 0 on failure.
   */
  uint8_t begin(IPAddress addr, uint16_t port);

  /**
   * @brief Open a socket bound to all interfaces (OT_IN6ADDR_ANY) on a port.
   * @param port Local UDP port.
   * @return 1 on success, 0 on failure.
   */
  uint8_t begin(uint16_t port);

  /**
   * @brief Open a socket and subscribe to an IPv6 multicast group.
   * @param group Multicast group to join (e.g. ff03::abcd).
   * @param port  Local UDP port.
   * @return 1 on success, 0 on failure.
   */
  uint8_t beginMulticast(IPAddress group, uint16_t port);

  /** @brief Close the socket. Multicast membership is reference-counted via
   *         OThread.subscribeMulticast() / unsubscribeMulticast(). */
  void stop();

  /**
   * @brief Begin composing an outgoing datagram to an address.
   * @param ip   Destination IPv6 address.
   * @param port Destination UDP port.
   * @return 1 on success, 0 on failure.
   */
  int beginPacket(IPAddress ip, uint16_t port);

  /**
   * @brief Begin composing an outgoing datagram to a textual IPv6 host.
   * @param host Destination IPv6 address in textual form.
   * @param port Destination UDP port.
   * @return 1 on success, 0 on failure.
   */
  int beginPacket(const char *host, uint16_t port);

  /**
   * @brief Send the datagram composed since the last beginPacket().
   * @return 1 on success, 0 on failure.
   */
  int endPacket();

  /**
   * @brief Append one byte to the outgoing datagram.
   * @param b Byte to append.
   * @return Number of bytes written (1 on success).
   */
  size_t write(uint8_t b);

  /**
   * @brief Append a buffer to the outgoing datagram.
   * @param buf  Source bytes.
   * @param size Number of bytes from @p buf.
   * @return Number of bytes written.
   */
  size_t write(const uint8_t *buf, size_t size);

  /**
   * @brief Dequeue the next received datagram for reading.
   * @return Size in bytes of the datagram, or 0 if none is available.
   */
  int parsePacket();

  /** @brief Number of unread bytes in the current received datagram. */
  int available();

  /** @brief Read one byte from the current datagram, or -1 if none. */
  int read();

  /**
   * @brief Read bytes from the current datagram into a buffer.
   * @param buf Destination buffer.
   * @param len Capacity of @p buf.
   * @return Number of bytes read, or -1 if none.
   */
  int read(unsigned char *buf, size_t len);

  /**
   * @brief Read bytes from the current datagram into a char buffer.
   * @param buf Destination buffer.
   * @param len Capacity of @p buf.
   * @return Number of bytes read, or -1 if none.
   */
  int read(char *buf, size_t len);

  /** @brief Peek at the next byte without consuming it, or -1 if none. */
  int peek();

  /** @brief Discard the current received datagram. */
  void flush();

  /** @brief Source IPv6 address of the datagram returned by parsePacket(). */
  IPAddress remoteIP();

  /** @brief Source UDP port of the datagram returned by parsePacket(). */
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

  // RX queue is allocated lazily on begin() (each RxPacket is ~ (OT_UDP_MAX_PACKET_SIZE + 20) ~ 532 bytes,
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
