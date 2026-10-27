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
 * @brief Common Audio Profile (CAP) roles.
 *
 * CAP ties the LE Audio profiles together so that a set of devices (two
 * earbuds) is handled as one:
 *
 *  - `BLEAudioCapAcceptor` (earbud, speaker): publishes CAS with an included
 *    CSIS instance, next to the BAP/VCP/MICP servers the sketch creates.
 *  - `BLEAudioCapInitiator` (phone, PC): starts, updates and stops unicast
 *    streams on every discovered acceptor at once, runs a broadcast source,
 *    and hands an active unicast session over to broadcast and back.
 *  - `BLEAudioCapCommander` (remote control): changes volume, mute and
 *    microphone settings on every discovered acceptor and tells them which
 *    broadcast to receive.
 *
 * @code
 * audio.onLinkReady([](uint16_t conn) { initiator.discover(conn); });
 * initiator.onDiscovered([](BTStatus st, const BLEAudioCapPeerInfo &peer) {
 *   if (st) initiator.startUnicast();
 * });
 * // loop(): initiator.stream(BLEAudioStream::Direction::Tx).write(sdu, len);
 * @endcode
 */

#include "core/BLEGuards.h"
#if BLE_AUDIO_SUPPORTED

#include <memory>
#include <functional>
#include "WString.h"
#include "BTStatus.h"
#include "BTAddress.h"
#include "audio/BLEAudioTypes.h"
#include "audio/BLEAudioStream.h"
#include "audio/BLEAudioUnicastClient.h"
#include "audio/BLEAudioCoordinatedSet.h"

class BLEAudio;

// --------------------------------------------------------------------------
// Acceptor
// --------------------------------------------------------------------------

/**
 * @brief CAP Acceptor: CAS with an included CSIS instance.
 *
 * Created by BLEAudio::createCapAcceptor() between audio.begin() and
 * audio.start(); the set configuration is registered by start(). The audio
 * itself flows through the BAP roles the sketch creates next to it.
 */
class BLEAudioCapAcceptor {
public:
  BLEAudioCapAcceptor() = default;
  ~BLEAudioCapAcceptor() = default;
  BLEAudioCapAcceptor(const BLEAudioCapAcceptor &) = default;
  BLEAudioCapAcceptor &operator=(const BLEAudioCapAcceptor &) = default;

  /** @brief Whether this handle references an acceptor (false when the factory failed). */
  explicit operator bool() const;

  // --- Coordinated set (same rules as BLEAudioCoordinatedSetMember) ---

  /** @brief See BLEAudioCoordinatedSetMember::setSirk(). */
  BLEAudioCapAcceptor &setSirk(const uint8_t sirk[BLE_AUDIO_SIRK_SIZE]);
  /** @brief See BLEAudioCoordinatedSetMember::setSetSize(). */
  BLEAudioCapAcceptor &setSetSize(uint8_t size);
  /** @brief See BLEAudioCoordinatedSetMember::setRank(). */
  BLEAudioCapAcceptor &setRank(uint8_t rank);
  /** @brief See BLEAudioCoordinatedSetMember::setLockable(). */
  BLEAudioCapAcceptor &setLockable(bool lockable);
  /** @brief The CAS-included CSIS instance: RSI, lock state and callbacks. */
  BLEAudioCoordinatedSetMember coordinatedSet() const;

private:
  explicit BLEAudioCapAcceptor(std::shared_ptr<BLEAudioCoordinatedSetMember::Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<BLEAudioCoordinatedSetMember::Impl> _impl;

  friend class BLEAudio;
};

// --------------------------------------------------------------------------
// Initiator
// --------------------------------------------------------------------------

/** @brief A CAP acceptor as discovered by the initiator (CAS + PACS + ASCS). */
struct BLEAudioCapPeerInfo : BLEAudioUnicastPeerInfo {
  bool coordinatedSet = false;  ///< CAS includes a CSIS instance (one device of a set).
};

/**
 * @brief CAP Initiator: drives every discovered acceptor as one unit.
 *
 * Created by BLEAudio::createCapInitiator(). Procedures return once
 * started; completion is reported through the matching callback.
 */
class BLEAudioCapInitiator {
public:
  /** @brief Discovery result for one acceptor. */
  using DiscoveredCallback = std::function<void(BTStatus status, const BLEAudioCapPeerInfo &peer)>;
  /** @brief Completion of a procedure. */
  using StatusCallback = std::function<void(BTStatus status)>;
  /** @brief Event without payload. */
  using Callback = std::function<void()>;
  /** @brief The broadcast stopped; @p reason is the HCI reason (0 after stopBroadcast()). */
  using StoppedCallback = std::function<void(uint8_t reason)>;
  /** @brief A handover finished; @p toBroadcast tells the direction. */
  using HandoverCallback = std::function<void(BTStatus status, bool toBroadcast)>;

  BLEAudioCapInitiator() = default;
  ~BLEAudioCapInitiator() = default;
  BLEAudioCapInitiator(const BLEAudioCapInitiator &) = default;
  BLEAudioCapInitiator &operator=(const BLEAudioCapInitiator &) = default;

  /** @brief Whether this handle references an initiator (false when the factory failed). */
  explicit operator bool() const;

  // --- Configuration (read when a session starts) ---

  /** @brief Codec and QoS of every stream, from a BAP preset. Default LC3_16_2_1. */
  BLEAudioCapInitiator &setPreset(BLEAudioCodecPreset preset);
  /** @brief Replace the preset QoS; `maxSdu` is recomputed per stream. */
  BLEAudioCapInitiator &setQos(const BLEAudioQos &qos);
  /** @brief Streaming context of unicast and broadcast streams. Default Media. */
  BLEAudioCapInitiator &setContext(BLEAudioContext context);
  /** @brief Unicast streams this device sends, per acceptor. Default 1. */
  BLEAudioCapInitiator &setSinkStreams(uint8_t perPeer);
  /** @brief Unicast streams this device receives, per acceptor. Default 0. */
  BLEAudioCapInitiator &setSourceStreams(uint8_t perPeer);
  /** @brief 24-bit Broadcast ID. Default random. */
  BLEAudioCapInitiator &setBroadcastId(uint32_t broadcastId);
  /** @return The Broadcast ID used by startBroadcast() and handovers. */
  uint32_t getBroadcastId() const;
  /** @brief Broadcast Code (up to 16 characters); empty = unencrypted. */
  BLEAudioCapInitiator &setBroadcastCode(const String &code);
  /** @brief Broadcast Name; empty = device name. */
  BLEAudioCapInitiator &setBroadcastName(const String &name);

  // --- Discovery (after audio.start()) ---

  /**
   * @brief Discover CAS, PACS and ASCS on a connected acceptor.
   *
   * Starts once `BLEAudio::onLinkReady()` fired for @p connHandle; the
   * result is reported by onDiscovered(). Discovered acceptors take part in
   * every following unicast session and handover.
   */
  BTStatus discover(uint16_t connHandle);

  // --- Unicast ---

  /** @brief Start streams on every discovered acceptor (onUnicastStarted() follows). */
  BTStatus startUnicast();
  /** @brief Change the streaming context of the running streams (onUnicastUpdated()). */
  BTStatus updateUnicast(BLEAudioContext context);
  /** @brief Release every unicast stream (onUnicastStopped() follows). */
  BTStatus stopUnicast();
  /** @return true while the unicast streams carry audio (after a successful start or a handover back). */
  bool isUnicastStreaming() const;

  // --- Broadcast ---

  /**
   * @brief Start a broadcast source with @p channels BIS on advertising
   *        instance @p advInstance (onBroadcastStarted() follows).
   */
  BTStatus startBroadcast(uint8_t channels = 1, uint8_t advInstance = 1);
  /** @brief Stop and delete the broadcast source (onBroadcastStopped() follows). */
  BTStatus stopBroadcast();
  /** @return true while a broadcast (plain or handed over) is streaming. */
  bool isBroadcasting() const;

  // --- Handover (CONFIG_BT_CAP_HANDOVER; NotSupported otherwise) ---

  /**
   * @brief Move the running unicast session to a broadcast on @p advInstance.
   *
   * The acceptors are told to receive the broadcast; the sink streams keep
   * their handles and go on carrying audio as BIS. Source streams stop.
   */
  BTStatus handoverToBroadcast(uint8_t advInstance = 1);
  /** @brief Move a handed-over broadcast back to unicast on the same acceptors. */
  BTStatus handoverToUnicast();

  // --- Streams ---

  /** @brief Streams of the current session: unicast (also after a handover) or broadcast. */
  size_t streamCount() const;
  /** @return Stream @p index of the current session, or an empty stream. */
  BLEAudioStream stream(size_t index) const;
  /** @return The @p index-th stream of direction @p dir in the current session, or an empty stream. */
  BLEAudioStream stream(BLEAudioStream::Direction dir, uint8_t index = 0) const;

  // --- Callbacks (Bluetooth host task; keep them short) ---

  /** @brief CAP discovery of an acceptor finished. */
  BLEAudioCapInitiator &onDiscovered(DiscoveredCallback cb);
  /** @brief startUnicast() finished; the streams carry audio on success. */
  BLEAudioCapInitiator &onUnicastStarted(StatusCallback cb);
  /** @brief updateUnicast() finished. */
  BLEAudioCapInitiator &onUnicastUpdated(StatusCallback cb);
  /** @brief The unicast streams were released. */
  BLEAudioCapInitiator &onUnicastStopped(Callback cb);
  /** @brief The broadcast source is streaming. */
  BLEAudioCapInitiator &onBroadcastStarted(Callback cb);
  /** @brief The broadcast stopped. */
  BLEAudioCapInitiator &onBroadcastStopped(StoppedCallback cb);
  /** @brief A handover procedure finished. */
  BLEAudioCapInitiator &onHandover(HandoverCallback cb);
  /** @brief Clear every callback of this role. */
  void resetCallbacks();

  struct Impl;

private:
  explicit BLEAudioCapInitiator(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;

  friend class BLEAudio;
};

// --------------------------------------------------------------------------
// Commander
// --------------------------------------------------------------------------

/**
 * @brief CAP Commander: rendering, capture and broadcast reception control of
 *        every discovered acceptor at once.
 *
 * Created by BLEAudio::createCapCommander(). Each procedure returns once
 * started and reports through onResult().
 */
class BLEAudioCapCommander {
public:
  /** @brief Procedure reported by onResult(). */
  enum class Operation : uint8_t {
    Volume = 0,               ///< setVolume()
    VolumeMute,               ///< setMute()
    VolumeOffset,             ///< setVolumeOffset()
    MicMute,                  ///< setMicMute()
    MicGain,                  ///< setMicGain()
    BroadcastReceptionStart,  ///< startBroadcastReception()
    BroadcastReceptionStop,   ///< stopBroadcastReception()
    BroadcastCode,            ///< distributeBroadcastCode()
  };

  /** @brief Broadcast receive state reported by an acceptor (BASS). */
  struct ReceiveState {
    uint16_t connHandle;      ///< Acceptor that reported it.
    uint8_t sourceId;         ///< Source_ID assigned by the acceptor.
    uint32_t broadcastId;
    uint8_t paSyncState;      ///< 0 not synced, 1 SyncInfo request, 2 synced, 3 failed, 4 no PAST.
    uint8_t encryptionState;  ///< 0 unencrypted, 1 code requested, 2 decrypting, 3 bad code.
    uint32_t bisSync;         ///< BIS indexes synced in any subgroup (bit n-1 = BIS n).
  };

  /** @brief @p coordinatedSet: part of a set; @p broadcastReception: BASS available. */
  using DiscoveredCallback = std::function<void(BTStatus status, uint16_t connHandle, bool coordinatedSet, bool broadcastReception)>;
  /** @brief Completion of procedure @p op over all acceptors. */
  using ResultCallback = std::function<void(Operation op, BTStatus status)>;
  /** @brief An acceptor's receive state was read or notified. */
  using ReceiveStateCallback = std::function<void(const ReceiveState &state)>;

  BLEAudioCapCommander() = default;
  ~BLEAudioCapCommander() = default;
  BLEAudioCapCommander(const BLEAudioCapCommander &) = default;
  BLEAudioCapCommander &operator=(const BLEAudioCapCommander &) = default;

  /** @brief Whether this handle references a commander (false when the factory failed). */
  explicit operator bool() const;

  /**
   * @brief Discover CAS and the rendering/capture/BASS services of an acceptor.
   *
   * Starts once `BLEAudio::onLinkReady()` fired for @p connHandle. Every
   * procedure below then acts on all discovered acceptors at once; each
   * reports through onResult(). NotSupported when the matching controller
   * profile (VCP, VOCS, MICP, AICS, BAP assistant) is not in the build.
   */
  BTStatus discover(uint16_t connHandle);

  /** @brief Set the absolute volume (0..255) of every acceptor (VCP). */
  BTStatus setVolume(uint8_t volume);
  /** @brief Mute or unmute the rendering of every acceptor (VCP). */
  BTStatus setMute(bool mute);
  /** @brief Set the volume offset (-255..255) of every acceptor's first VOCS instance. */
  BTStatus setVolumeOffset(int16_t offset);
  /** @brief Mute or unmute the microphone of every acceptor (MICP). */
  BTStatus setMicMute(bool mute);
  /** @brief Set the gain of every acceptor's first microphone AICS instance. */
  BTStatus setMicGain(int8_t gain);

  /**
   * @brief Ask the acceptors to receive a broadcast.
   * @param source      Advertiser address of the broadcast source.
   * @param sid         Advertising SID of the source.
   * @param broadcastId 24-bit Broadcast ID of the source.
   * @param bisSync     BIS indexes to sync (bit n-1 = BIS n); 0 = no preference.
   * @param paInterval  PA interval in 1.25 ms units; 0xFFFF = unknown.
   */
  BTStatus startBroadcastReception(const BTAddress &source, uint8_t sid, uint32_t broadcastId, uint32_t bisSync = 0,
                                   uint16_t paInterval = 0xFFFF);
  /** @brief Ask the acceptors to stop receiving the broadcast started above. */
  BTStatus stopBroadcastReception();
  /** @brief Send the Broadcast Code of an encrypted broadcast to the acceptors. */
  BTStatus distributeBroadcastCode(const String &code);

  // --- Callbacks (Bluetooth host task; keep them short) ---

  /** @brief Commander discovery of an acceptor finished. */
  BLEAudioCapCommander &onDiscovered(DiscoveredCallback cb);
  /** @brief A procedure finished. */
  BLEAudioCapCommander &onResult(ResultCallback cb);
  /** @brief Acceptor receive states. */
  BLEAudioCapCommander &onReceiveState(ReceiveStateCallback cb);
  /** @brief Clear every callback of this role. */
  void resetCallbacks();

  struct Impl;

private:
  explicit BLEAudioCapCommander(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;

  friend class BLEAudio;
};

#endif /* BLE_AUDIO_SUPPORTED */
