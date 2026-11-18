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
  Incoming = 0,                ///< Incoming call ringing locally.
  Dialing = 1,                 ///< Outgoing call being dialed.
  Alerting = 2,                ///< Far end is being alerted.
  Active = 3,                  ///< Call is active.
  LocallyHeld = 4,             ///< Call held locally.
  RemotelyHeld = 5,            ///< Call held by the far end.
  LocallyAndRemotelyHeld = 6,  ///< Call held on both ends.
  Ended = 0xFF,                ///< Call is gone; reported once via onCallState.
};

/**
 * @brief CCP Call Control Server (the phone side, GTBS).
 *
 * Created by BLEAudio::createCallServer() while the engine is active and
 * before audio.begin(). The application drives the calls (incoming call, far end
 * answered or hung up); connected controllers see every state change.
 * Copies of the handle control the same server; an empty handle does
 * nothing, and its BTStatus calls return InvalidState.
 */
class BLEAudioCallServer {
public:
  /**
   * @brief A controller asked to place an outgoing call.
   * @param callIndex Index already assigned to the new call.
   * @param uri Requested destination URI.
   * @return true to accept the outgoing call request.
   */
  using OriginateCallback = std::function<bool(uint8_t callIndex, const String &uri)>;
  /**
   * @brief A controller ended a call.
   * @param callIndex Index of the ended call.
   * @param reason TBS termination reason code.
   */
  using TerminatedCallback = std::function<void(uint8_t callIndex, uint8_t reason)>;
  /**
   * @brief A controller accepted, held or retrieved a call.
   * @param operation The requested operation (Accept, Hold or Retrieve).
   * @param callIndex Index of the affected call.
   */
  using CallControlCallback = std::function<void(BLEAudioCallOperation operation, uint8_t callIndex)>;

  BLEAudioCallServer();
  ~BLEAudioCallServer() = default;
  BLEAudioCallServer(const BLEAudioCallServer &) = default;
  BLEAudioCallServer &operator=(const BLEAudioCallServer &) = default;
  BLEAudioCallServer(BLEAudioCallServer &&) = default;
  BLEAudioCallServer &operator=(BLEAudioCallServer &&) = default;

  /** @return true when the handle refers to a created server. */
  explicit operator bool() const;

  // --- Configuration (before audio.begin()) ---

  /**
   * @brief Default "ESP Phone".
   * @return BTStatus::OK when staged; InvalidParam when @p name is empty;
   *         InvalidState when the handle is empty or audio is already started.
   */
  BTStatus setProviderName(const String &name);
  /**
   * @brief Uniform Caller Identifier. Default "un000".
   * @return BTStatus::OK when staged; InvalidParam when @p uci is empty;
   *         InvalidState when the handle is empty or audio is already started.
   */
  BTStatus setUci(const String &uci);
  /**
   * @brief Comma-separated URI schemes, e.g. "tel,skype". Default "tel".
   * @return BTStatus::OK when staged; InvalidParam when @p schemes has no usable scheme;
   *         InvalidState when the handle is empty or audio is already started.
   */
  BTStatus setUriSchemes(const String &schemes);

  // --- Calls (after audio.begin()) ---

  /**
   * @brief Raise an incoming call (state Incoming).
   * @param from Caller URI, e.g. "tel:+15551234567"; also used as the caller name when @p friendlyName is empty.
   * @param callIndex Receives the index that identifies the call from now on.
   * @param friendlyName Shown to the peer instead of the URI when non-empty.
   * @return BTStatus::InvalidState when the handle is empty; otherwise the stack status (OK when the call was raised).
   */
  BTStatus incomingCall(const String &from, uint8_t &callIndex, const String &friendlyName = "");
  /**
   * @brief Answer locally.
   * @param callIndex Index from incomingCall() or onOriginate().
   * @return BTStatus::InvalidState when the handle is empty; otherwise the stack status.
   */
  BTStatus accept(uint8_t callIndex);
  /**
   * @brief Put the call on local hold.
   * @param callIndex Index of the call to hold.
   * @return BTStatus::InvalidState when the handle is empty; otherwise the stack status.
   */
  BTStatus hold(uint8_t callIndex);
  /**
   * @brief Resume a locally held call.
   * @param callIndex Index of the held call.
   * @return BTStatus::InvalidState when the handle is empty; otherwise the stack status.
   */
  BTStatus retrieve(uint8_t callIndex);
  /**
   * @brief Hang up locally.
   * @param callIndex Index of the call to end.
   * @return BTStatus::InvalidState when the handle is empty; otherwise the stack status.
   */
  BTStatus terminate(uint8_t callIndex);
  /**
   * @brief The far end answered an outgoing call.
   * @param callIndex Index of the outgoing call.
   * @return BTStatus::InvalidState when the handle is empty; otherwise the stack status.
   */
  BTStatus remoteAnswered(uint8_t callIndex);
  /**
   * @brief The far end hung up.
   * @param callIndex Index of the ended call.
   * @return BTStatus::InvalidState when the handle is empty; otherwise the stack status.
   */
  BTStatus remoteTerminated(uint8_t callIndex);
  /**
   * @brief Change the bearer provider name at runtime; controllers are notified.
   * @param name New provider name.
   * @return BTStatus::InvalidState when the handle is empty; otherwise the stack status.
   */
  BTStatus updateProviderName(const String &name);

  // --- Callbacks (Bluetooth host task; keep them short) ---

  /** @brief A controller asked to place a call. Default: accept. */
  void onOriginate(OriginateCallback cb);
  /** @brief A controller ended a call. */
  void onTerminated(TerminatedCallback cb);
  /** @brief A controller accepted, held or retrieved a call. */
  void onCallControl(CallControlCallback cb);
  /** @brief Drop every registered callback. */
  void resetCallbacks();

  struct Impl;

private:
  explicit BLEAudioCallServer(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;

  friend class BLEAudio;
  friend struct BLEAudioImplCommon;
};

/**
 * @brief CCP Call Control Client (the headset side), bound to one peer.
 *
 * Created by BLEAudio::createCallController() while the engine is active and
 * before audio.begin(). Operations are control point writes: OK means the write
 * was queued, and the server's answer arrives through onResult; the
 * resulting state changes arrive through onCallState. Operations before
 * discover() return BTStatus::InvalidState.
 */
class BLEAudioCallController {
public:
  /**
   * @brief Discovery result.
   * @param status OK when discovery finished; error status when it failed.
   * @param gtbsFound True when the peer exposes GTBS.
   */
  using DiscoveredCallback = std::function<void(BTStatus status, bool gtbsFound)>;
  /**
   * @brief The server's answer to a call operation.
   * @param operation The operation the answer refers to.
   * @param status OK when the server accepted the operation; error status otherwise.
   * @param callIndex Index of the affected call.
   */
  using ResultCallback = std::function<void(BLEAudioCallOperation operation, BTStatus status, uint8_t callIndex)>;
  /**
   * @brief State of one call.
   * @param callIndex Index of the call.
   * @param state Current call state (Ended once the call is gone).
   */
  using CallStateCallback = std::function<void(uint8_t callIndex, BLEAudioCallState state)>;

  BLEAudioCallController();
  ~BLEAudioCallController() = default;
  BLEAudioCallController(const BLEAudioCallController &) = default;
  BLEAudioCallController &operator=(const BLEAudioCallController &) = default;
  BLEAudioCallController(BLEAudioCallController &&) = default;
  BLEAudioCallController &operator=(BLEAudioCallController &&) = default;

  /** @return true when the handle refers to a created controller. */
  explicit operator bool() const;

  /**
   * @brief Discover the peer's telephone bearers on @p connHandle (after
   *        audio.begin()).
   *
   * Waits for the engine's GATT discovery of the link if needed; the current
   * calls are read right after onDiscovered. Every call below acts on the
   * peer's GTBS.
   * @param connHandle ACL connection handle of the peer.
   * @return BTStatus::InvalidState when the handle is empty, before audio.begin(), or for BLE_AUDIO_CONN_NONE;
   *         OK when discovery started or was deferred until GATT discovery; another status when discovery failed to start.
   */
  BTStatus discover(uint16_t connHandle);
  /** @return The connection passed to discover(), or BLE_AUDIO_CONN_NONE. */
  uint16_t getConnHandle() const;

  /**
   * @brief Ask the server to call @p uri (a scheme it lists, e.g. "tel:+15551234567").
   * @param uri Destination URI to dial.
   * @return BTStatus::InvalidState when the handle is empty or no peer is discovered/active;
   *         OK when the control-point write was queued (answer via onResult); another status when the write failed to start.
   */
  BTStatus originate(const String &uri);
  /**
   * @brief Answer an incoming call.
   * @param callIndex Index reported by onCallState().
   * @return BTStatus::InvalidState when the handle is empty or no peer is discovered/active;
   *         OK when the control-point write was queued (answer via onResult); another status when the write failed to start.
   */
  BTStatus accept(uint8_t callIndex);
  /**
   * @brief End a call.
   * @param callIndex Index reported by onCallState().
   * @return BTStatus::InvalidState when the handle is empty or no peer is discovered/active;
   *         OK when the control-point write was queued (answer via onResult); another status when the write failed to start.
   */
  BTStatus terminate(uint8_t callIndex);
  /**
   * @brief Put a call on local hold.
   * @param callIndex Index reported by onCallState().
   * @return BTStatus::InvalidState when the handle is empty or no peer is discovered/active;
   *         OK when the control-point write was queued (answer via onResult); another status when the write failed to start.
   */
  BTStatus hold(uint8_t callIndex);
  /**
   * @brief Resume a locally held call.
   * @param callIndex Index reported by onCallState().
   * @return BTStatus::InvalidState when the handle is empty or no peer is discovered/active;
   *         OK when the control-point write was queued (answer via onResult); another status when the write failed to start.
   */
  BTStatus retrieve(uint8_t callIndex);
  /**
   * @brief Re-read the call list (answered via onCallState, one call each).
   * @return BTStatus::InvalidState when the handle is empty or no peer is discovered/active;
   *         OK when the read was queued; another status when the read failed to start.
   */
  BTStatus readCalls();

  // --- Callbacks (Bluetooth host task; keep them short) ---

  /** @brief Discovery finished; also fires with an error when a deferred discovery could not start. */
  void onDiscovered(DiscoveredCallback cb);
  /** @brief Completion of each call operation. */
  void onResult(ResultCallback cb);
  /** @brief Call state reads and notifications; Ended once a call is gone. */
  void onCallState(CallStateCallback cb);
  /** @brief Drop every registered callback. */
  void resetCallbacks();

  struct Impl;

private:
  explicit BLEAudioCallController(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;

  friend class BLEAudio;
  friend struct BLEAudioImplCommon;
};

#endif /* BLE_AUDIO_SUPPORTED */
