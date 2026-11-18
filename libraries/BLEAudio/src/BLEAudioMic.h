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
 * Created by BLEAudio::createMicDevice() while the engine is active and
 * before audio.begin(). Copies of the handle control the same device; an empty
 * handle does nothing, and its BTStatus calls return InvalidState.
 */
class BLEAudioMicDevice {
public:
  /** @brief Mute state after a change. */
  using MuteCallback = std::function<void(bool muted)>;
  /** @brief Local control-point operation reported by onResult. */
  enum class Operation : uint8_t {
    Unknown = 0,  ///< No operation in flight.
    Mute,         ///< mute().
    Unmute,       ///< unmute().
    DisableMute,  ///< disableMute().
  };
  /** @brief A queued mute()/unmute()/disableMute() was rejected; the state did not change. */
  using ResultCallback = std::function<void(Operation op, BTStatus status)>;

  BLEAudioMicDevice();
  ~BLEAudioMicDevice() = default;
  BLEAudioMicDevice(const BLEAudioMicDevice &) = default;
  BLEAudioMicDevice &operator=(const BLEAudioMicDevice &) = default;
  BLEAudioMicDevice(BLEAudioMicDevice &&) = default;
  BLEAudioMicDevice &operator=(BLEAudioMicDevice &&) = default;

  /** @return true when the handle refers to a created device. */
  explicit operator bool() const;

  /**
   * @brief Default unmuted. Before audio.begin().
   * @return BTStatus::OK when staged; InvalidState when the handle is empty or audio is already started.
   */
  BTStatus setInitialMute(bool muted);

  /**
   * @brief Mute locally (after audio.begin()); connected controllers are notified.
   * @return BTStatus::OK when the change was queued (reported via onMuteChanged, or via onResult on failure);
   *         InvalidState when the handle is empty or audio is not started;
   *         another error when the stack refused the request.
   */
  BTStatus mute();
  /**
   * @brief Unmute; also re-enables a disabled mute.
   * @return BTStatus::OK when the change was queued (reported via onMuteChanged, or via onResult on failure);
   *         InvalidState when the handle is empty or audio is not started;
   *         another error when the stack refused the request.
   */
  BTStatus unmute();
  /**
   * @brief Report mute as unavailable; controllers can no longer change it.
   * @return BTStatus::OK when the change was queued (reported via onMuteChanged, or via onResult on failure);
   *         InvalidState when the handle is empty or audio is not started;
   *         another error when the stack refused the request.
   */
  BTStatus disableMute();
  /** @return The initial mute before start, then the last reported mute (false while disabled). */
  bool isMuted() const;
  /** @brief Whether mute is currently reported as disabled (neither muted nor unmuted). */
  bool isMuteDisabled() const;

  // --- Callbacks (Bluetooth host task; keep them short) ---

  /** @brief Every mute change, local or remote (disabled reports unmuted/false: see isMuteDisabled()). */
  void onMuteChanged(MuteCallback cb);
  /** @brief A queued local operation failed; the state did not change. */
  void onResult(ResultCallback cb);
  /** @brief Drop every registered callback. */
  void resetCallbacks();

  struct Impl;

private:
  explicit BLEAudioMicDevice(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;

  friend class BLEAudio;
  friend struct BLEAudioImplCommon;
};

/**
 * @brief MICP Microphone Controller (client of one device at a time).
 *
 * Created by BLEAudio::createMicController() while the engine is active and
 * before audio.begin(). Commands are GATT writes: OK means the write was queued.
 * Success arrives as onMuteChanged / onInputChanged; a rejected write arrives
 * as onResult and changes nothing. Cached state is cleared on disconnect.
 * Commands before discover() return BTStatus::InvalidState. Only the first
 * AICS instance is driven; more instances are reported in onDiscovered and
 * otherwise ignored. A second discover() replaces the peer.
 */
class BLEAudioMicController {
public:
  /** @brief Discovery result, with the number of microphone AICS instances found. */
  using DiscoveredCallback = std::function<void(BTStatus status, uint8_t inputCount)>;
  /** @brief The device's mute state. */
  using MuteCallback = std::function<void(bool muted)>;
  /** @brief Gain and mute of the device's first microphone AICS instance. */
  using InputCallback = std::function<void(int8_t gain, bool muted)>;
  /** @brief Queued control-point operation reported by onResult. */
  enum class Operation : uint8_t {
    Unknown = 0,   ///< No operation in flight.
    Mute,          ///< mute().
    Unmute,        ///< unmute().
    ReadMute,      ///< readMute().
    SetInputGain,  ///< setInputGain().
  };
  /** @brief A queued mute()/unmute()/readMute()/setInputGain() was rejected; the state did not change. */
  using ResultCallback = std::function<void(Operation op, BTStatus status)>;

  BLEAudioMicController();
  ~BLEAudioMicController() = default;
  BLEAudioMicController(const BLEAudioMicController &) = default;
  BLEAudioMicController &operator=(const BLEAudioMicController &) = default;
  BLEAudioMicController(BLEAudioMicController &&) = default;
  BLEAudioMicController &operator=(BLEAudioMicController &&) = default;

  /** @return true when the handle refers to a created controller. */
  explicit operator bool() const;

  /**
   * @brief Discover the microphone device on @p connHandle (after audio.begin()).
   *
   * Waits for the engine's GATT discovery of the link if needed; the peer's
   * mute state is read right after onDiscovered. Every call below targets
   * this peer.
   * @param connHandle ACL connection of the microphone device.
   * @return BTStatus::OK when discovery started (or was deferred until the engine's GATT discovery finishes);
   *         InvalidState when the handle is empty, audio is not started, or @p connHandle is BLE_AUDIO_CONN_NONE;
   *         another error when the stack refused discovery.
   */
  BTStatus discover(uint16_t connHandle);
  /** @return The connection passed to discover(), or BLE_AUDIO_CONN_NONE. */
  uint16_t getConnHandle() const;

  /**
   * @brief Mute the device's microphone.
   * @return BTStatus::OK when the write was queued (reported via onMuteChanged, or via onResult on failure);
   *         InvalidState when the handle is empty or no peer is discovered (also while discovery is still deferred);
   *         another error when the stack refused the write.
   */
  BTStatus mute();
  /**
   * @brief Unmute the device's microphone (fails at the peer while its mute is disabled).
   * @return BTStatus::OK when the write was queued (reported via onMuteChanged, or via onResult on failure);
   *         InvalidState when the handle is empty, no peer is discovered (also while discovery is still deferred),
   *         or the peer currently reports mute as disabled;
   *         another error when the stack refused the write.
   */
  BTStatus unmute();
  /**
   * @brief Re-read the peer's mute state (answered via onMuteChanged).
   * @return BTStatus::OK when the read was queued;
   *         InvalidState when the handle is empty or no peer is discovered (also while discovery is still deferred);
   *         another error when the stack refused the read.
   */
  BTStatus readMute();
  /** @brief Last mute state reported by the peer (false while disabled; cleared on disconnect). */
  bool isMuted() const;
  /** @brief Whether the peer currently reports mute as disabled. */
  bool isMuteDisabled() const;
  /**
   * @brief Gain of the peer's first microphone AICS instance.
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
  /** @brief The device's mute state was read or changed. */
  void onMuteChanged(MuteCallback cb);
  /** @brief The device's microphone input gain or mute changed. */
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
  explicit BLEAudioMicController(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;

  friend class BLEAudio;
  friend struct BLEAudioImplCommon;
};

#endif /* BLE_AUDIO_SUPPORTED */
