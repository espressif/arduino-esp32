// Copyright 2015-2019 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License
#ifndef _ESP_AEC_H_
#define _ESP_AEC_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define USE_AEC_FFT                      // Not kiss_fft
#define AEC_USE_SPIRAM      0
#define AEC_SAMPLE_RATE     16000        // Only Support 16000Hz
#define AEC_FRAME_LENGTH_MS 16
#define AEC_FILTER_LENGTH   1200         // Number of samples of echo to cancel

typedef void* aec_handle_t;

/**
 * @brief Creates an instance to the AEC structure.
 * 
 * @deprecated This API will be deprecated after version 1.0, please use aec_pro_create
 *
 * @param sample_rate       The Sampling frequency (Hz) must be 16000.
 *
 * @param frame_length      The length of the audio processing must be 16ms.
 *
 * @param filter_length     Number of samples of echo to cancel.
 *
 * @return
 *         - NULL: Create failed
 *         - Others: The instance of AEC
 */
aec_handle_t aec_create(int sample_rate, int frame_length, int filter_length);

/**
 * @brief Creates an instance to the AEC structure.
 * 
 * @deprecated This API will be deprecated after version 1.0, please use aec_pro_create
 *
 * @param sample_rate       The Sampling frequency (Hz) must be 16000.
 *
 * @param frame_length      The length of the audio processing must be 16ms.
 *
 * @param filter_length     Number of samples of echo to cancel.
 * 
 * @param nch               Number of input signal channel.
 *
 * @return
 *         - NULL: Create failed
 *         - Others: The instance of AEC
 */
aec_handle_t aec_create_multimic(int sample_rate, int frame_length, int filter_length, int nch);

/**
 * @brief Creates an instance of more powerful AEC.
 *
 * @param frame_length      Length of input signal. Must be 16ms if mode is 0; otherwise could be 16ms or 32ms. Length of input signal to aec_process must be modified accordingly.
 *
 * @param nch               Number of microphones.
 *
 * @param mode              Mode of AEC (0 to 5), indicating aggressiveness and RAM allocation. 0: mild; 1 or 2: medium (1: internal RAM, 2: SPIRAM); 3 and 4: aggressive (3: internal RAM, 4: SPIRAM); 5: agressive, accelerated for ESP32-S3.
 *
 * @return
 *         - NULL: Create failed
 *         - Others: An Instance of AEC
 */
aec_handle_t aec_pro_create(int frame_length, int nch, int mode);

/**
 * @brief Performs echo cancellation a frame, based on the audio sent to the speaker and frame from mic.
 *
 * @param inst        The instance of AEC.
 *
 * @param indata      An array of 16-bit signed audio samples from mic.
 *
 * @param refdata     An array of 16-bit signed audio samples sent to the speaker.
 *
 * @param outdata     Returns near-end signal with echo removed.
 *
 * @return None
 *
 */
void aec_process(const aec_handle_t inst, int16_t *indata, int16_t *refdata, int16_t *outdata);

/**
 * @brief Free the AEC instance
 *
 * @param inst The instance of AEC.
 *
 * @return None
 *
 */
void aec_destroy(aec_handle_t inst);

#ifdef __cplusplus
}
#endif

#endif //_ESP_AEC_H_
