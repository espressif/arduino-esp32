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
 * @brief Volume Control Profile (VCP): renderer (VCS server) and controller.
 *
 * Volume is 0..255. The renderer also publishes the Kconfig-sized Volume
 * Offset (VOCS) and Audio Input (AICS) instances; the controller can drive the
 * peer's first instance of each.
 */

#include "core/BLEGuards.h"
#if BLE_AUDIO_SUPPORTED

#include <memory>
#include <functional>
#include "BTStatus.h"

class BLEAudio;

/**
 * @brief VCP Volume Renderer (Volume Control Service server).
 *
 * Created by BLEAudio::createVolumeRenderer() while the engine is active and
 * before audio.begin(). The handle is a cheap shared reference; copies control the
 * same renderer. A default-constructed (or failed) handle is empty: calls on
 * it do nothing, and those returning BTStatus return InvalidState.
 */
class BLEAudioVolumeRenderer {
public:
  /** @brief Volume (0..255) and mute after a change. */
  using StateCallback = std::function<void(uint8_t volume, bool muted)>;
  /** @brief Local control-point operation reported by onResult. */
  enum class Operation : uint8_t {
    Unknown = 0,  ///< No operation in flight.
    SetVolume,    ///< setVolume().
    Mute,         ///< mute().
    Unmute,       ///< unmute().
    VolumeUp,     ///< volumeUp().
    VolumeDown,   ///< volumeDown().
    SetStep,      ///< setVolumeStep() applied at runtime.
  };
  /** @brief A queued local operation failed; the state did not change. */
  using ResultCallback = std::function<void(Operation op, BTStatus status)>;

  BLEAudioVolumeRenderer();
  ~BLEAudioVolumeRenderer() = default;
  BLEAudioVolumeRenderer(const BLEAudioVolumeRenderer &) = default;
  BLEAudioVolumeRenderer &operator=(const BLEAudioVolumeRenderer &) = default;
  BLEAudioVolumeRenderer(BLEAudioVolumeRenderer &&) = default;
  BLEAudioVolumeRenderer &operator=(BLEAudioVolumeRenderer &&) = default;

  /** @return true when the handle refers to a created renderer. */
  explicit operator bool() const;

  // --- Configuration (before audio.begin()) ---

  /**
   * @brief Default 100.
   * @return BTStatus::OK when staged; InvalidState when the handle is empty or audio is already started.
   */
  BTStatus setInitialVolume(uint8_t volume);
  /**
   * @brief Default unmuted.
   * @return BTStatus::OK when staged; InvalidState when the handle is empty or audio is already started.
   */
  BTStatus setInitialMute(bool muted);
  /**
   * @brief Step used by volumeUp()/volumeDown() and remote relative writes; 1..255, default 1. Also at runtime.
   * @return BTStatus::OK when staged (before start) or applied (once running);
   *         InvalidParam when @p step is 0; InvalidState when the handle is empty;
   *         another error when the stack refused the runtime update (the previous step is kept).
   */
  BTStatus setVolumeStep(uint8_t step);

  // --- Local control (after audio.begin()); connected controllers are notified ---

  /**
   * @brief Set the absolute volume, 0..255.
   * @param volume New volume (0..255).
   * @return BTStatus::OK when the change was queued (reported via onStateChanged, or via onResult on failure);
   *         InvalidState when the handle is empty or audio is not started;
   *         another error when the stack refused the request.
   */
  BTStatus setVolume(uint8_t volume);
  /**
   * @brief Mute; the volume setting is kept.
   * @return BTStatus::OK when the change was queued (reported via onStateChanged, or via onResult on failure);
   *         InvalidState when the handle is empty or audio is not started;
   *         another error when the stack refused the request.
   */
  BTStatus mute();
  /**
   * @brief Unmute.
   * @return BTStatus::OK when the change was queued (reported via onStateChanged, or via onResult on failure);
   *         InvalidState when the handle is empty or audio is not started;
   *         another error when the stack refused the request.
   */
  BTStatus unmute();
  /**
   * @brief Raise the volume by the step (saturates at 255).
   * @return BTStatus::OK when the change was queued (reported via onStateChanged, or via onResult on failure);
   *         InvalidState when the handle is empty or audio is not started;
   *         another error when the stack refused the request.
   */
  BTStatus volumeUp();
  /**
   * @brief Lower the volume by the step (saturates at 0).
   * @return BTStatus::OK when the change was queued (reported via onStateChanged, or via onResult on failure);
   *         InvalidState when the handle is empty or audio is not started;
   *         another error when the stack refused the request.
   */
  BTStatus volumeDown();
  /** @return The initial volume before start, then the last reported volume. */
  uint8_t getVolume() const;
  /** @return The initial mute before start, then the last reported mute. */
  bool isMuted() const;

  // --- Callbacks (Bluetooth host task; keep them short) ---

  /** @brief Every volume/mute change, local or remote. */
  void onStateChanged(StateCallback cb);
  /** @brief A queued local operation failed; the state did not change. */
  void onResult(ResultCallback cb);
  /** @brief Drop every registered callback. */
  void resetCallbacks();

  struct Impl;

private:
  explicit BLEAudioVolumeRenderer(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;

  friend class BLEAudio;
  friend struct BLEAudioImplCommon;
};

/**
 * @brief VCP Volume Controller (client of one renderer at a time).
 *
 * Created by BLEAudio::createVolumeController() while the engine is active and
 * before audio.begin(). Commands are GATT writes: a returned OK means the write was
 * queued. Success arrives as onStateChanged / onOffsetChanged / onInputChanged
 * when the renderer notifies; a rejected write arrives as onResult (best
 * effort op attribution, see onResult()) and changes nothing. Cached state is
 * cleared on disconnect. Commands before discover() return
 * BTStatus::InvalidState. Only the first VOCS/AICS instance is driven; more
 * instances are reported in onDiscovered and otherwise ignored. A second
 * discover() replaces the peer.
 */
class BLEAudioVolumeController {
public:
  /** @brief Discovery result, with the number of VOCS and AICS instances found. */
  using DiscoveredCallback = std::function<void(BTStatus status, uint8_t offsetCount, uint8_t inputCount)>;
  /** @brief Renderer volume (0..255) and mute. */
  using StateCallback = std::function<void(uint8_t volume, bool muted)>;
  /** @brief Volume offset of the renderer's first VOCS instance. */
  using OffsetCallback = std::function<void(int16_t offset)>;
  /** @brief Gain and mute of the renderer's first AICS instance. */
  using InputCallback = std::function<void(int8_t gain, bool muted)>;
  /** @brief Queued control-point operation reported by onResult. */
  enum class Operation : uint8_t {
    Unknown = 0,   ///< No operation in flight.
    SetVolume,     ///< setVolume().
    Mute,          ///< mute().
    Unmute,        ///< unmute().
    VolumeUp,      ///< volumeUp().
    VolumeDown,    ///< volumeDown().
    ReadState,     ///< readState().
    SetOffset,     ///< setVolumeOffset().
    SetInputGain,  ///< setInputGain().
  };
  /** @brief A queued operation was rejected; the state did not change. */
  using ResultCallback = std::function<void(Operation op, BTStatus status)>;

  BLEAudioVolumeController();
  ~BLEAudioVolumeController() = default;
  BLEAudioVolumeController(const BLEAudioVolumeController &) = default;
  BLEAudioVolumeController &operator=(const BLEAudioVolumeController &) = default;
  BLEAudioVolumeController(BLEAudioVolumeController &&) = default;
  BLEAudioVolumeController &operator=(BLEAudioVolumeController &&) = default;

  /** @return true when the handle refers to a created controller. */
  explicit operator bool() const;

  /**
   * @brief Discover the renderer on @p connHandle (after audio.begin()).
   *
   * Waits for the engine's GATT discovery of the link if needed. onDiscovered
   * fires once the peer's current volume is known (followed by onStateChanged);
   * every other call below targets this peer.
   * @param connHandle ACL connection of the renderer.
   * @return BTStatus::OK when discovery started (or was deferred until the engine's GATT discovery finishes);
   *         InvalidState when the handle is empty, audio is not started, or @p connHandle is BLE_AUDIO_CONN_NONE;
   *         another error when the stack refused discovery.
   */
  BTStatus discover(uint16_t connHandle);
  /** @return The connection passed to discover(), or BLE_AUDIO_CONN_NONE. */
  uint16_t getConnHandle() const;

  /**
   * @brief Write an absolute volume, 0..255.
   * @param volume New volume (0..255).
   * @return BTStatus::OK when the write was queued (reported via onStateChanged, or via onResult on failure);
   *         InvalidState when the handle is empty or no peer is discovered (also while discovery is still deferred);
   *         another error when the stack refused the write.
   */
  BTStatus setVolume(uint8_t volume);
  /**
   * @brief Mute the renderer.
   * @return BTStatus::OK when the write was queued (reported via onStateChanged, or via onResult on failure);
   *         InvalidState when the handle is empty or no peer is discovered (also while discovery is still deferred);
   *         another error when the stack refused the write.
   */
  BTStatus mute();
  /**
   * @brief Unmute the renderer.
   * @return BTStatus::OK when the write was queued (reported via onStateChanged, or via onResult on failure);
   *         InvalidState when the handle is empty or no peer is discovered (also while discovery is still deferred);
   *         another error when the stack refused the write.
   */
  BTStatus unmute();
  /**
   * @brief Relative volume up by the renderer's own step.
   * @return BTStatus::OK when the write was queued (reported via onStateChanged, or via onResult on failure);
   *         InvalidState when the handle is empty or no peer is discovered (also while discovery is still deferred);
   *         another error when the stack refused the write.
   */
  BTStatus volumeUp();
  /**
   * @brief Relative volume down by the renderer's own step.
   * @return BTStatus::OK when the write was queued (reported via onStateChanged, or via onResult on failure);
   *         InvalidState when the handle is empty or no peer is discovered (also while discovery is still deferred);
   *         another error when the stack refused the write.
   */
  BTStatus volumeDown();
  /**
   * @brief Re-read the peer's volume state (answered via onStateChanged).
   * @return BTStatus::OK when the read was queued;
   *         InvalidState when the handle is empty or no peer is discovered (also while discovery is still deferred);
   *         another error when the stack refused the read.
   */
  BTStatus readState();
  /** @brief Last volume reported by the peer. */
  uint8_t getVolume() const;
  /** @brief Last mute state reported by the peer. */
  bool isMuted() const;

  /**
   * @brief Volume offset of the peer's first VOCS instance, -255..255.
   * @param offset New offset (-255..255).
   * @return BTStatus::OK when the write was queued (reported via onOffsetChanged, or via onResult on failure);
   *         InvalidState when the handle is empty, no peer is discovered, or the peer exposed no VOCS instance;
   *         NotSupported when this build has no VOCS controller support;
   *         another error when the stack refused the write.
   */
  BTStatus setVolumeOffset(int16_t offset);
  /**
   * @brief Gain of the peer's first AICS instance, in the peer's gain units.
   * @param gain New gain, in the peer's gain units.
   * @return BTStatus::OK when the write was queued (reported via onInputChanged, or via onResult on failure);
   *         InvalidState when the handle is empty, no peer is discovered, or the peer exposed no AICS instance;
   *         NotSupported when this build has no AICS controller support;
   *         another error when the stack refused the write.
   */
  BTStatus setInputGain(int8_t gain);

  // --- Callbacks (Bluetooth host task; keep them short) ---

  /** @brief Discovery finished; also fires with an error when a deferred discovery could not start. */
  void onDiscovered(DiscoveredCallback cb);
  /** @brief The renderer's volume or mute changed. */
  void onStateChanged(StateCallback cb);
  /** @brief The renderer's volume offset changed. */
  void onOffsetChanged(OffsetCallback cb);
  /** @brief The renderer's audio input gain or mute changed. */
  void onInputChanged(InputCallback cb);
  /**
   * @brief A queued operation failed; the state did not change.
   *
   * The op is the most recently queued one (best effort: concurrent writes
   * share one notification channel, so with several in flight the reported op
   * may be a sibling). Unknown when no operation was queued.
   * NotConnected when the link dropped while an operation was in flight.
   */
  void onResult(ResultCallback cb);
  /** @brief Drop every registered callback. */
  void resetCallbacks();

  struct Impl;

private:
  explicit BLEAudioVolumeController(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;

  friend class BLEAudio;
  friend struct BLEAudioImplCommon;
};

#endif /* BLE_AUDIO_SUPPORTED */
