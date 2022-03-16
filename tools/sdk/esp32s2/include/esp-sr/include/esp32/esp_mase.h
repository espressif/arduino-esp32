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

#ifndef _ESP_MASE_H_
#define _ESP_MASE_H_

#ifdef __cplusplus
extern "C" {
#endif

#define MASE_SAMPLE_RATE 16000        // Supports 16kHz only
#define MASE_FRAME_SIZE 16            // Supports 16ms only
#define MASE_MIC_DISTANCE 65          // According to physical design of mic-array

/**
 * @brief Sets mic-array type, currently 2-mic line array and 3-mic circular array 
 * are supported.
 */
typedef enum {
    TWO_MIC_LINE = 0,
    THREE_MIC_CIRCLE = 1
} mase_mic_array_type_t;

/**
 * @brief Sets operating mode, supporting normal mode and wake-up enhancement mode
 */
typedef enum {
    NORMAL_ENHANCEMENT_MODE = 0,
    WAKE_UP_ENHANCEMENT_MODE = 1
} mase_op_mode_t;

typedef void* mase_handle_t;

/**
 * @brief Creates an instance to the MASE structure.
 *
 * @param sample_rate       The sampling frequency (Hz) must be 16000.
 *
 * @param frame_size        The length of the audio processing must be 16ms.
 *
 * @param array_type        '0' for 2-mic line array and '1' for 3-mic circular array.
 *
 * @param mic_distance      The distance between neiboring microphones in mm.
 *
 * @param operating_mode	'0' for normal mode and '1' for wake-up enhanced mode.
 *
 * @param filter_strength	Strengh of the mic-array speech enhancement, must be 0, 1, 2 or 3.
 * 
 * @return
 *         - NULL: Create failed
 *         - Others: An instance of MASE
 */
mase_handle_t mase_create(int fs, int frame_size, int array_type, float mic_distance, int operating_mode, int filter_strength);

/**
 * @brief Performs mic array processing for one frame.
 *
 * @param inst        The instance of MASE.
 *
 * @param in          An array of 16-bit signed audio samples from mic.
 *
 * @param dsp_out     Returns enhanced signal.
 *
 * @return None
 *
 */
void mase_process(mase_handle_t st, int16_t *in, int16_t *dsp_out);

/**
 * @brief Free the MASE instance
 *
 * @param inst The instance of MASE.
 *
 * @return None
 *
 */
void mase_destory(mase_handle_t st);

#ifdef __cplusplus
}
#endif

#endif