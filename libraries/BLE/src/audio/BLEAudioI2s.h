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
 * @brief PCM endpoint types of BLEAudioPlayer / BLEAudioRecorder.
 *
 * Pins are plain GPIO numbers so this header never names an I2S driver type.
 */

#include "core/BLEGuards.h"
#if BLE_AUDIO_LC3_SUPPORTED

#include <functional>
#include <cstdint>
#include <cstddef>

/** Use for any I2S GPIO field that is not wired. */
static constexpr int BLE_AUDIO_I2S_GPIO_UNUSED = -1;

/**
 * @brief Standard-mode (Philips) 16-bit I2S master pin map.
 *
 * The sample rate follows the stream's codec configuration. A player drives
 * @ref dout (always stereo; mono streams go to both slots), a recorder reads
 * @ref din (stereo when it encodes two channels, else the left slot).
 */
struct BLEAudioI2sConfig {
  int bclk = BLE_AUDIO_I2S_GPIO_UNUSED;  ///< Bit clock (SCLK) GPIO.
  int ws = BLE_AUDIO_I2S_GPIO_UNUSED;    ///< Word select (LRCLK/WS) GPIO.
  int dout = BLE_AUDIO_I2S_GPIO_UNUSED;  ///< Data out GPIO (player -> DAC).
  int din = BLE_AUDIO_I2S_GPIO_UNUSED;   ///< Data in GPIO (recorder <- ADC/mic).
  int mclk = BLE_AUDIO_I2S_GPIO_UNUSED;  ///< Master clock GPIO (optional).
  int port = 0;                          ///< I2S peripheral port.
};

/**
 * @brief Consumer of decoded PCM, called on the codec task once per LC3 frame.
 * @param pcm     Interleaved 16-bit PCM with `channels()` channels.
 * @param samples Total samples (frames x channels).
 */
using BLEAudioPcmSink = std::function<void(const int16_t *pcm, size_t samples)>;

/**
 * @brief Producer of PCM to encode, called on the codec task once per LC3 frame.
 * @param pcm     Fill with interleaved 16-bit PCM with `channels()` channels.
 * @param samples Total samples wanted (frames x channels).
 * @return Samples produced; the rest of the frame is padded with silence.
 */
using BLEAudioPcmSource = std::function<size_t(int16_t *pcm, size_t samples)>;

#endif /* BLE_AUDIO_LC3_SUPPORTED */
