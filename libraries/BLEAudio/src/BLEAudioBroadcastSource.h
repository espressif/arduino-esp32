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
 * @brief BAP Broadcast Source role (Auracast transmitter).
 *
 * A broadcast source sends audio to any number of receivers without a
 * connection. It transmits one BIG (Broadcast Isochronous Group) with one BIS
 * per channel: mono uses one Tx stream, stereo two (front left, front right),
 * all in one subgroup.
 *
 * start() owns one extended advertising instance and sets up everything a
 * receiver needs to find and join the broadcast:
 *  - Extended advertising: the Broadcast Audio Announcement (Broadcast ID),
 *    the Public Broadcast Announcement when PBP is compiled in, and the
 *    Broadcast Name.
 *  - Periodic advertising: the BASE (codec, BIS layout, metadata).
 *  - The BIG itself, attached to that advertising train.
 *
 * Feed each stream one SDU per SDU interval once onStarted() fires (or use a
 * `BLEAudioRecorder` to encode PCM).
 *
 * @code
 * BLEAudioBroadcastSource source = audio.createBroadcastSource();
 * source.setPreset(BLEAudioCodecPreset::LC3_48_4_1);
 * source.setChannels(2);
 * source.setName("Living room");
 * audio.begin();
 * source.start();
 * @endcode
 */

#include "core/BLEGuards.h"
#if BLE_AUDIO_SUPPORTED

#include <memory>
#include <functional>
#include "WString.h"
#include "BTStatus.h"
#include "BLEAudioTypes.h"
#include "BLEAudioStream.h"

class BLEAudio;

class BLEAudioBroadcastSource {
public:
  /**
   * @brief The BIG is up: every stream is streaming.
   *
   * Runs on the Bluetooth host task; keep it short.
   */
  using Callback = std::function<void()>;
  /** @brief The BIG was terminated; @p reason is the HCI reason (0x16 after stop()). */
  using StoppedCallback = std::function<void(uint8_t reason)>;

  BLEAudioBroadcastSource();
  ~BLEAudioBroadcastSource() = default;
  BLEAudioBroadcastSource(const BLEAudioBroadcastSource &) = default;
  BLEAudioBroadcastSource &operator=(const BLEAudioBroadcastSource &) = default;
  BLEAudioBroadcastSource(BLEAudioBroadcastSource &&) = default;
  BLEAudioBroadcastSource &operator=(BLEAudioBroadcastSource &&) = default;

  /** @brief Whether this handle references a source (false when the factory failed). */
  explicit operator bool() const;

  // --- Configuration (applied by the next start(); every setter returns BTStatus) ---
  //
  // Setters return InvalidState on an empty handle. Codec, QoS, channels,
  // code and context mark the engine source dirty so start() re-creates it;
  // the Broadcast ID, name and PBP flag only change the advertising.

  /**
   * @brief Codec and QoS from a BAP preset (broadcast table). Default LC3_16_2_1.
   * @param preset Preset to use.
   * @return BTStatus::OK when staged; InvalidState on an empty handle or while
   *         starting or streaming; InvalidParam for an unknown preset.
   */
  BTStatus setPreset(BLEAudioCodecPreset preset);
  /**
   * @brief Replace the preset's QoS with a custom one.
   *
   * Without it, the preset QoS is used with the controller's presentation delay.
   * @param qos Custom QoS applied instead of the preset one from the next start().
   * @return BTStatus::OK when staged; InvalidState on an empty handle or while
   *         starting or streaming.
   */
  BTStatus setQos(const BLEAudioQos &qos);
  /**
   * @brief Number of channels, one BIS each: 1 = mono (default), 2 = stereo (FL + FR).
   *
   * A stopped engine source is deleted before the stream vector is resized, so
   * the engine never points at freed slots. Streams are (re)allocated
   * immediately; attach callbacks afterwards.
   * @param channels 1 = mono (one BIS), 2 = stereo (two BISes).
   * @return BTStatus::OK when staged; InvalidState on an empty handle or while
   *         starting or streaming; InvalidParam unless @p channels is 1 or 2.
   */
  BTStatus setChannels(uint8_t channels);
  /**
   * @brief 24-bit Broadcast ID receivers use to recognize the source. Default: random.
   * @param broadcastId Only the low 24 bits are kept; advertised, not baked into the source.
   * @return BTStatus::OK when staged; InvalidState on an empty handle.
   */
  BTStatus setBroadcastId(uint32_t broadcastId);
  /**
   * @brief Broadcast Code (up to 16 characters) that encrypts the BIG.
   *
   * Empty (default) = unencrypted. Receivers need the same code to decode.
   * @param code Broadcast Code, empty (open) or up to 16 characters.
   * @return BTStatus::OK when staged; InvalidState on an empty handle;
   *         InvalidParam when @p code is longer than 16 characters.
   */
  BTStatus setBroadcastCode(const String &code);
  /**
   * @brief Broadcast Name shown by scanners (up to 32 characters). Default: the device name.
   * @param name Name advertised in the extended advertising; empty falls back to the device name.
   * @return BTStatus::OK when staged; InvalidState on an empty handle;
   *         InvalidParam when @p name is longer than 32 characters.
   */
  BTStatus setName(const String &name);
  /**
   * @brief Streaming context in the BASE metadata. Default Media.
   * @param context Context announced in the BASE.
   * @return BTStatus::OK when staged; InvalidState on an empty handle.
   */
  BTStatus setContext(BLEAudioContext context);
  /**
   * @brief Advertise a Public Broadcast Announcement (Auracast, PBP builds only).
   *
   * Lets phones list the broadcast with its quality and encryption flags.
   * Default on; ignored when PBP is not compiled in.
   * @param enable True to advertise the PBA alongside the announcement.
   * @return BTStatus::OK when staged; InvalidState on an empty handle.
   */
  BTStatus setPublicBroadcast(bool enable);

  /**
   * @brief Configure a ready-to-start Auracast transmitter.
   *
   * Shorthand for:
   * @code
   * setPreset(preset);
   * setChannels(channels);
   * setName(name);
   * setContext(Media);
   * setPublicBroadcast(true);
   * @endcode
   *
   * @param name     Broadcast Name shown by receivers and phone Auracast UIs
   *                 (up to 32 characters, must not be empty).
   * @param channels 1 = mono (one BIS), 2 = stereo (front-left + front-right
   *                 BISes). Default 2.
   * @param preset   Codec/QoS preset. Default LC3_48_2_1: 48 kHz music, the
   *                 usual Auracast quality.
   * @return BTStatus::OK when every setter was applied; the first error
   *         otherwise (InvalidState on an empty handle or while starting or
   *         streaming; InvalidParam for an empty or overlong @p name, a bad
   *         @p channels count or an unknown @p preset).
   *
   * Call it before the commit (audio.begin()). Encryption stays opt-in: call
   * setBroadcastCode() separately. Individual setters can still override any
   * field afterwards.
   */
  BTStatus configureAuracast(const String &name, uint8_t channels = 2, BLEAudioCodecPreset preset = BLEAudioCodecPreset::LC3_48_2_1);

  // --- Control (after audio.begin()) ---

  /**
   * @brief Start advertising and create the BIG (non-blocking; onStarted() follows).
   *
   * Re-creates the source first when the configuration changed since the
   * last start().
   *
   * @param advInstance Extended advertising instance reserved for the broadcast.
   *                    Must be a user-addressable set (below the reserved
   *                    legacy instance). Default 0 is the first user set;
   *                    1 is the legacy instance on targets with only two
   *                    extended slots (e.g. ESP32-S31).
   * @return BTStatus::OK when started; InvalidState before audio.begin(),
   *         while streaming or without streams; another error when creating
   *         the source or the advertising failed.
   */
  BTStatus start(uint8_t advInstance = 0);
  /**
   * @brief Terminate the BIG and stop the advertising instance (onStopped() follows).
   *
   * Blocks until the BIG is down (or 5 s) so a following `audio.end()` /
   * `BLE.end()` does not tear the host down over a live ISO channel.
   * Called from a Bluetooth stack/audio callback, stop() is refused with Busy
   * because it must wait for the asynchronous BIG terminate; defer it to loop().
   * @return BTStatus::OK on success (also when already stopped); InvalidState
   *         when not started or on an empty handle; Busy from a stack callback;
   *         another error when the teardown stalled (retry stop()).
   */
  BTStatus stop();
  /**
   * @brief Whether the BIG is up (between onStarted() and onStopped()).
   * @return True while the BIG streams; false on an empty handle.
   */
  bool isStreaming() const;
  /**
   * @brief Change the streaming context in the BASE while running.
   * @param context Context written into the existing source metadata in place.
   * @return BTStatus::OK on success; InvalidState before the first start() or on
   *         an empty handle; another error when the engine refused the update.
   */
  BTStatus updateContext(BLEAudioContext context);
  /**
   * @brief Broadcast ID in use (random unless set).
   * @return The staged ID (masked to 24 bits); 0 on an empty handle.
   */
  uint32_t getBroadcastId() const;

  // --- Streams (one Tx stream per BIS; valid after setChannels()) ---

  /** @brief Number of streams (= channels, fewer if the pool ran out). */
  size_t streamCount() const;
  /**
   * @brief Stream @p index: 0 = mono or front left, 1 = front right. Empty when out of range.
   * @param index BIS position (0 = mono/front left, 1 = front right).
   * @return The stream, or an empty handle when @p index is out of range.
   */
  BLEAudioStream stream(size_t index) const;

  // --- Callbacks (Bluetooth host task; keep them short) ---
  //
  // Every setter below takes the callback in @p cb; a no-op on an empty handle.

  /** @brief The BIG is up: every stream is streaming.
   *  @param cb Callback fired once the BIG streams. */
  void onStarted(Callback cb);
  /** @brief The BIG was terminated.
   *  @param cb Callback receiving the HCI termination @p reason. */
  void onStopped(StoppedCallback cb);
  /** @brief Clear every callback of this role. */
  void resetCallbacks();

  struct Impl;

private:
  explicit BLEAudioBroadcastSource(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;

  friend class BLEAudio;
};

#endif /* BLE_AUDIO_SUPPORTED */
