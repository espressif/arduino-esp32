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
 * @brief Call Control Profile (CCP): call server (GTBS) and call controller.
 *
 * The server publishes a Generic Telephone Bearer. Calls it raises itself with
 * incomingCall() can be accepted, held and ended from either side. With no
 * extra telephone bearers configured (CONFIG_BT_TBS_BEARER_COUNT = 0) GTBS has
 * nowhere to route an outgoing call, so a controller's originate() is rejected
 * with "invalid URI" and onOriginate() never fires.
 */

#include "core/BLEGuards.h"
#if BLE_AUDIO_SUPPORTED

#include <memory>
#include <functional>
#include "WString.h"
#include "BTStatus.h"

class BLEAudio;

/** Call control point operations reported by callbacks. */
enum class BLEAudioCallOperation : uint8_t {
  Originate = 0,  ///< Place an outgoing call.
  Accept,         ///< Answer an incoming call.
  Terminate,      ///< End a call.
  Hold,           ///< Put a call on local hold.
  Retrieve,       ///< Resume a locally held call.
};

/** Telephone Bearer call states, plus Ended once a call is gone. */
enum class BLEAudioCallState : uint8_t {
  Incoming = 0,
  Dialing = 1,
  Alerting = 2,
  Active = 3,
  LocallyHeld = 4,
  RemotelyHeld = 5,
  LocallyAndRemotelyHeld = 6,
  Ended = 0xFF,
};

/**
 * @brief CCP Call Control Server (the phone side, GTBS).
 *
 * Created by BLEAudio::createCallServer() between audio.begin() and
 * audio.start(). The application drives the calls (incoming call, far end
 * answered or hung up); connected controllers see every state change.
 * Copies of the handle control the same server; an empty handle does
 * nothing, and its BTStatus calls return InvalidState.
 */
class BLEAudioCallServer {
public:
  /**
   * @brief A controller asked to call @p uri; the call already has @p callIndex.
   * @return true to accept the outgoing call request.
   */
  using OriginateCallback = std::function<bool(uint8_t callIndex, const String &uri)>;
  /** @brief @p callIndex ended; @p reason is the TBS termination reason code. */
  using TerminatedCallback = std::function<void(uint8_t callIndex, uint8_t reason)>;
  /** @brief A controller accepted, held or retrieved @p callIndex. */
  using CallControlCallback = std::function<void(BLEAudioCallOperation operation, uint8_t callIndex)>;

  BLEAudioCallServer();
  ~BLEAudioCallServer() = default;
  BLEAudioCallServer(const BLEAudioCallServer &) = default;
  BLEAudioCallServer &operator=(const BLEAudioCallServer &) = default;

  /** @return true when the handle refers to a created server. */
  explicit operator bool() const;

  // --- Configuration (before audio.start()) ---

  /** @brief Default "ESP Phone". */
  BLEAudioCallServer &setProviderName(const String &name);
  /** @brief Uniform Caller Identifier. Default "un000". */
  BLEAudioCallServer &setUci(const String &uci);
  /** @brief Comma-separated URI schemes, e.g. "tel,skype". Default "tel". */
  BLEAudioCallServer &setUriSchemes(const String &schemes);

  // --- Calls (after audio.start()) ---

  /**
   * @brief Raise an incoming call (state Incoming).
   * @param from      Caller URI, e.g. "tel:+15551234567"; also used as the caller name.
   * @param callIndex Receives the index that identifies the call from now on.
   */
  BTStatus incomingCall(const String &from, uint8_t &callIndex);
  /** @brief Answer locally. */
  BTStatus accept(uint8_t callIndex);
  /** @brief Put the call on local hold. */
  BTStatus hold(uint8_t callIndex);
  /** @brief Resume a locally held call. */
  BTStatus retrieve(uint8_t callIndex);
  /** @brief Hang up locally. */
  BTStatus terminate(uint8_t callIndex);
  /** @brief The far end answered an outgoing call. */
  BTStatus remoteAnswered(uint8_t callIndex);
  /** @brief The far end hung up. */
  BTStatus remoteTerminated(uint8_t callIndex);
  /** @brief Change the bearer provider name at runtime; controllers are notified. */
  BTStatus updateProviderName(const String &name);

  /** @brief A controller asked to place a call. Default: accept. */
  BLEAudioCallServer &onOriginate(OriginateCallback cb);
  /** @brief A controller ended a call. */
  BLEAudioCallServer &onTerminated(TerminatedCallback cb);
  /** @brief A controller accepted, held or retrieved a call. */
  BLEAudioCallServer &onCallControl(CallControlCallback cb);
  /** @brief Drop every registered callback. */
  void resetCallbacks();

  struct Impl;

private:
  explicit BLEAudioCallServer(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;

  friend class BLEAudio;
};

/**
 * @brief CCP Call Control Client (the headset side), bound to one peer.
 *
 * Created by BLEAudio::createCallController() between audio.begin() and
 * audio.start(). Operations are control point writes: OK means the write
 * was queued, and the server's answer arrives through onResult; the
 * resulting state changes arrive through onCallState. Operations before
 * discover() return BTStatus::InvalidState.
 */
class BLEAudioCallController {
public:
  /** @brief Discovery result; @p gtbsFound is false when the peer has no GTBS. */
  using DiscoveredCallback = std::function<void(BTStatus status, bool gtbsFound)>;
  /** @brief The server's answer to @p operation on @p callIndex. */
  using ResultCallback = std::function<void(BLEAudioCallOperation operation, BTStatus status, uint8_t callIndex)>;
  /** @brief State of one call. */
  using CallStateCallback = std::function<void(uint8_t callIndex, BLEAudioCallState state)>;

  BLEAudioCallController();
  ~BLEAudioCallController() = default;
  BLEAudioCallController(const BLEAudioCallController &) = default;
  BLEAudioCallController &operator=(const BLEAudioCallController &) = default;

  /** @return true when the handle refers to a created controller. */
  explicit operator bool() const;

  /**
   * @brief Discover the peer's telephone bearers on @p connHandle (after
   *        audio.start()).
   *
   * Waits for the engine's GATT discovery of the link if needed; the current
   * calls are read right after onDiscovered. Every call below acts on the
   * peer's GTBS.
   */
  BTStatus discover(uint16_t connHandle);
  /** @return The connection passed to discover(), or BLE_AUDIO_CONN_NONE. */
  uint16_t getConnHandle() const;

  /** @brief Ask the server to call @p uri (a scheme it lists, e.g. "tel:+15551234567"). */
  BTStatus originate(const String &uri);
  /** @brief Answer an incoming call. */
  BTStatus accept(uint8_t callIndex);
  /** @brief End a call. */
  BTStatus terminate(uint8_t callIndex);
  /** @brief Put a call on local hold. */
  BTStatus hold(uint8_t callIndex);
  /** @brief Resume a locally held call. */
  BTStatus retrieve(uint8_t callIndex);
  /** @brief Re-read the call list (answered via onCallState, one call each). */
  BTStatus readCalls();

  /** @brief Discovery finished; also fires with an error when a deferred discovery could not start. */
  BLEAudioCallController &onDiscovered(DiscoveredCallback cb);
  /** @brief Completion of each call operation. */
  BLEAudioCallController &onResult(ResultCallback cb);
  /** @brief Call state reads and notifications; Ended once a call is gone. */
  BLEAudioCallController &onCallState(CallStateCallback cb);
  /** @brief Drop every registered callback. */
  void resetCallbacks();

  struct Impl;

private:
  explicit BLEAudioCallController(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;

  friend class BLEAudio;
};

#endif /* BLE_AUDIO_SUPPORTED */
