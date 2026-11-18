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
 * you and `startPlayback()` spawns the decode task. Each time the streams
 * start, the player sizes its jitter buffer from the negotiated QoS (presentation delay /
 * SDU interval, at least 2 SDUs), prefills it and plays; lost, invalid or late
 * SDUs are concealed with LC3 PLC. Stereo comes from one stream carrying two
 * channels or from two mono streams (`attach(left, right)`). I2S writes pace
 * the task; a PCM sink is paced to the stream's real-time frame rate instead.
 *
 * Only available when the LC3 codec is compiled in (BLE_AUDIO_LC3_SUPPORTED).
 */

#include "core/BLEGuards.h"
#if BLE_AUDIO_LC3_SUPPORTED

#include <memory>
#include <cstdint>
#include "BTStatus.h"
#include "BLEAudioPCM.h"
#include "BLEAudioStream.h"

class BLEAudioPlayer {
public:
  BLEAudioPlayer();
  ~BLEAudioPlayer() = default;
  BLEAudioPlayer(const BLEAudioPlayer &) = default;
  BLEAudioPlayer &operator=(const BLEAudioPlayer &) = default;
  BLEAudioPlayer(BLEAudioPlayer &&) = default;
  BLEAudioPlayer &operator=(BLEAudioPlayer &&) = default;

  /** @brief True between a successful begin() and end(). */
  explicit operator bool() const;

  /**
   * @brief Use the built-in I2S output, or no I2S when a PCM sink is set.
   *
   * With no pins, this is only valid after setPCMSink(): the player then
   * delivers frames to the callback and never opens an I2S port. For I2S,
   * use begin(bclk, ws, dout) instead.
   *
   * @return InvalidParam without a PCM sink or I2S pins, InvalidState while
   *         playback is running.
   */
  BTStatus begin();
  /**
   * @brief Select the I2S output pins.
   *
   * @param bclk Bit clock (SCLK) GPIO.
   * @param ws   Word select (LRCLK/WS) GPIO.
   * @param dout Data out GPIO (player -> DAC).
   * @param mclk Master clock GPIO, or -1 when not wired.
   * @param port I2S peripheral port (default 0).
   * @return InvalidParam when a required pin is missing, InvalidState while
   *         playback is running.
   */
  BTStatus begin(int bclk, int ws, int dout, int mclk = -1, int port = 0);
  /**
   * @brief Stop, detach and release everything.
   *
   * Called from a PCM sink callback (codec task), the release is deferred:
   * the task is asked to stop and end() must be called again from loop() to
   * free the Impl. This avoids destroying the codec task from inside itself.
   */
  void end();

  /** @brief Deliver decoded PCM to @p sink instead of I2S (before begin(); InvalidState once begun/running).
   *
   * A set sink wins over I2S: begin(pins) then only logs and keeps the sink.
   * @return BTStatus::OK when stored; InvalidState when begun or running.
   */
  BTStatus setPCMSink(BLEAudioPCMSink sink);
  /** @brief Jitter-buffer depth in SDUs (min 2, max 16); 0 = from the stream QoS (default).
   *
   * Larger requests are clamped to 16 with a warning when the session builds;
   * a presentation delay that needs more than 16 SDUs underruns by design.
   * @return BTStatus::OK when stored; InvalidState while running.
   */
  BTStatus setJitterDepth(uint8_t sdus);

  /**
   * @brief Play one Rx stream (mono or interleaved stereo).
   *
   * One argument: the stream carries mono or interleaved stereo (1 or 2
   * channels). Use the two-argument overload for two separate mono streams.
   */
  BTStatus attach(BLEAudioStream stream);
  /**
   * @brief Play two separate mono Rx streams (left, right).
   *
   * Both handles must be non-empty, carry exactly one channel and describe
   * the same codec/QoS; otherwise the data path cannot pair them.
   */
  BTStatus attach(BLEAudioStream stream, BLEAudioStream right);
  /**
   * @brief Start the decode task; playback follows the attached streams' start/stop.
   *
   * This is not BLEAudio::begin(): the controller commits the roles and GATT
   * table, while this method starts the LC3 task that consumes the streams.
   */
  BTStatus startPlayback();
  /** @brief Stop the decode task; attachments are kept, so startPlayback() resumes. */
  void stopPlayback();
  /** @return true while the decode task runs. */
  bool isRunning() const;

  /** @brief PCM channels handed to the sink (I2S is always stereo); 0 while idle. */
  uint8_t channels() const;
  /** @brief Current sample rate in Hz; 0 while idle. */
  uint32_t sampleRate() const;

  // --- Statistics (reset by startPlayback()) ---

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
  /** @brief PCM-sink timeline restarts after falling behind (tick quantization, no data lost). */
  uint32_t lateResyncs() const;

  struct Impl;

private:
  std::shared_ptr<Impl> _impl;
};

#endif /* BLE_AUDIO_LC3_SUPPORTED */
