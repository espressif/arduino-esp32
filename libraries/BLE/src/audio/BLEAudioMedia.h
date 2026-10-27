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
 * @brief Media Control Profile (MCP): media player (MCS/GMCS server) and
 *        media controller.
 *
 * The player is the stack's built-in media player exposed through GMCS;
 * remote controllers drive it directly. Local commands and state/result
 * reporting need CONFIG_BT_MCTL_LOCAL_PLAYER_LOCAL_CONTROL, otherwise those
 * calls return NotSupported and the player callbacks never fire.
 */

#include "core/BLEGuards.h"
#if BLE_AUDIO_SUPPORTED

#include <memory>
#include <functional>
#include "WString.h"
#include "BTStatus.h"

class BLEAudio;

/** Media State characteristic values. */
enum class BLEAudioMediaState : uint8_t {
  Inactive = 0,  ///< No current track.
  Playing = 1,
  Paused = 2,
  Seeking = 3,   ///< Fast forward or fast rewind in progress.
};

/** Media Control Point opcodes. */
enum class BLEAudioMediaCommand : uint8_t {
  Play = 0x01,
  Pause = 0x02,
  FastRewind = 0x03,
  FastForward = 0x04,
  Stop = 0x05,
  PreviousTrack = 0x30,
  NextTrack = 0x31,
  FirstTrack = 0x32,
  LastTrack = 0x33,
  PreviousGroup = 0x40,
  NextGroup = 0x41,
};

/**
 * @brief MCP Media Player (GMCS server).
 *
 * Created by BLEAudio::createMediaPlayer() between audio.begin() and
 * audio.start(). Copies of the handle control the same player; an empty
 * handle does nothing, and its BTStatus calls return InvalidState.
 */
class BLEAudioMediaPlayer {
public:
  /** @brief New media state. */
  using StateCallback = std::function<void(BLEAudioMediaState state)>;
  /** @brief Outcome of @p command. */
  using CommandCallback = std::function<void(BLEAudioMediaCommand command, BTStatus result)>;

  BLEAudioMediaPlayer();
  ~BLEAudioMediaPlayer() = default;
  BLEAudioMediaPlayer(const BLEAudioMediaPlayer &) = default;
  BLEAudioMediaPlayer &operator=(const BLEAudioMediaPlayer &) = default;

  /** @return true when the handle refers to a created player. */
  explicit operator bool() const;

  /** @brief Player name; applied at audio.start(), or immediately (and notified) once running. */
  BTStatus setPlayerName(const String &name);
  /** @brief Current track title; same timing as setPlayerName(). */
  BTStatus setTrackTitle(const String &title);

  // --- Local control (after audio.start()) ---

  /** @brief Run @p command on the local player; the outcome arrives through onCommandResult. */
  BTStatus sendCommand(BLEAudioMediaCommand command);
  /** @brief sendCommand(Play). */
  BTStatus play();
  /** @brief sendCommand(Pause). */
  BTStatus pause();
  /** @brief sendCommand(Stop). */
  BTStatus stop();
  /** @brief sendCommand(NextTrack). */
  BTStatus nextTrack();
  /** @brief sendCommand(PreviousTrack). */
  BTStatus previousTrack();
  /** @brief Last state reported by the player. */
  BLEAudioMediaState getState() const;

  /** @brief Player state changes, whoever caused them. */
  BLEAudioMediaPlayer &onStateChanged(StateCallback cb);
  /** @brief Result of every command, local or from a remote controller. */
  BLEAudioMediaPlayer &onCommandResult(CommandCallback cb);
  /** @brief Drop every registered callback. */
  void resetCallbacks();

  struct Impl;

private:
  explicit BLEAudioMediaPlayer(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;

  friend class BLEAudio;
};

/**
 * @brief MCP Media Controller (client of one peer's GMCS).
 *
 * Created by BLEAudio::createMediaController() between audio.begin() and
 * audio.start(). Commands and reads are GATT operations: OK means the
 * request was queued, and the answer arrives through the matching
 * callback. Requests before discover() return BTStatus::InvalidState.
 */
class BLEAudioMediaController {
public:
  /** @brief Discovery (and subscription) result. */
  using DiscoveredCallback = std::function<void(BTStatus status)>;
  /** @brief The peer's media state. */
  using StateCallback = std::function<void(BLEAudioMediaState state)>;
  /** @brief The peer's answer to @p command. */
  using CommandCallback = std::function<void(BLEAudioMediaCommand command, BTStatus result)>;
  /** @brief A text characteristic value (player name or track title). */
  using TextCallback = std::function<void(const String &text)>;

  BLEAudioMediaController();
  ~BLEAudioMediaController() = default;
  BLEAudioMediaController(const BLEAudioMediaController &) = default;
  BLEAudioMediaController &operator=(const BLEAudioMediaController &) = default;

  /** @return true when the handle refers to a created controller. */
  explicit operator bool() const;

  /**
   * @brief Discover the peer's media player on @p connHandle and subscribe to
   *        it (after audio.start()).
   *
   * Waits for the engine's GATT discovery of the link if needed; the media
   * state is read right after onDiscovered. Every call below targets this peer.
   */
  BTStatus discover(uint16_t connHandle);
  /** @return The connection passed to discover(), or BLE_AUDIO_CONN_NONE. */
  uint16_t getConnHandle() const;

  /** @brief Write @p command to the peer's Media Control Point (answered via onCommandResult). */
  BTStatus sendCommand(BLEAudioMediaCommand command);
  /** @brief sendCommand(Play). */
  BTStatus play();
  /** @brief sendCommand(Pause). */
  BTStatus pause();
  /** @brief sendCommand(Stop). */
  BTStatus stop();
  /** @brief sendCommand(NextTrack). */
  BTStatus nextTrack();
  /** @brief sendCommand(PreviousTrack). */
  BTStatus previousTrack();

  /** @brief Read the media state (answered via onStateChanged). */
  BTStatus readState();
  /** @brief Read the player name (answered via onPlayerName). */
  BTStatus readPlayerName();
  /** @brief Read the track title (answered via onTrackTitle). */
  BTStatus readTrackTitle();
  /** @brief Last state reported by the peer. */
  BLEAudioMediaState getState() const;

  /** @brief Discovery finished; also fires with an error when a deferred discovery could not start. */
  BLEAudioMediaController &onDiscovered(DiscoveredCallback cb);
  /** @brief State reads and notifications. */
  BLEAudioMediaController &onStateChanged(StateCallback cb);
  /** @brief Media Control Point results. */
  BLEAudioMediaController &onCommandResult(CommandCallback cb);
  /** @brief Player name reads. */
  BLEAudioMediaController &onPlayerName(TextCallback cb);
  /** @brief Track title reads and notifications. */
  BLEAudioMediaController &onTrackTitle(TextCallback cb);
  /** @brief Drop every registered callback. */
  void resetCallbacks();

  struct Impl;

private:
  explicit BLEAudioMediaController(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;

  friend class BLEAudio;
};

#endif /* BLE_AUDIO_SUPPORTED */
