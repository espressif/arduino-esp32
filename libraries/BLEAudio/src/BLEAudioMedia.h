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
  Playing = 1,   ///< Playback in progress.
  Paused = 2,    ///< Playback paused.
  Seeking = 3,   ///< Fast forward or fast rewind in progress.
};

/** Media Control Point opcodes. */
enum class BLEAudioMediaCommand : uint8_t {
  Play = 0x01,           ///< Start playback.
  Pause = 0x02,          ///< Pause playback.
  FastRewind = 0x03,     ///< Fast rewind.
  FastForward = 0x04,    ///< Fast forward.
  Stop = 0x05,           ///< Stop playback.
  PreviousTrack = 0x30,  ///< Skip to the previous track.
  NextTrack = 0x31,      ///< Skip to the next track.
  FirstTrack = 0x32,     ///< Skip to the first track.
  LastTrack = 0x33,      ///< Skip to the last track.
  PreviousGroup = 0x40,  ///< Skip to the previous group.
  NextGroup = 0x41,      ///< Skip to the next group.
};

/**
 * @brief MCP Media Player (GMCS server).
 *
 * Created by BLEAudio::createMediaPlayer() while the engine is active and
 * before audio.begin(). Copies of the handle control the same player; an empty
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
  BLEAudioMediaPlayer(BLEAudioMediaPlayer &&) = default;
  BLEAudioMediaPlayer &operator=(BLEAudioMediaPlayer &&) = default;

  /** @return true when the handle refers to a created player. */
  explicit operator bool() const;

  /**
   * @brief Player name; applied at audio.begin(), or immediately (and notified) once running.
   * @param name New player name.
   * @return BTStatus::OK when the name was staged (before start) or applied (once running);
   *         InvalidState when the handle is empty;
   *         another error when the stack refused the new text (the previous text is kept).
   */
  BTStatus setPlayerName(const String &name);
  /**
   * @brief Current track title; same timing as setPlayerName().
   * @param title New track title.
   * @return BTStatus::OK when the title was staged (before start) or applied (once running);
   *         InvalidState when the handle is empty;
   *         another error when the stack refused the new text (the previous text is kept).
   */
  BTStatus setTrackTitle(const String &title);

  // --- Local control (after audio.begin()) ---
  // Needs CONFIG_BT_MCTL_LOCAL_PLAYER_LOCAL_CONTROL in the stack; without it
  // the calls below return NotSupported (state reporting is unavailable too).

  /**
   * @brief Run @p command on the local player; the outcome arrives through onCommandResult.
   * @param command Media Control Point opcode to run.
   * @return BTStatus::OK when the command was sent (the outcome arrives via onCommandResult);
   *         InvalidState when the handle is empty or the player is not running yet;
   *         NotSupported without CONFIG_BT_MCTL_LOCAL_PLAYER_LOCAL_CONTROL;
   *         another error when the stack refused the command.
   */
  BTStatus sendCommand(BLEAudioMediaCommand command);
  /** @brief sendCommand(Play). @return As sendCommand(). */
  BTStatus play();
  /** @brief sendCommand(Pause). @return As sendCommand(). */
  BTStatus pause();
  /** @brief sendCommand(Stop). @return As sendCommand(). */
  BTStatus stop();
  /** @brief sendCommand(NextTrack). @return As sendCommand(). */
  BTStatus nextTrack();
  /** @brief sendCommand(PreviousTrack). @return As sendCommand(). */
  BTStatus previousTrack();
  /** @brief Last state reported by the player. */
  BLEAudioMediaState getState() const;

  // --- Callbacks (Bluetooth host task; keep them short) ---

  /** @brief Player state changes, whoever caused them. */
  void onStateChanged(StateCallback cb);
  /** @brief Result of every command, local or from a remote controller. */
  void onCommandResult(CommandCallback cb);
  /** @brief Drop every registered callback. */
  void resetCallbacks();

  struct Impl;

private:
  explicit BLEAudioMediaPlayer(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;

  friend class BLEAudio;
  friend struct BLEAudioImplCommon;
};

/**
 * @brief MCP Media Controller (client of one peer's GMCS).
 *
 * Created by BLEAudio::createMediaController() while the engine is active and
 * before audio.begin(). Commands and reads are GATT operations: OK means the
 * request was queued, and the answer arrives through the matching
 * callback. Requests before discover() return BTStatus::InvalidState.
 *
 * Stack Kconfig matrix (a call returns NotSupported when its option is off):
 * sendCommand()/play()/pause()/stop()/nextTrack()/previousTrack() need
 * CONFIG_BT_MCC_SET_MEDIA_CONTROL_POINT; readState() needs
 * CONFIG_BT_MCC_READ_MEDIA_STATE; readTrackTitle() needs
 * CONFIG_BT_MCC_READ_TRACK_TITLE; readPlayerName() is always available.
 * Only the GMCS instance is driven; discrete MCS and OTS are not exposed.
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
  BLEAudioMediaController(BLEAudioMediaController &&) = default;
  BLEAudioMediaController &operator=(BLEAudioMediaController &&) = default;

  /** @return true when the handle refers to a created controller. */
  explicit operator bool() const;

  /**
   * @brief Discover the peer's media player on @p connHandle and subscribe to
   *        it (after audio.begin()).
   *
   * Waits for the engine's GATT discovery of the link if needed; the media
   * state is read right after onDiscovered. Every call below targets this peer.
   * @param connHandle ACL connection of the peer's media player.
   * @return BTStatus::OK when discovery started (or was deferred until the engine's GATT discovery finishes);
   *         InvalidState when the handle is empty, audio is not started, or @p connHandle is BLE_AUDIO_CONN_NONE;
   *         another error when the stack refused discovery.
   */
  BTStatus discover(uint16_t connHandle);
  /** @return The connection passed to discover(), or BLE_AUDIO_CONN_NONE. */
  uint16_t getConnHandle() const;

  /**
   * @brief Write @p command to the peer's Media Control Point (answered via onCommandResult).
   * @param command Media Control Point opcode to write.
   * @return BTStatus::OK when the write was queued (the answer arrives via onCommandResult);
   *         InvalidState when the handle is empty or no peer is discovered (also while discovery is still deferred);
   *         NotSupported without CONFIG_BT_MCC_SET_MEDIA_CONTROL_POINT;
   *         another error when the stack refused the write.
   */
  BTStatus sendCommand(BLEAudioMediaCommand command);
  /** @brief sendCommand(Play). @return As sendCommand(). */
  BTStatus play();
  /** @brief sendCommand(Pause). @return As sendCommand(). */
  BTStatus pause();
  /** @brief sendCommand(Stop). @return As sendCommand(). */
  BTStatus stop();
  /** @brief sendCommand(NextTrack). @return As sendCommand(). */
  BTStatus nextTrack();
  /** @brief sendCommand(PreviousTrack). @return As sendCommand(). */
  BTStatus previousTrack();

  /**
   * @brief Read the media state (answered via onStateChanged).
   * @return BTStatus::OK when the read was queued;
   *         InvalidState when the handle is empty or no peer is discovered (also while discovery is still deferred);
   *         NotSupported without CONFIG_BT_MCC_READ_MEDIA_STATE;
   *         another error when the stack refused the read.
   */
  BTStatus readState();
  /**
   * @brief Read the player name (answered via onPlayerName).
   * @return BTStatus::OK when the read was queued;
   *         InvalidState when the handle is empty or no peer is discovered (also while discovery is still deferred);
   *         another error when the stack refused the read.
   */
  BTStatus readPlayerName();
  /**
   * @brief Read the track title (answered via onTrackTitle).
   * @return BTStatus::OK when the read was queued;
   *         InvalidState when the handle is empty or no peer is discovered (also while discovery is still deferred);
   *         NotSupported without CONFIG_BT_MCC_READ_TRACK_TITLE;
   *         another error when the stack refused the read.
   */
  BTStatus readTrackTitle();
  /** @brief Last state reported by the peer (Inactive after disconnect). */
  BLEAudioMediaState getState() const;

  // --- Callbacks (Bluetooth host task; keep them short) ---

  /** @brief Discovery finished; also fires with an error when a deferred discovery could not start. */
  void onDiscovered(DiscoveredCallback cb);
  /** @brief State reads and notifications. */
  void onStateChanged(StateCallback cb);
  /** @brief Media Control Point results. */
  void onCommandResult(CommandCallback cb);
  /** @brief Player name reads. */
  void onPlayerName(TextCallback cb);
  /** @brief Track title reads and notifications. */
  void onTrackTitle(TextCallback cb);
  /** @brief Drop every registered callback. */
  void resetCallbacks();

  struct Impl;

private:
  explicit BLEAudioMediaController(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;

  friend class BLEAudio;
  friend struct BLEAudioImplCommon;
};

#endif /* BLE_AUDIO_SUPPORTED */
