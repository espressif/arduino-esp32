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
 * Created by BLEAudio::createHearingAidDevice() while the engine is active and
 * before audio.begin(). Copies of the handle control the same device; an empty
 * handle does nothing, and its BTStatus calls return InvalidState.
 */
class BLEAudioHearingAidDevice {
public:
  /**
   * @brief A controller selected a preset.
   * @param index Selected preset index.
   * @param sync True when the controller asked the binaural pair to follow.
   */
  using SelectCallback = std::function<void(uint8_t index, bool sync)>;
  /**
   * @brief A controller renamed a preset.
   * @param index Renamed preset index.
   * @param name New preset name.
   */
  using RenameCallback = std::function<void(uint8_t index, const String &name)>;

  BLEAudioHearingAidDevice();
  ~BLEAudioHearingAidDevice() = default;
  BLEAudioHearingAidDevice(const BLEAudioHearingAidDevice &) = default;
  BLEAudioHearingAidDevice &operator=(const BLEAudioHearingAidDevice &) = default;
  BLEAudioHearingAidDevice(BLEAudioHearingAidDevice &&) = default;
  BLEAudioHearingAidDevice &operator=(BLEAudioHearingAidDevice &&) = default;

  /** @return true when the handle refers to a created device. */
  explicit operator bool() const;

  // --- Configuration (before audio.begin()) ---

  /** @brief Default Monaural.
   * @return BTStatus::OK when staged; InvalidState when the handle is empty or after audio.begin(). */
  BTStatus setType(BLEHearingAidType type);
  /** @brief Keep presets in sync across a binaural pair. Default off.
   * @return BTStatus::OK when staged; InvalidState when the handle is empty or after audio.begin(). */
  BTStatus setPresetSync(bool enabled);

  /**
   * @brief Add a preset (index >= 1, unique).
   *
   * Presets added before audio.begin() are registered by start(); later ones
   * are registered at once. At most CONFIG_BT_HAS_PRESET_COUNT presets;
   * @p writable needs CONFIG_BT_HAS_PRESET_NAME_DYNAMIC.
   * @param index Preset index (>= 1, unique; 0 is logged and ignored).
   * @param name Preset name shown to controllers.
   * @param available Whether controllers may select the preset.
   * @param writable Whether controllers may rename the preset.
   * @return BTStatus::OK when staged or registered; InvalidState when the handle is empty;
   *         InvalidParam for index 0 or an empty/duplicate name; another status when registration failed.
   */
  BTStatus addPreset(uint8_t index, const String &name, bool available = true, bool writable = false);

  // --- Runtime (after audio.begin()); connected controllers are notified ---

  /**
   * @brief Unregister preset @p index.
   * @param index Preset index to remove.
   * @return BTStatus::InvalidState when the handle is empty; otherwise the stack status.
   */
  BTStatus removePreset(uint8_t index);
  /**
   * @brief Mark preset @p index (un)available; controllers cannot select an unavailable one.
   * @param index Preset index to update.
   * @param available True to make the preset selectable.
   * @return BTStatus::InvalidState when the handle is empty; otherwise the stack status.
   */
  BTStatus setPresetAvailable(uint8_t index, bool available);
  /**
   * @brief Rename preset @p index locally.
   * @param index Preset index to rename.
   * @param name New preset name.
   * @return BTStatus::InvalidState when the handle is empty; otherwise the stack status.
   */
  BTStatus renamePreset(uint8_t index, const String &name);
  /**
   * @brief Activate preset @p index locally.
   * @param index Preset index to activate.
   * @return BTStatus::InvalidState when the handle is empty; otherwise the stack status.
   */
  BTStatus setActivePreset(uint8_t index);
  /** @return the active preset index, 0 when none. */
  uint8_t getActivePreset() const;

  // --- Callbacks (Bluetooth host task; keep them short) ---

  /** @brief A controller selected a preset. */
  void onPresetSelected(SelectCallback cb);
  /** @brief A controller renamed a writable preset. */
  void onPresetRenamed(RenameCallback cb);
  /** @brief Drop every registered callback. */
  void resetCallbacks();

  struct Impl;

private:
  explicit BLEAudioHearingAidDevice(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;

  friend class BLEAudio;
  friend struct BLEAudioImplCommon;
};

/**
 * @brief HAP Hearing Aid Unicast Client (HAS client), bound to one peer.
 *
 * Created by BLEAudio::createHearingAidController() while the engine is active and
 * before audio.begin(). Requests are GATT operations: OK means the request was
 * queued, and the answer arrives through onPreset / onPresetSwitch.
 * Requests fail until discover() has bound a peer.
 */
class BLEAudioHearingAidController {
public:
  /**
   * @brief Discovery result.
   * @param status OK when discovery finished; error status when it failed.
   * @param type The peer's hearing aid type.
   */
  using DiscoveredCallback = std::function<void(BTStatus status, BLEHearingAidType type)>;
  /**
   * @brief One preset record, from readPresets() or a peer notification.
   * @param index Preset index.
   * @param available Whether the preset is selectable.
   * @param name Preset name.
   * @param isLast True for the last record of a readPresets() batch.
   */
  using PresetCallback = std::function<void(uint8_t index, bool available, const String &name, bool isLast)>;
  /**
   * @brief The peer's active preset changed.
   * @param status OK when the switch succeeded; error status when it failed.
   * @param index New active preset index.
   */
  using SwitchCallback = std::function<void(BTStatus status, uint8_t index)>;

  BLEAudioHearingAidController();
  ~BLEAudioHearingAidController() = default;
  BLEAudioHearingAidController(const BLEAudioHearingAidController &) = default;
  BLEAudioHearingAidController &operator=(const BLEAudioHearingAidController &) = default;
  BLEAudioHearingAidController(BLEAudioHearingAidController &&) = default;
  BLEAudioHearingAidController &operator=(BLEAudioHearingAidController &&) = default;

  /** @return true when the handle refers to a created controller. */
  explicit operator bool() const;

  /**
   * @brief Discover the peer's HAS on @p connHandle (after audio.begin()).
   *
   * Waits for the engine's GATT discovery of the link if needed. Only one
   * peer is driven at a time: discovering another one replaces it.
   * @param connHandle ACL connection handle of the peer.
   * @return BTStatus::InvalidState when the handle is empty, before audio.begin(), or for BLE_AUDIO_CONN_NONE;
   *         OK when discovery started or was deferred until GATT discovery; another status when discovery failed to start.
   */
  BTStatus discover(uint16_t connHandle);
  /** @return The connection passed to discover(), or BLE_AUDIO_CONN_NONE. */
  uint16_t getConnHandle() const;

  /**
   * @brief Read presets from the peer (one onPreset each, the last with isLast).
   * @param startIndex First preset index to read.
   * @param maxCount Maximum number of presets to read.
   * @return BTStatus::InvalidState when the handle is empty or no peer is discovered/active;
   *         OK when the request was queued (answers via onPreset); another status when the request failed to start.
   */
  BTStatus readPresets(uint8_t startIndex = 1, uint8_t maxCount = 255);
  /**
   * @brief Activate preset @p index on the peer.
   * @param index Preset index to activate.
   * @param sync Ask the peer to switch its binaural partner too (needs preset sync support on the peer).
   * @return BTStatus::InvalidState when the handle is empty or no peer is discovered/active;
   *         OK when the request was queued (answer via onPresetSwitch); another status when the request failed to start.
   */
  BTStatus setActivePreset(uint8_t index, bool sync = false);
  /**
   * @brief Activate the peer's next available preset.
   * @param sync As in setActivePreset().
   * @return BTStatus::InvalidState when the handle is empty or no peer is discovered/active;
   *         OK when the request was queued (answer via onPresetSwitch); another status when the request failed to start.
   */
  BTStatus nextPreset(bool sync = false);
  /**
   * @brief Activate the peer's previous available preset.
   * @param sync As in setActivePreset().
   * @return BTStatus::InvalidState when the handle is empty or no peer is discovered/active;
   *         OK when the request was queued (answer via onPresetSwitch); another status when the request failed to start.
   */
  BTStatus previousPreset(bool sync = false);
  /**
   * @brief Rename a writable preset on the peer.
   * @param index Preset index to rename.
   * @param name New preset name.
   * @return BTStatus::InvalidState when the handle is empty or no peer is discovered/active;
   *         OK when the request was queued; another status when the request failed to start.
   */
  BTStatus renamePreset(uint8_t index, const String &name);
  /** @return the peer's last reported active preset, 0 when unknown. */
  uint8_t getActivePreset() const;

  // --- Callbacks (Bluetooth host task; keep them short) ---

  /** @brief Discovery finished; also fires with an error when a deferred discovery could not start. */
  void onDiscovered(DiscoveredCallback cb);
  /** @brief Preset records (reads and change notifications). */
  void onPreset(PresetCallback cb);
  /** @brief The peer's active preset changed. */
  void onPresetSwitch(SwitchCallback cb);
  /** @brief Drop every registered callback. */
  void resetCallbacks();

  struct Impl;

private:
  explicit BLEAudioHearingAidController(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;

  friend class BLEAudio;
  friend struct BLEAudioImplCommon;
};

#endif /* BLE_AUDIO_SUPPORTED */
