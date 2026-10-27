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
 * @brief Microphone Control Profile (MICP): microphone device (MICS server)
 *        and controller.
 *
 * The device also publishes the Kconfig-sized microphone Audio Input (AICS)
 * instances; the controller can set the gain of the peer's first one.
 */

#include "core/BLEGuards.h"
#if BLE_AUDIO_SUPPORTED

#include <memory>
#include <functional>
#include "BTStatus.h"

class BLEAudio;

/**
 * @brief MICP Microphone Device (Microphone Control Service server).
 *
 * Created by BLEAudio::createMicDevice() between audio.begin() and
 * audio.start(). Copies of the handle control the same device; an empty
 * handle does nothing, and its BTStatus calls return InvalidState.
 */
class BLEAudioMicDevice {
public:
  /** @brief Mute state after a change. */
  using MuteCallback = std::function<void(bool muted)>;

  BLEAudioMicDevice();
  ~BLEAudioMicDevice() = default;
  BLEAudioMicDevice(const BLEAudioMicDevice &) = default;
  BLEAudioMicDevice &operator=(const BLEAudioMicDevice &) = default;

  /** @return true when the handle refers to a created device. */
  explicit operator bool() const;

  /** @brief Default unmuted. Before audio.start(). */
  BLEAudioMicDevice &setInitialMute(bool muted);

  /** @brief Mute locally (after audio.start()); connected controllers are notified. */
  BTStatus mute();
  /** @brief Unmute; also re-enables a disabled mute. */
  BTStatus unmute();
  /** @brief Report mute as unavailable; controllers can no longer change it. */
  BTStatus disableMute();
  /** @return The initial mute before start, then the last reported mute. */
  bool isMuted() const;

  /** @brief Every mute change, local or remote. */
  BLEAudioMicDevice &onMuteChanged(MuteCallback cb);
  /** @brief Drop every registered callback. */
  void resetCallbacks();

  struct Impl;

private:
  explicit BLEAudioMicDevice(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;

  friend class BLEAudio;
};

/**
 * @brief MICP Microphone Controller (client of one device at a time).
 *
 * Created by BLEAudio::createMicController() between audio.begin() and
 * audio.start(). Commands are GATT writes: OK means the write was queued,
 * and the result arrives as onMuteChanged / onInputChanged. Commands before
 * discover() return BTStatus::InvalidState.
 */
class BLEAudioMicController {
public:
  /** @brief Discovery result, with the number of microphone AICS instances found. */
  using DiscoveredCallback = std::function<void(BTStatus status, uint8_t inputCount)>;
  /** @brief The device's mute state. */
  using MuteCallback = std::function<void(bool muted)>;
  /** @brief Gain and mute of the device's first microphone AICS instance. */
  using InputCallback = std::function<void(int8_t gain, bool muted)>;

  BLEAudioMicController();
  ~BLEAudioMicController() = default;
  BLEAudioMicController(const BLEAudioMicController &) = default;
  BLEAudioMicController &operator=(const BLEAudioMicController &) = default;

  /** @return true when the handle refers to a created controller. */
  explicit operator bool() const;

  /**
   * @brief Discover the microphone device on @p connHandle (after audio.start()).
   *
   * Waits for the engine's GATT discovery of the link if needed; the peer's
   * mute state is read right after onDiscovered. Every call below targets
   * this peer.
   */
  BTStatus discover(uint16_t connHandle);
  /** @return The connection passed to discover(), or BLE_AUDIO_CONN_NONE. */
  uint16_t getConnHandle() const;

  /** @brief Mute the device's microphone. */
  BTStatus mute();
  /** @brief Unmute the device's microphone (fails at the peer while its mute is disabled). */
  BTStatus unmute();
  /** @brief Re-read the peer's mute state (answered via onMuteChanged). */
  BTStatus readMute();
  /** @brief Last mute state reported by the peer. */
  bool isMuted() const;
  /** @brief Gain of the peer's first microphone AICS instance. */
  BTStatus setInputGain(int8_t gain);

  /** @brief Discovery finished; also fires with an error when a deferred discovery could not start. */
  BLEAudioMicController &onDiscovered(DiscoveredCallback cb);
  /** @brief The device's mute state was read or changed. */
  BLEAudioMicController &onMuteChanged(MuteCallback cb);
  /** @brief The device's microphone input gain or mute changed. */
  BLEAudioMicController &onInputChanged(InputCallback cb);
  /** @brief Drop every registered callback. */
  void resetCallbacks();

  struct Impl;

private:
  explicit BLEAudioMicController(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;

  friend class BLEAudio;
};

#endif /* BLE_AUDIO_SUPPORTED */
