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
 * @brief PCM endpoint callbacks of BLEAudioPlayer / BLEAudioRecorder.
 *
 * These callbacks let an application use any PCM transport (for example the
 * ESP_I2S I2SClass, an ADC/DAC driver, or a synthetic source) instead of the
 * built-in I2S port.
 */

#include "core/BLEGuards.h"
#if BLE_AUDIO_LC3_SUPPORTED

#include <functional>
#include <cstdint>
#include <cstddef>

/**
 * @brief Consumer of decoded PCM, called on the codec task once per LC3 frame.
 *
 * Real-time budget: return within one frame interval (7.5/10 ms nominal,
 * 8.16/10.88 ms at 44.1 kHz); overruns stall the output and count as
 * underruns. Must not block on BLE, call attach()/start()/stop()/end(), or
 * touch stream taps (use loop() for teardown). Reentrant-safe: it runs on
 * the codec task, never on the BT host or Arduino loop tasks.
 *
 * @param pcm     Interleaved 16-bit PCM with `channels()` channels.
 * @param samples Total samples (frames x channels).
 */
using BLEAudioPCMSink = std::function<void(const int16_t *pcm, size_t samples)>;

/**
 * @brief Producer of PCM to encode, called on the codec task once per LC3 frame.
 *
 * Same real-time budget and restrictions as the sink: fill fast, never block
 * on BLE or the data path. A short return is padded with silence.
 *
 * @param pcm     Fill with interleaved 16-bit PCM with `channels()` channels.
 * @param samples Total samples wanted (frames x channels).
 * @return Samples produced; the rest of the frame is padded with silence.
 */
using BLEAudioPCMSource = std::function<size_t(int16_t *pcm, size_t samples)>;

#endif /* BLE_AUDIO_LC3_SUPPORTED */
