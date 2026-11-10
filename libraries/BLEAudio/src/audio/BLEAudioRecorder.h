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
 * you and `start()` spawns the encode task. While the streams are streaming,
 * one SDU per SDU interval is encoded and written, paced by a microsecond
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
#include "audio/BLEAudioStream.h"
#include "audio/BLEAudioI2s.h"

class BLEAudioRecorder {
public:
  BLEAudioRecorder();
  ~BLEAudioRecorder() = default;
  BLEAudioRecorder(const BLEAudioRecorder &) = default;
  BLEAudioRecorder &operator=(const BLEAudioRecorder &) = default;

  /** @brief True between a successful begin() and end(). */
  explicit operator bool() const;

  /**
   * @brief Select the input: an I2S mic/ADC (needs bclk, ws, din) unless a PCM source is set.
   * @return InvalidParam when the pins are missing, InvalidState while started.
   */
  BTStatus begin(const BLEAudioI2sConfig &i2s = BLEAudioI2sConfig());
  /** @brief Stop, detach and release everything. */
  void end();

  /** @brief Take PCM from @p source instead of I2S (before begin()). */
  BLEAudioRecorder &setPcmSource(BLEAudioPcmSource source);

  /** @brief Feed one Tx stream (1 or 2 channels) or two mono Tx streams (left, right). */
  BTStatus attach(BLEAudioStream stream, BLEAudioStream right = BLEAudioStream());
  /** @brief Start the encode task; sending follows the attached streams' start/stop. */
  BTStatus start();
  /** @brief Stop the encode task; attachments are kept, so start() resumes. */
  void stop();
  /** @return true while the encode task runs. */
  bool isRunning() const;

  /** @brief PCM channels taken from the source/I2S; 0 while idle. */
  uint8_t channels() const;
  /** @brief Current sample rate in Hz; 0 while idle. */
  uint32_t sampleRate() const;

  // --- Statistics (reset by start()) ---

  /** @brief SDUs accepted by the stack (counted per stream). */
  uint32_t sdusSent() const;
  /** @brief SDUs that failed to encode or were refused by the stack. */
  uint32_t sendErrors() const;

  struct Impl;

private:
  std::shared_ptr<Impl> _impl;
};

#endif /* BLE_AUDIO_LC3_SUPPORTED */
