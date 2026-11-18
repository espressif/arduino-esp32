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
 * @brief LC3 capture: I2S mic/ADC (or a PCM source) -> LC3 encode -> Tx BLEAudioStream(s).
 *
 * `begin()` sets the input, `attach()` binds the Tx stream(s) a role handed
 * you and `startCapture()` spawns the encode task. While the streams are
 * streaming, one SDU per SDU interval is encoded and written, paced by a microsecond
 * timer and kept in step with the controller through its SDU-completion
 * events. Stereo goes to one stream carrying two channels or to two mono
 * streams (`attach(left, right)`).
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

class BLEAudioRecorder {
public:
  BLEAudioRecorder();
  ~BLEAudioRecorder() = default;
  BLEAudioRecorder(const BLEAudioRecorder &) = default;
  BLEAudioRecorder &operator=(const BLEAudioRecorder &) = default;
  BLEAudioRecorder(BLEAudioRecorder &&) = default;
  BLEAudioRecorder &operator=(BLEAudioRecorder &&) = default;

  /** @brief True between a successful begin() and end(). */
  explicit operator bool() const;

  /**
   * @brief Use the built-in I2S input, or no I2S when a PCM source is set.
   *
   * With no pins, this is only valid after setPCMSource(): the recorder then
   * takes frames from the callback and never opens an I2S port. For I2S,
   * use begin(bclk, ws, din) instead.
   *
   * @return InvalidParam without a PCM source or I2S pins, InvalidState while
   *         capture is running.
   */
  BTStatus begin();
  /**
   * @brief Select the I2S input pins.
   *
   * @param bclk Bit clock (SCLK) GPIO.
   * @param ws   Word select (LRCLK/WS) GPIO.
   * @param din  Data in GPIO (recorder <- ADC/mic).
   * @param mclk Master clock GPIO, or -1 when not wired.
   * @param port I2S peripheral port (default 0).
   * @return InvalidParam when a required pin is missing, InvalidState while
   *         capture is running.
   */
  BTStatus begin(int bclk, int ws, int din, int mclk = -1, int port = 0);
  /**
   * @brief Stop, detach and release everything.
   *
   * Called from a PCM source callback (codec task), the release is deferred:
   * the task is asked to stop and end() must be called again from loop() to
   * free the Impl and its timer. This avoids destroying the codec task from
   * inside itself.
   */
  void end();

  /** @brief Take PCM from @p source instead of I2S (before begin(); InvalidState once begun/running).
   * @return BTStatus::OK when stored; InvalidState when begun or running. */
  BTStatus setPCMSource(BLEAudioPCMSource source);

  /**
   * @brief Feed one Tx stream (mono or interleaved stereo).
   *
   * One argument: the stream carries mono or interleaved stereo (1 or 2
   * channels). Use the two-argument overload for two separate mono streams.
   */
  BTStatus attach(BLEAudioStream stream);
  /**
   * @brief Feed two separate mono Tx streams (left, right).
   *
   * Both handles must be non-empty, carry exactly one channel and describe
   * the same codec/QoS; otherwise the data path cannot pair them.
   */
  BTStatus attach(BLEAudioStream stream, BLEAudioStream right);
  /**
   * @brief Start the encode task; sending follows the attached streams' start/stop.
   *
   * This is not BLEAudio::begin(): the controller commits the roles and GATT
   * table, while this method starts the LC3 task that produces the streams.
   */
  BTStatus startCapture();
  /** @brief Stop the encode task; attachments are kept, so startCapture() resumes. */
  void stopCapture();
  /** @return true while the encode task runs. */
  bool isRunning() const;

  /** @brief PCM channels taken from the source/I2S; 0 while idle. */
  uint8_t channels() const;
  /** @brief Current sample rate in Hz; 0 while idle. */
  uint32_t sampleRate() const;

  // --- Statistics (reset by startCapture()) ---

  /** @brief SDUs accepted by the stack (counted per stream). */
  uint32_t sdusSent() const;
  /** @brief SDUs that failed to encode or were refused by the stack. */
  uint32_t sendErrors() const;
  /** @brief Resync episodes after the controller stopped releasing SDUs (TX stall). */
  uint32_t txStalls() const;

  struct Impl;

private:
  std::shared_ptr<Impl> _impl;
};

#endif /* BLE_AUDIO_LC3_SUPPORTED */
