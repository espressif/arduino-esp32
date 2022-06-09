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
#ifndef _ESP_MAP_H_
#define _ESP_MAP_H_

#ifdef __cplusplus
extern "C" {
#endif

#define MAP_SAMPLE_RATE 16000        // Supports 16kHz only
#define MAP_FRAME_SIZE 16            // Supports 16ms only
#define MAP_MIC_DISTANCE 50          // According to physical design of mic-array
#define MAP_AEC_ON true              
#define MAP_AEC_OFF false            
#define MAP_AEC_FILTER_LENGTH   1200 // Number of samples of echo to cancel

/**
 * @brief Sets mic-array type, currently 2-mic line array and 3-mic circular array 
 * are supported.
 */
typedef enum {
    TWO_MIC_LINE = 0,
    THREE_MIC_CIRCLE = 1
} map_mic_array_type_t;

typedef void* mic_array_processor_t;

/**
 * @brief Creates an instance to the MAP structure.
 *
 * @param sample_rate       The sampling frequency (Hz) must be 16000.
 *
 * @param frame_size        The length of the audio processing must be 16ms.
 *
 * @param array_type        '0' for 2-mic line array and '1' for 3-mic circular array.
 *
 * @param mic_distance      The distance between neiboring microphones in mm.
 * 
 * @param aec_on            Decides whether to turn on AEC.
 *
 * @param filter_length     Number of samples of echo to cancel, effective when AEC is on.
 *
 * @return
 *         - NULL: Create failed
 *         - Others: An instance of MAP
 */
mic_array_processor_t map_create(int fs, int frame_size, int array_type, float mic_distance, bool aec_on, int filter_length);

/**
 * @brief Performs mic array processing for one frame.
 *
 * @param inst        The instance of MAP.
 *
 * @param in          An array of 16-bit signed audio samples from mic.
 *
 * @param far_end     An array of 16-bit signed audio samples sent to the speaker, can be none when AEC is turned off.
 *
 * @param dsp_out     Returns enhanced signal.
 *
 * @return None
 *
 */
void map_process(mic_array_processor_t st, int16_t *in, int16_t *far_end, int16_t *dsp_out);

/**
 * @brief Free the MAP instance
 *
 * @param inst The instance of MAP.
 *
 * @return None
 *
 */
void map_destory(mic_array_processor_t st);

#ifdef __cplusplus
}
#endif

#endif