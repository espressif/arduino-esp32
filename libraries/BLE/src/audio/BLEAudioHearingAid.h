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
 * @brief Hearing Access Profile (HAP): hearing aid device (HAS server) and
 *        hearing aid controller (HAS client).
 *
 * A hearing aid publishes named presets (listening programs) and lets a
 * controller read them and switch the active one.
 */

#include "core/BLEGuards.h"
#if BLE_AUDIO_SUPPORTED

#include <memory>
#include <functional>
#include "WString.h"
#include "BTStatus.h"

class BLEAudio;

/** Hearing Aid Type field of the Hearing Aid Features characteristic. */
enum class BLEHearingAidType : uint8_t {
  Binaural = 0,  //!< One of a coordinated left/right pair.
  Monaural = 1,  //!< A single hearing aid.
  Banded = 2,    //!< Two aids sharing one radio.
};

/**
 * @brief HAP Hearing Aid (HAS server).
 *
 * Created by BLEAudio::createHearingAidDevice() between audio.begin() and
 * audio.start(). Copies of the handle control the same device; an empty
 * handle does nothing, and its BTStatus calls return InvalidState.
 */
class BLEAudioHearingAidDevice {
public:
  /**
   * @brief A controller selected @p index; the stack has already activated it.
   *        @p sync is set when the controller asked for the pair to follow.
   */
  using SelectCallback = std::function<void(uint8_t index, bool sync)>;
  /** @brief New name of preset @p index. */
  using RenameCallback = std::function<void(uint8_t index, const String &name)>;

  BLEAudioHearingAidDevice();
  ~BLEAudioHearingAidDevice() = default;
  BLEAudioHearingAidDevice(const BLEAudioHearingAidDevice &) = default;
  BLEAudioHearingAidDevice &operator=(const BLEAudioHearingAidDevice &) = default;

  /** @return true when the handle refers to a created device. */
  explicit operator bool() const;

  // --- Configuration (before audio.start()) ---

  /** @brief Default Monaural. */
  BLEAudioHearingAidDevice &setType(BLEHearingAidType type);
  /** @brief Keep presets in sync across a binaural pair. Default off. */
  BLEAudioHearingAidDevice &setPresetSync(bool enabled);

  /**
   * @brief Add a preset (index >= 1, unique).
   *
   * Presets added before audio.start() are registered by start(); later ones
   * are registered at once. At most CONFIG_BT_HAS_PRESET_COUNT presets;
   * @p writable needs CONFIG_BT_HAS_PRESET_NAME_DYNAMIC.
   */
  BLEAudioHearingAidDevice &addPreset(uint8_t index, const String &name, bool available = true, bool writable = false);

  // --- Runtime (after audio.start()); connected controllers are notified ---

  /** @brief Unregister preset @p index. */
  BTStatus removePreset(uint8_t index);
  /** @brief Mark preset @p index (un)available; controllers cannot select an unavailable one. */
  BTStatus setPresetAvailable(uint8_t index, bool available);
  /** @brief Rename preset @p index locally. */
  BTStatus renamePreset(uint8_t index, const String &name);
  /** @brief Activate preset @p index locally. */
  BTStatus setActivePreset(uint8_t index);
  /** @return the active preset index, 0 when none. */
  uint8_t getActivePreset() const;

  /** @brief A controller selected a preset. */
  BLEAudioHearingAidDevice &onPresetSelected(SelectCallback cb);
  /** @brief A controller renamed a writable preset. */
  BLEAudioHearingAidDevice &onPresetRenamed(RenameCallback cb);
  /** @brief Drop every registered callback. */
  void resetCallbacks();

  struct Impl;

private:
  explicit BLEAudioHearingAidDevice(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;

  friend class BLEAudio;
};

/**
 * @brief HAP Hearing Aid Unicast Client (HAS client), bound to one peer.
 *
 * Created by BLEAudio::createHearingAidController() between audio.begin()
 * and audio.start(). Requests are GATT operations: OK means the request was
 * queued, and the answer arrives through onPreset / onPresetSwitch.
 * Requests fail until discover() has bound a peer.
 */
class BLEAudioHearingAidController {
public:
  /** @brief Discovery result, with the peer's hearing aid type. */
  using DiscoveredCallback = std::function<void(BTStatus status, BLEHearingAidType type)>;
  /** @brief One preset record, from readPresets() or a peer notification. */
  using PresetCallback = std::function<void(uint8_t index, bool available, const String &name, bool isLast)>;
  /** @brief The peer's active preset is now @p index (or the switch failed with @p status). */
  using SwitchCallback = std::function<void(BTStatus status, uint8_t index)>;

  BLEAudioHearingAidController();
  ~BLEAudioHearingAidController() = default;
  BLEAudioHearingAidController(const BLEAudioHearingAidController &) = default;
  BLEAudioHearingAidController &operator=(const BLEAudioHearingAidController &) = default;

  /** @return true when the handle refers to a created controller. */
  explicit operator bool() const;

  /**
   * @brief Discover the peer's HAS on @p connHandle (after audio.start()).
   *
   * Waits for the engine's GATT discovery of the link if needed. Only one
   * peer is driven at a time: discovering another one replaces it.
   */
  BTStatus discover(uint16_t connHandle);
  /** @return The connection passed to discover(), or BLE_AUDIO_CONN_NONE. */
  uint16_t getConnHandle() const;

  /** @brief Read up to @p maxCount presets from @p startIndex (one onPreset each, the last with isLast). */
  BTStatus readPresets(uint8_t startIndex = 1, uint8_t maxCount = 255);
  /**
   * @brief Activate preset @p index on the peer.
   * @param sync Ask the peer to switch its binaural partner too (needs preset sync support on the peer).
   */
  BTStatus setActivePreset(uint8_t index, bool sync = false);
  /** @brief Activate the peer's next available preset; @p sync as in setActivePreset(). */
  BTStatus nextPreset(bool sync = false);
  /** @brief Activate the peer's previous available preset; @p sync as in setActivePreset(). */
  BTStatus previousPreset(bool sync = false);
  /** @brief Rename a writable preset on the peer. */
  BTStatus renamePreset(uint8_t index, const String &name);
  /** @return the peer's last reported active preset, 0 when unknown. */
  uint8_t getActivePreset() const;

  /** @brief Discovery finished; also fires with an error when a deferred discovery could not start. */
  BLEAudioHearingAidController &onDiscovered(DiscoveredCallback cb);
  /** @brief Preset records (reads and change notifications). */
  BLEAudioHearingAidController &onPreset(PresetCallback cb);
  /** @brief The peer's active preset changed. */
  BLEAudioHearingAidController &onPresetSwitch(SwitchCallback cb);
  /** @brief Drop every registered callback. */
  void resetCallbacks();

  struct Impl;

private:
  explicit BLEAudioHearingAidController(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;

  friend class BLEAudio;
};

#endif /* BLE_AUDIO_SUPPORTED */
