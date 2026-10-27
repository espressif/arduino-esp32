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
 * source.setPreset(BLEAudioCodecPreset::LC3_48_4_1).setChannels(2).setName("Living room");
 * audio.start();
 * source.start();
 * @endcode
 */

#include "core/BLEGuards.h"
#if BLE_AUDIO_SUPPORTED

#include <memory>
#include <functional>
#include "WString.h"
#include "BTStatus.h"
#include "audio/BLEAudioTypes.h"
#include "audio/BLEAudioStream.h"

class BLEAudio;

class BLEAudioBroadcastSource {
public:
  using Callback = std::function<void()>;
  /** @brief The BIG was terminated; @p reason is the HCI reason (0x16 after stop()). */
  using StoppedCallback = std::function<void(uint8_t reason)>;

  BLEAudioBroadcastSource();
  ~BLEAudioBroadcastSource() = default;
  BLEAudioBroadcastSource(const BLEAudioBroadcastSource &) = default;
  BLEAudioBroadcastSource &operator=(const BLEAudioBroadcastSource &) = default;

  /** @brief Whether this handle references a source (false when the factory failed). */
  explicit operator bool() const;

  // --- Configuration (applied by the next start()) ---

  /** @brief Codec and QoS from a BAP preset (broadcast table). Default LC3_16_2_1. */
  BLEAudioBroadcastSource &setPreset(BLEAudioCodecPreset preset);
  /**
   * @brief Replace the preset's QoS with a custom one.
   *
   * Without it, the preset QoS is used with the controller's presentation delay.
   */
  BLEAudioBroadcastSource &setQos(const BLEAudioQos &qos);
  /**
   * @brief Number of channels, one BIS each: 1 = mono (default), 2 = stereo (FL + FR).
   *
   * Clamped to 1..2 and capped by the packaged Kconfig BIS count. Ignored
   * while streaming. Streams are (re)allocated immediately, so attach stream
   * callbacks after calling it.
   */
  BLEAudioBroadcastSource &setChannels(uint8_t channels);
  /** @brief 24-bit Broadcast ID receivers use to recognise the source. Default: random. */
  BLEAudioBroadcastSource &setBroadcastId(uint32_t broadcastId);
  /**
   * @brief Broadcast Code (up to 16 characters) that encrypts the BIG.
   *
   * Empty (default) = unencrypted. Receivers need the same code to decode.
   */
  BLEAudioBroadcastSource &setBroadcastCode(const String &code);
  /** @brief Broadcast Name shown by scanners (truncated to 32 characters). Default: the device name. */
  BLEAudioBroadcastSource &setName(const String &name);
  /** @brief Streaming context in the BASE metadata. Default Media. */
  BLEAudioBroadcastSource &setContext(BLEAudioContext context);
  /**
   * @brief Advertise a Public Broadcast Announcement (Auracast, PBP builds only).
   *
   * Lets phones list the broadcast with its quality and encryption flags.
   * Default on; ignored when PBP is not compiled in.
   */
  BLEAudioBroadcastSource &setPublicBroadcast(bool enable);

  // --- Control (after audio.start()) ---

  /**
   * @brief Start advertising and create the BIG (non-blocking; onStarted() follows).
   *
   * Re-creates the source first when the configuration changed since the
   * last start().
   *
   * @param advInstance Extended advertising instance reserved for the broadcast.
   *                    Do not use it for anything else while broadcasting.
   * @return BTStatus::OK when started; InvalidState before audio.start(),
   *         while streaming or without streams; another error when creating
   *         the source or the advertising failed.
   */
  BTStatus start(uint8_t advInstance = 1);
  /**
   * @brief Terminate the BIG and stop the advertising instance (onStopped() follows).
   * @return BTStatus::OK on success; InvalidState when not started.
   */
  BTStatus stop();
  /** @brief Whether the BIG is up (between onStarted() and onStopped()). */
  bool isStreaming() const;
  /**
   * @brief Change the streaming context in the BASE while running.
   * @return BTStatus::OK on success; InvalidState before the first start().
   */
  BTStatus updateContext(BLEAudioContext context);
  /** @brief Broadcast ID in use (random unless set). */
  uint32_t getBroadcastId() const;

  // --- Streams (one Tx stream per BIS; valid after setChannels()) ---

  /** @brief Number of streams (= channels, fewer if the pool ran out). */
  size_t streamCount() const;
  /** @brief Stream @p index: 0 = mono or front left, 1 = front right. Empty when out of range. */
  BLEAudioStream stream(size_t index) const;

  // --- Callbacks (Bluetooth host task; keep them short) ---

  /** @brief The BIG is up: every stream is streaming. */
  BLEAudioBroadcastSource &onStarted(Callback cb);
  /** @brief The BIG was terminated. */
  BLEAudioBroadcastSource &onStopped(StoppedCallback cb);
  /** @brief Clear every callback of this role. */
  void resetCallbacks();

  struct Impl;

private:
  explicit BLEAudioBroadcastSource(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;

  friend class BLEAudio;
};

#endif /* BLE_AUDIO_SUPPORTED */
