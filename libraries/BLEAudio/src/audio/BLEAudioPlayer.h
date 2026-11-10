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
 * @brief LC3 playback: Rx BLEAudioStream(s) -> LC3 decode -> I2S DAC (or a PCM sink).
 *
 * `begin()` sets the output, `attach()` binds the Rx stream(s) a role handed
 * you and `start()` spawns the decode task. Each time the streams start, the
 * player sizes its jitter buffer from the negotiated QoS (presentation delay /
 * SDU interval, at least 2 SDUs), prefills it and plays; lost, invalid or late
 * SDUs are concealed with LC3 PLC. Stereo comes from one stream carrying two
 * channels or from two mono streams (`attach(left, right)`).
 *
 * Only available when the LC3 codec is compiled in (BLE_AUDIO_LC3_SUPPORTED).
 */

#include "core/BLEGuards.h"
#if BLE_AUDIO_LC3_SUPPORTED

#include <memory>
#include <cstdint>
#include "BTStatus.h"
#include "audio/BLEAudioStream.h"
#include "audio/BLEAudioI2s.h"

class BLEAudioPlayer {
public:
  BLEAudioPlayer();
  ~BLEAudioPlayer() = default;
  BLEAudioPlayer(const BLEAudioPlayer &) = default;
  BLEAudioPlayer &operator=(const BLEAudioPlayer &) = default;

  /** @brief True between a successful begin() and end(). */
  explicit operator bool() const;

  /**
   * @brief Select the output: an I2S DAC (needs bclk, ws, dout) unless a PCM sink is set.
   * @return InvalidParam when the pins are missing, InvalidState while started.
   */
  BTStatus begin(const BLEAudioI2sConfig &i2s = BLEAudioI2sConfig());
  /** @brief Stop, detach and release everything. */
  void end();

  /** @brief Deliver decoded PCM to @p sink instead of I2S (before begin()). */
  BLEAudioPlayer &setPcmSink(BLEAudioPcmSink sink);
  /** @brief Jitter-buffer depth in SDUs (min 2); 0 = from the stream QoS (default). */
  BLEAudioPlayer &setJitterDepth(uint8_t sdus);

  /** @brief Play one Rx stream (1 or 2 channels) or two mono Rx streams (left, right). */
  BTStatus attach(BLEAudioStream stream, BLEAudioStream right = BLEAudioStream());
  /** @brief Start the decode task; playback follows the attached streams' start/stop. */
  BTStatus start();
  /** @brief Stop the decode task; attachments are kept, so start() resumes. */
  void stop();
  /** @return true while the decode task runs. */
  bool isRunning() const;

  /** @brief PCM channels handed to the sink (I2S is always stereo); 0 while idle. */
  uint8_t channels() const;
  /** @brief Current sample rate in Hz; 0 while idle. */
  uint32_t sampleRate() const;

  // --- Statistics (reset by start()) ---

  /** @brief SDUs delivered by the stack, whatever their status. */
  uint32_t sdusReceived() const;
  /** @brief SDUs discarded because the jitter buffer was full. */
  uint32_t sdusDropped() const;
  /** @brief SDUs flagged lost/invalid by the controller or with an unexpected length. */
  uint32_t sdusLost() const;
  /** @brief LC3 frames (per channel) produced by packet-loss concealment. */
  uint32_t plcFrames() const;
  /** @brief SDU intervals in which a streaming stream had nothing queued. */
  uint32_t underruns() const;

  struct Impl;

private:
  std::shared_ptr<Impl> _impl;
};

#endif /* BLE_AUDIO_LC3_SUPPORTED */
