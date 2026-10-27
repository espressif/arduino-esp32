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
 * Created by BLEAudio::createVolumeRenderer() between audio.begin() and
 * audio.start(). The handle is a cheap shared reference; copies control the
 * same renderer. A default-constructed (or failed) handle is empty: calls on
 * it do nothing, and those returning BTStatus return InvalidState.
 */
class BLEAudioVolumeRenderer {
public:
  /** @brief Volume (0..255) and mute after a change. */
  using StateCallback = std::function<void(uint8_t volume, bool muted)>;

  BLEAudioVolumeRenderer();
  ~BLEAudioVolumeRenderer() = default;
  BLEAudioVolumeRenderer(const BLEAudioVolumeRenderer &) = default;
  BLEAudioVolumeRenderer &operator=(const BLEAudioVolumeRenderer &) = default;

  /** @return true when the handle refers to a created renderer. */
  explicit operator bool() const;

  // --- Configuration (before audio.start()) ---

  /** @brief Default 100. */
  BLEAudioVolumeRenderer &setInitialVolume(uint8_t volume);
  /** @brief Default unmuted. */
  BLEAudioVolumeRenderer &setInitialMute(bool muted);
  /** @brief Step used by volumeUp()/volumeDown() and remote relative writes; 1..255, default 1. Also at runtime. */
  BLEAudioVolumeRenderer &setVolumeStep(uint8_t step);

  // --- Local control (after audio.start()); connected controllers are notified ---

  /** @brief Set the absolute volume, 0..255. */
  BTStatus setVolume(uint8_t volume);
  /** @brief Mute; the volume setting is kept. */
  BTStatus mute();
  /** @brief Unmute. */
  BTStatus unmute();
  /** @brief Raise the volume by the step (saturates at 255). */
  BTStatus volumeUp();
  /** @brief Lower the volume by the step (saturates at 0). */
  BTStatus volumeDown();
  /** @return The initial volume before start, then the last reported volume. */
  uint8_t getVolume() const;
  /** @return The initial mute before start, then the last reported mute. */
  bool isMuted() const;

  /** @brief Every volume/mute change, local or remote. */
  BLEAudioVolumeRenderer &onStateChanged(StateCallback cb);
  /** @brief Drop every registered callback. */
  void resetCallbacks();

  struct Impl;

private:
  explicit BLEAudioVolumeRenderer(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;

  friend class BLEAudio;
};

/**
 * @brief VCP Volume Controller (client of one renderer at a time).
 *
 * Created by BLEAudio::createVolumeController() between audio.begin() and
 * audio.start(). Commands are GATT writes: a returned OK means the write was
 * queued, and the outcome arrives as onStateChanged / onOffsetChanged /
 * onInputChanged when the renderer notifies. Commands before discover()
 * return BTStatus::InvalidState.
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

  BLEAudioVolumeController();
  ~BLEAudioVolumeController() = default;
  BLEAudioVolumeController(const BLEAudioVolumeController &) = default;
  BLEAudioVolumeController &operator=(const BLEAudioVolumeController &) = default;

  /** @return true when the handle refers to a created controller. */
  explicit operator bool() const;

  /**
   * @brief Discover the renderer on @p connHandle (after audio.start()).
   *
   * Waits for the engine's GATT discovery of the link if needed. onDiscovered
   * fires once the peer's current volume is known (followed by onStateChanged);
   * every other call below targets this peer.
   */
  BTStatus discover(uint16_t connHandle);
  /** @return The connection passed to discover(), or BLE_AUDIO_CONN_NONE. */
  uint16_t getConnHandle() const;

  /** @brief Write an absolute volume, 0..255. */
  BTStatus setVolume(uint8_t volume);
  /** @brief Mute the renderer. */
  BTStatus mute();
  /** @brief Unmute the renderer. */
  BTStatus unmute();
  /** @brief Relative volume up by the renderer's own step. */
  BTStatus volumeUp();
  /** @brief Relative volume down by the renderer's own step. */
  BTStatus volumeDown();
  /** @brief Re-read the peer's volume state (answered via onStateChanged). */
  BTStatus readState();
  /** @brief Last volume reported by the peer. */
  uint8_t getVolume() const;
  /** @brief Last mute state reported by the peer. */
  bool isMuted() const;

  /** @brief Volume offset of the peer's first VOCS instance, -255..255. */
  BTStatus setVolumeOffset(int16_t offset);
  /** @brief Gain of the peer's first AICS instance, in the peer's gain units. */
  BTStatus setInputGain(int8_t gain);

  /** @brief Discovery finished; also fires with an error when a deferred discovery could not start. */
  BLEAudioVolumeController &onDiscovered(DiscoveredCallback cb);
  /** @brief The renderer's volume or mute changed. */
  BLEAudioVolumeController &onStateChanged(StateCallback cb);
  /** @brief The renderer's volume offset changed. */
  BLEAudioVolumeController &onOffsetChanged(OffsetCallback cb);
  /** @brief The renderer's audio input gain or mute changed. */
  BLEAudioVolumeController &onInputChanged(InputCallback cb);
  /** @brief Drop every registered callback. */
  void resetCallbacks();

  struct Impl;

private:
  explicit BLEAudioVolumeController(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;

  friend class BLEAudio;
};

#endif /* BLE_AUDIO_SUPPORTED */
