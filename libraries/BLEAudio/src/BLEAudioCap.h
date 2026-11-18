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
#include "BLEAudioTypes.h"
#include "BLEAudioStream.h"
#include "BLEAudioUnicastClient.h"
#include "BLEAudioCoordinatedSet.h"

class BLEAudio;

// --------------------------------------------------------------------------
// Acceptor
// --------------------------------------------------------------------------

/**
 * @brief CAP Acceptor: CAS with an included CSIS instance.
 *
 * Created by BLEAudio::createCapAcceptor() while the engine is active and
 * before audio.begin(); the set configuration is registered by start(). The audio
 * itself flows through the BAP roles the sketch creates next to it.
 */
class BLEAudioCapAcceptor {
public:
  BLEAudioCapAcceptor() = default;
  ~BLEAudioCapAcceptor() = default;
  BLEAudioCapAcceptor(const BLEAudioCapAcceptor &) = default;
  BLEAudioCapAcceptor &operator=(const BLEAudioCapAcceptor &) = default;
  BLEAudioCapAcceptor(BLEAudioCapAcceptor &&) = default;
  BLEAudioCapAcceptor &operator=(BLEAudioCapAcceptor &&) = default;

  /** @brief Whether this handle references an acceptor (false when the factory failed). */
  explicit operator bool() const;

  // --- Coordinated set (same rules as BLEAudioCoordinatedSetMember) ---
  //
  // Every setter below returns BTStatus::OK when staged (InvalidState on an
  // empty handle or after audio.begin()); values are read when a session
  // starts. Stage the configuration before audio.begin().

  /**
   * @brief See BLEAudioCoordinatedSetMember::setSirk().
   * @param sirk 16-octet Set Identity Resolving Key shared by the set; null is rejected.
   * @return BTStatus::OK when staged; InvalidState on an empty handle or after audio.begin();
   *         InvalidParam for a null @p sirk.
   */
  BTStatus setSirk(const uint8_t sirk[BLE_AUDIO_SIRK_SIZE]);
  /**
   * @brief See BLEAudioCoordinatedSetMember::setSetSize().
   * @param size Total devices in the set; must be non-zero.
   * @return BTStatus::OK when staged; InvalidState on an empty handle or after audio.begin();
   *         InvalidParam for a zero @p size.
   */
  BTStatus setSetSize(uint8_t size);
  /**
   * @brief See BLEAudioCoordinatedSetMember::setRank().
   * @param rank 1-based rank of this device; zero is rejected.
   * @return BTStatus::OK when staged; InvalidState on an empty handle or after audio.begin();
   *         InvalidParam for a zero @p rank.
   */
  BTStatus setRank(uint8_t rank);
  /**
   * @brief See BLEAudioCoordinatedSetMember::setLockable().
   * @param lockable Whether a coordinator may lock this device for ordered access.
   * @return BTStatus::OK when staged; InvalidState on an empty handle or after audio.begin().
   */
  BTStatus setLockable(bool lockable);
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
 *
 * Like all role handles, the initiator's methods must be called from one
 * task (normally loop()): start/stop/update/handover serialize against each
 * other through the hoBusy guard, which is a check-then-act sequence, not a
 * lock held across the asynchronous engine call. Concurrent calls from two
 * tasks can interleave a stop with an in-flight handover and corrupt the
 * shared unicast parameters; the engine does not second-guess the caller.
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
  BLEAudioCapInitiator(BLEAudioCapInitiator &&) = default;
  BLEAudioCapInitiator &operator=(BLEAudioCapInitiator &&) = default;

  /** @brief Whether this handle references an initiator (false when the factory failed). */
  explicit operator bool() const;

  // --- Configuration (read when a session starts; every setter returns BTStatus) ---

  /**
   * @brief Codec and QoS of every stream, from a BAP preset. Default LC3_16_2_1.
   * @param preset Preset to use; an unknown preset is rejected.
   * @return BTStatus::OK when staged; InvalidState on an empty handle or after audio.begin();
   *         InvalidParam for an unknown @p preset.
   */
  BTStatus setPreset(BLEAudioCodecPreset preset);
  /**
   * @brief Replace the preset QoS; `maxSdu` is recomputed per stream.
   * @param qos Custom QoS applied instead of the preset one from the next session.
   * @return BTStatus::OK when staged; InvalidState on an empty handle or after audio.begin().
   */
  BTStatus setQos(const BLEAudioQos &qos);
  /**
   * @brief Streaming context of unicast and broadcast streams. Default Media.
   * @param context Context sent with Enable and in the BASE metadata.
   * @return BTStatus::OK when staged; InvalidState on an empty handle or after audio.begin().
   */
  BTStatus setContext(BLEAudioContext context);
  /**
   * @brief Unicast streams this device sends, per acceptor. Default 1.
   * @param perPeer Requested sink ASEs per acceptor, capped by what the acceptor has.
   * @return BTStatus::OK when staged; InvalidState on an empty handle or after audio.begin().
   */
  BTStatus setSinkStreams(uint8_t perPeer);
  /**
   * @brief Unicast streams this device receives, per acceptor. Default 0.
   * @param perPeer Requested source ASEs per acceptor, capped by what the acceptor has.
   * @return BTStatus::OK when staged; InvalidState on an empty handle or after audio.begin().
   */
  BTStatus setSourceStreams(uint8_t perPeer);
  /**
   * @brief 24-bit Broadcast ID. Default random.
   * @param broadcastId Only the low 24 bits are kept.
   * @return BTStatus::OK when staged; InvalidState on an empty handle or after audio.begin().
   */
  BTStatus setBroadcastId(uint32_t broadcastId);
  /**
   * @brief The Broadcast ID used by startBroadcast() and handovers.
   * @return The staged ID (masked to 24 bits); 0 on an empty handle.
   */
  uint32_t getBroadcastId() const;
  /**
   * @brief Broadcast Code (up to 16 characters); empty = unencrypted.
   * @param code Longer codes are truncated with a warning and zero-padded per spec.
   * @return BTStatus::OK when staged; InvalidState on an empty handle or after audio.begin().
   */
  BTStatus setBroadcastCode(const String &code);
  /**
   * @brief Broadcast Name; empty = device name.
   * @param name Name advertised in the extended advertising.
   * @return BTStatus::OK when staged; InvalidState on an empty handle or after audio.begin().
   */
  BTStatus setBroadcastName(const String &name);

  // --- Discovery (after audio.begin()) ---

  /**
   * @brief Discover CAS, PACS and ASCS on a connected acceptor.
   *
   * Starts once `BLEAudio::onLinkReady()` fired for @p connHandle; the
   * result is reported by onDiscovered(). Discovered acceptors take part in
   * every following unicast session and handover. Calling it again for a
   * known acceptor, or before its link is GATT-ready, defers to (or reports)
   * the in-flight discovery.
   * @param connHandle ACL connection of the acceptor.
   * @return BTStatus::OK when discovery started, is in flight, or the acceptor
   *         is already discovered; InvalidState before audio.begin(); another
   *         error when the engine refused the discovery.
   */
  BTStatus discover(uint16_t connHandle);

  // --- Unicast ---

  /** @brief Start streams on every discovered acceptor (onUnicastStarted() follows).
   *  @return BTStatus::OK when the setup started; InvalidState when already
   *          running/starting, handed over, a handover is busy, or no discovered
   *          acceptor has a usable endpoint; NoMemory when the stream pool is
   *          exhausted; another error when the engine refused the start. */
  BTStatus startUnicast();
  /** @brief Change the streaming context of the running streams (onUnicastUpdated()).
   *  @param context Context applied to the running session.
   *  @return BTStatus::OK when the update started; InvalidState when no unicast
   *          session is running or a handover is busy; another error otherwise. */
  BTStatus updateUnicast(BLEAudioContext context);
  /** @brief Release every unicast stream (onUnicastStopped() follows).
   *  @return BTStatus::OK when the release started; InvalidState when no session
   *          (running or handed-over) exists or a handover is busy; another error
   *          when the engine refused the stop. */
  BTStatus stopUnicast();
  /** @return true while the unicast streams carry audio (after a successful start or a handover back). */
  bool isUnicastStreaming() const;

  // --- Broadcast ---

  /**
   * @brief Start a broadcast source with @p channels BIS on advertising
   *        instance @p advInstance (onBroadcastStarted() follows).
   */
  BTStatus startBroadcast(uint8_t channels = 1, uint8_t advInstance = 0);
  /** @brief Stop and delete the broadcast source (onBroadcastStopped() follows).
   *
   * Refused with InvalidState while a handover owns the broadcast (hoBusy);
   * wait for its completion instead. Returns Fail (state retained for a
   * retry) when the source is still live after the teardown wait.
   */
  BTStatus stopBroadcast();
  /** @return true while a broadcast (plain or handed over) is streaming. */
  bool isBroadcasting() const;

  // --- Handover (CONFIG_BT_CAP_HANDOVER; NotSupported otherwise) ---

  /**
   * @brief Move the running unicast session to a broadcast on @p advInstance.
   *
   * The acceptors are told to receive the broadcast; the sink streams keep
   * their handles and go on carrying audio as BIS. Source streams stop.
   *
   * @note If the procedure fails, the initiator also stops the unicast
   * session (onUnicastStopped() fires) so startUnicast() can rebuild it.
   * @param advInstance Extended advertising instance for the broadcast.
   * @return BTStatus::OK when the handover started (onHandover() follows);
   *         InvalidState when no unicast session streams, a broadcast already
   *         runs, or a handover is busy; NotSupported when handover is disabled
   *         (CONFIG_BT_CAP_HANDOVER); another error when the engine or the
   *         advertising refused the start.
   */
  BTStatus handoverToBroadcast(uint8_t advInstance = 0);
  /**
   * @brief Move a handed-over broadcast back to unicast on the same acceptors.
   * @return BTStatus::OK when the handover started (onHandover() follows);
   *         InvalidState when no handover is in effect, a handover is busy, or
   *         the acceptors changed since the handover to broadcast; NotSupported
   *         when handover is disabled (CONFIG_BT_CAP_HANDOVER); another error
   *         when the engine refused the start.
   */
  BTStatus handoverToUnicast();

  // --- Streams ---

  /** @brief Streams of the current session: unicast (also after a handover) or broadcast. */
  size_t streamCount() const;
  /**
   * @brief Stream @p index of the current session, or an empty stream.
   * @param index Position in request order (per acceptor: sink streams, then source).
   * @return The stream, or an empty handle when @p index is out of range.
   */
  BLEAudioStream stream(size_t index) const;
  /**
   * @brief The @p index-th stream of direction @p dir in the current session, or an empty stream.
   * @param dir Tx = streams this device sends; Rx = streams it receives.
   * @param index Position among the streams of @p dir.
   * @return The stream, or an empty handle when fewer streams exist in @p dir.
   */
  BLEAudioStream stream(BLEAudioStream::Direction dir, uint8_t index = 0) const;

  // --- Callbacks (Bluetooth host task; keep them short) ---
  //
  // Every setter below takes the callback in @p cb; a no-op on an empty handle.

  /** @brief CAP discovery of an acceptor finished.
   *  @param cb Callback receiving the discovery @p status and the acceptor @p peer. */
  void onDiscovered(DiscoveredCallback cb);
  /** @brief startUnicast() finished; the streams carry audio on success.
   *  @param cb Callback receiving the procedure @p status. */
  void onUnicastStarted(StatusCallback cb);
  /** @brief updateUnicast() finished.
   *  @param cb Callback receiving the procedure @p status. */
  void onUnicastUpdated(StatusCallback cb);
  /** @brief The unicast streams were released.
   *  @param cb Callback fired after the release. */
  void onUnicastStopped(Callback cb);
  /** @brief The broadcast source is streaming.
   *  @param cb Callback fired once the broadcast streams. */
  void onBroadcastStarted(Callback cb);
  /** @brief The broadcast stopped.
   *  @param cb Callback receiving the HCI @p reason (0 after stopBroadcast()). */
  void onBroadcastStopped(StoppedCallback cb);
  /** @brief A handover procedure finished.
   *  @param cb Callback receiving the @p status and the @p toBroadcast direction. */
  void onHandover(HandoverCallback cb);
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
    uint16_t connHandle;  ///< Acceptor that reported it.
    uint8_t sourceId;     ///< Source_ID assigned by the acceptor.
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
  BLEAudioCapCommander(BLEAudioCapCommander &&) = default;
  BLEAudioCapCommander &operator=(BLEAudioCapCommander &&) = default;

  /** @brief Whether this handle references a commander (false when the factory failed). */
  explicit operator bool() const;

  /**
   * @brief Discover CAS and the rendering/capture/BASS services of an acceptor.
   *
   * Starts once `BLEAudio::onLinkReady()` fired for @p connHandle. Every
   * procedure below then acts on all discovered acceptors at once; each
   * reports through onResult(). NotSupported when the matching controller
   * profile (VCP, VOCS, MICP, AICS, BAP assistant) is not in the build.
   * @param connHandle ACL connection of the acceptor; GATT discovery of the
   *        link gates the start (deferred until onLinkReady() when pending).
   * @return BTStatus::OK when discovery started or is deferred to the link's
   *         GATT discovery; InvalidState before audio.begin() or for an invalid
   *         handle; another error when the engine refused the discovery.
   */
  BTStatus discover(uint16_t connHandle);

  /**
   * @brief Run procedures with CSIP ordered access when the acceptors form one set.
   *
   * Off by default (AD_HOC). When on, every procedure uses the stack's CSIP
   * set type — lock, rank-ordered execution and unlock inside the stack — if
   * every driven acceptor belongs to one discovered coordinated set (run
   * CSIP coordinator discovery on each link first); mixed or undiscovered
   * sets fall back to AD_HOC with a warning. Can be toggled any time; takes
   * effect on the next procedure. Returns NotSupported when the CSIP
   * coordinator is compiled out.
   * @param enable True for rank-ordered execution with CSIP lock/unlock, false for AD_HOC.
   * @return BTStatus::OK when queued; InvalidState on an empty handle; NotSupported
   *         when the CSIP coordinator is compiled out; another error when the engine refused.
   */
  BTStatus setCoordinated(bool enable);
  /**
   * @brief Set the absolute volume (0..255) of every acceptor (VCP).
   * @param volume Absolute volume step.
   * @return BTStatus::OK when queued; InvalidState on an empty handle; another
   *         error (including NotSupported with no VCP controller in the build) otherwise.
   */
  BTStatus setVolume(uint8_t volume);
  /**
   * @brief Mute or unmute the rendering of every acceptor (VCP).
   * @param mute True to mute, false to unmute.
   * @return BTStatus::OK when queued; InvalidState on an empty handle; another error otherwise.
   */
  BTStatus setMute(bool mute);
  /**
   * @brief Set the volume offset (-255..255) of every acceptor's first VOCS instance.
   * @param offset Offset step.
   * @return BTStatus::OK when queued; InvalidState on an empty handle; another
   *         error (including NotSupported with no VOCS in the build) otherwise.
   */
  BTStatus setVolumeOffset(int16_t offset);
  /**
   * @brief Mute or unmute the microphone of every acceptor (MICP).
   * @param mute True to mute, false to unmute.
   * @return BTStatus::OK when queued; InvalidState on an empty handle; another error otherwise.
   */
  BTStatus setMicMute(bool mute);
  /**
   * @brief Set the gain of every acceptor's first microphone AICS instance.
   * @param gain Gain step.
   * @return BTStatus::OK when queued; InvalidState on an empty handle; another
   *         error (including NotSupported with no AICS in the build) otherwise.
   */
  BTStatus setMicGain(int8_t gain);

  /**
   * @brief Ask the acceptors to receive a broadcast.
   * @param source      Advertiser address of the broadcast source.
   * @param sid         Advertising SID of the source.
   * @param broadcastId 24-bit Broadcast ID of the source.
   * @param bisSync     BIS indexes to sync (bit n-1 = BIS n); 0 = no preference.
   * @param paInterval  PA interval in 1.25 ms units; 0xFFFF = unknown.
   * @return BTStatus::OK when queued; InvalidState on an empty handle; another
   *         error (including NotSupported with no BASS client in the build) otherwise.
   */
  BTStatus startBroadcastReception(const BTAddress &source, uint8_t sid, uint32_t broadcastId, uint32_t bisSync = 0, uint16_t paInterval = 0xFFFF);
  /**
   * @brief Ask the acceptors to stop receiving the broadcast started above.
   * @return BTStatus::OK when queued; InvalidState on an empty handle; another error otherwise.
   */
  BTStatus stopBroadcastReception();
  /**
   * @brief Send the Broadcast Code of an encrypted broadcast to the acceptors.
   * @param code Non-empty code (up to 16 characters); an empty code is rejected.
   * @return BTStatus::OK when queued; InvalidParam for an empty code; InvalidState
   *         on an empty handle; another error otherwise.
   */
  BTStatus distributeBroadcastCode(const String &code);

  // --- Callbacks (Bluetooth host task; keep them short) ---
  //
  // Every setter below takes the callback in @p cb; a no-op on an empty handle.

  /** @brief Commander discovery of an acceptor finished.
   *  @param cb Callback receiving the @p status, the acceptor @p connHandle, and
   *            whether it is @p coordinatedSet / @p broadcastReception capable. */
  void onDiscovered(DiscoveredCallback cb);
  /** @brief A procedure finished.
   *  @param cb Callback receiving the finished @p op and its @p status. */
  void onResult(ResultCallback cb);
  /** @brief Acceptor receive states.
   *  @param cb Callback receiving each read/notified receive @p state. */
  void onReceiveState(ReceiveStateCallback cb);
  /** @brief Clear every callback of this role. */
  void resetCallbacks();

  struct Impl;

private:
  explicit BLEAudioCapCommander(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;

  friend class BLEAudio;
};

#endif /* BLE_AUDIO_SUPPORTED */
