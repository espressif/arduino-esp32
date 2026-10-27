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

#pragma once

/**
 * @file
 * @brief Isochronous (ISO) channels: raw CIS and BIS transport.
 *
 * Time-bounded, sequence-numbered SDU delivery over the LE isochronous
 * channels, without any audio profile on top: connected isochronous streams
 * (CIS, point to point over an ACL link) and broadcast isochronous streams
 * (BIS, one to many over periodic advertising). Payloads are transparent:
 * the application defines what an SDU carries. For LC3 audio with BAP/CAP
 * interoperability use `BLEAudio` instead; both can run in one sketch.
 *
 * A thin C++ wrapper over the ESP-BLE-ISO engine, shared across NimBLE and
 * Bluedroid (the engine API is host-agnostic). Every `esp_ble_iso_*` call
 * lives in `BLEIso.cpp`; the one NimBLE-specific piece (GAP event forwarding)
 * lives in `BLEIso.nimble.*`, so this header names no stack type.
 *
 * Roles, each guarded by its own `BLE_ISO_*_SUPPORTED` flag (the factory logs
 * and returns nullptr when its role is not compiled in):
 *  - CIS Central     (`connectCis`) -- creates the CIG and connects a CIS on an
 *                                      existing ACL link (from `BLEClient`).
 *                                      `BLE_ISO_CIS_CENTRAL_SUPPORTED`.
 *  - CIS Peripheral  (`listenCis`)  -- registers the ISO server; the CIS is
 *                                      accepted when the Central establishes it.
 *                                      `BLE_ISO_CIS_PERIPHERAL_SUPPORTED`.
 *  - BIS Broadcaster (`createBig`)  -- creates a BIG on a running extended +
 *                                      periodic advertising set (from `BLEAdvertising`).
 *                                      `BLE_ISO_BROADCASTER_SUPPORTED`.
 *  - BIS Receiver    (`syncBig`)    -- arms a BIG sync; it is issued when the
 *                                      BIGInfo report arrives on the periodic
 *                                      train the host is synced to (`BLEScan`).
 *                                      `BLE_ISO_SYNC_RECEIVER_SUPPORTED`.
 *
 * A CIS carries data Central to Peripheral (`CisParams::sduSize`) and,
 * optionally, back (`CisParams::returnSduSize`). A BIS carries data from the
 * Broadcaster to every Receiver.
 *
 * Limits of this transport: one CIS as Central (a single-CIS CIG), one BIG as
 * Broadcaster and one BIG sync as Receiver at a time, each BIG with one BIS.
 * A Peripheral may listen on several channels, one per incoming CIS. Every
 * channel comes from a fixed pool of `CONFIG_BT_ISO_MAX_CHAN` entries.
 *
 * May run alongside the LE Audio engine: when `BLEAudio` owns the host this
 * transport attaches to it instead of initializing ISO a second time.
 *
 * @code
 * BLEIso::begin();
 * BLEIso::CisParams qos;                      // 10 ms, 120-octet SDUs, 2M
 * BLEIso::Channel *ch = BLEIso::connectCis(client.getHandle(), qos);
 * ch->onConnected([](BLEIso::Channel &c) { ... })
 *   .onDisconnected([](BLEIso::Channel &c, uint8_t reason) { ... });
 * ...
 * ch->send(sdu, sizeof(sdu));                 // once per SDU interval
 * @endcode
 */

#include "core/BLEGuards.h"
#if BLE_ISO_SUPPORTED

#include <cstdint>
#include <cstddef>
#include <functional>
#include "WString.h"
#include "BTStatus.h"

namespace BLEIso {

/** PHY selector for ISO QoS. */
enum class Phy : uint8_t {
  Phy1M = 1,
  Phy2M = 2,
  PhyCoded = 3,
};

/** Receive metadata for one transparent SDU. */
struct SduInfo {
  uint32_t timestampUs = 0;    /*!< Controller timestamp (only if timestampValid). */
  uint16_t seqNum = 0;         /*!< SDU sequence number of the first fragment. */
  bool valid = false;          /*!< Payload is complete and error-free. */
  bool timestampValid = false; /*!< The timestamp field is meaningful. */
};

/**
 * @brief One ISO channel (a CIS or a single BIS).
 *
 * Not directly constructed: the factories below return a pointer to a
 * pool-owned instance, valid until `end()`. Calling the same factory again
 * after the channel went down (CIS Central, BIS Broadcaster, BIS Receiver)
 * reuses the same instance and drops its callbacks, so register them again.
 *
 * Callbacks fire from the ISO host task and must not block.
 */
class Channel {
public:
  using ConnectedCb = std::function<void(Channel &)>;
  using DisconnectedCb = std::function<void(Channel &, uint8_t reason)>;
  using ReceiveCb = std::function<void(Channel &, const SduInfo &, const uint8_t *sdu, uint16_t len)>;
  using SentCb = std::function<void(Channel &)>;

  /** @brief Pool slot index (stable for the channel's lifetime), or -1. */
  int slot() const {
    return _slot;
  }

  /** @brief Whether the underlying CIS/BIS is currently established. */
  bool isConnected() const;

  /**
   * @brief Whether this side transmits on the channel.
   *
   * True for a CIS Central with a nonzero `sduSize`, a CIS Peripheral with a
   * nonzero `returnSduSize`, and a BIS Broadcaster.
   */
  bool canSend() const;

  /**
   * @brief Send one transparent SDU with the next sequence number.
   *
   * The channel numbers SDUs itself: the counter restarts at 0 on every
   * connection and advances only when the stack accepts the SDU. Call once
   * per SDU interval.
   * @return OK, InvalidState for an unbound channel, Fail when the stack refused
   *         (not connected, no Tx direction, no buffer).
   */
  BTStatus send(const uint8_t *sdu, uint16_t len);

  /**
   * @brief Send one transparent SDU with a caller-managed sequence number.
   * @param seqNum SDU sequence number; the next send(sdu, len) continues from @p seqNum + 1.
   * @return As send(const uint8_t *, uint16_t).
   */
  BTStatus send(const uint8_t *sdu, uint16_t len, uint16_t seqNum);

  /**
   * @brief Take the channel down.
   *
   * CIS: disconnects it. BIS Broadcaster: terminates the BIG. BIS Receiver:
   * terminates the BIG sync, or disarms a sync that was not issued yet.
   * `onDisconnected` follows once the controller confirms (not for a disarm).
   * @return OK, InvalidState for an unbound channel, Fail when the stack refused.
   */
  BTStatus disconnect();

  /** @brief The CIS/BIS is up and its data path is set up. */
  Channel &onConnected(ConnectedCb cb) {
    _onConnected = std::move(cb);
    return *this;
  }
  /** @brief The CIS/BIS went down (or failed to come up); @p reason is the HCI reason. */
  Channel &onDisconnected(DisconnectedCb cb) {
    _onDisconnected = std::move(cb);
    return *this;
  }
  /** @brief One SDU received; check `SduInfo::valid`, damaged SDUs are delivered too. */
  Channel &onReceive(ReceiveCb cb) {
    _onReceive = std::move(cb);
    return *this;
  }
  /** @brief The controller consumed one SDU passed to send(). */
  Channel &onSent(SentCb cb) {
    _onSent = std::move(cb);
    return *this;
  }
  /** @brief Drop every callback. */
  void resetCallbacks();

private:
  friend struct ChannelAccess;
  int _slot = -1;
  uint16_t _nextSeq = 0;
  ConnectedCb _onConnected;
  DisconnectedCb _onDisconnected;
  ReceiveCb _onReceive;
  SentCb _onSent;
};

/**
 * Connected Isochronous Stream (CIS) QoS.
 *
 * Use the same values on both sides. The Central's QoS defines the CIS; the
 * Peripheral only uses the two SDU sizes to size its directions.
 */
struct CisParams {
  uint32_t sduIntervalUs = 10000;  /*!< SDU interval, microseconds (both directions). */
  uint16_t latencyMs = 10;         /*!< Max transport latency, milliseconds (both directions). */
  uint16_t sduSize = 120;          /*!< Max SDU size Central to Peripheral, octets (0: no data this way). */
  uint16_t returnSduSize = 0;      /*!< Max SDU size Peripheral to Central, octets (0: no data this way). */
  Phy phy = Phy::Phy2M;
  uint8_t rtn = 2;                 /*!< Retransmission number. */
  uint8_t packing = 0;             /*!< 0 sequential, 1 interleaved. */
  uint8_t framing = 0;             /*!< 0 unframed, 1 framed. */
};

/** Broadcast Isochronous Group (BIG) QoS; one BIS per group. */
struct BigParams {
  uint32_t sduIntervalUs = 10000;  /*!< SDU interval, microseconds. */
  uint16_t latencyMs = 10;         /*!< Max transport latency, milliseconds. */
  uint16_t sduSize = 120;          /*!< Max SDU size, octets. */
  Phy phy = Phy::Phy2M;
  uint8_t rtn = 2;                 /*!< Retransmission number. */
  uint8_t packing = 0;             /*!< 0 sequential, 1 interleaved. */
  uint8_t framing = 0;             /*!< 0 unframed, 1 framed. */
  String broadcastCode;            /*!< Up to 16 characters; empty for an unencrypted BIG. */
};

/**
 * @brief Bring the transport up (idempotent). Call after `BLE.begin()`.
 *
 * Initializes ISO on the host, or attaches to it when the LE Audio engine
 * already owns it. Every channel from a previous session is dropped.
 */
BTStatus begin();

/**
 * @brief Terminate every group and release every channel.
 *
 * Also releases the host ISO layer when this transport initialized it.
 * Disconnect every CIS first: the host refuses to deinit with live channels.
 */
void end();

/** @brief Whether begin() succeeded and end() has not run. */
bool isActive();

/**
 * @brief Create the CIG and connect a CIS as Central on an existing ACL link.
 *
 * Only one Central CIS at a time: after it went down, calling this again
 * reconnects the same channel (possibly with new QoS).
 * @param connHandle ACL connection handle (e.g. `BLEClient::getHandle()`).
 * @return The channel, or nullptr on error. `onConnected` fires when the CIS is up.
 */
Channel *connectCis(uint16_t connHandle, const CisParams &params = CisParams());

/**
 * @brief Register the ISO server and accept an inbound CIS as Peripheral.
 *
 * Only `sduSize` and `returnSduSize` are used: the Central chooses the rest
 * of the QoS. The channel keeps listening after its CIS disconnects; call
 * again to accept one more CIS at a time.
 * @return The channel, or nullptr on error. `onConnected` fires on acceptance.
 */
Channel *listenCis(const CisParams &params = CisParams());

/**
 * @brief Create a single-BIS BIG (Broadcaster) on a running extended + periodic advertising set.
 *
 * Only one BIG at a time: after `disconnect()` and `onDisconnected`, calling
 * this again creates a new BIG on the same channel.
 * @param advHandle Advertising instance carrying the periodic train.
 * @return The channel, or nullptr on error. `onConnected` fires when the BIS is up.
 */
Channel *createBig(uint8_t advHandle, const BigParams &params = BigParams());

/**
 * @brief Arm a BIG sync (Receiver) for one BIS index on the synced periodic train.
 *
 * Only `sduSize` and `broadcastCode` are used; the BIGInfo supplies the
 * rest. The code is applied only if the BIGInfo reports an encrypted BIG.
 * While armed, a lost sync is re-issued on the next BIGInfo report. Calling
 * this again while not synced replaces the armed sync.
 * @param bisIndex 1-based BIS index to receive.
 * @return The channel, or nullptr on error. `onConnected` fires when synced.
 */
Channel *syncBig(uint8_t bisIndex, const BigParams &params = BigParams());

}  // namespace BLEIso

#endif /* BLE_ISO_SUPPORTED */
