// Copyright 2018-2019 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _dsps_sub_H_
#define _dsps_sub_H_
#include "dsp_err.h"

#include "dsps_sub_platform.h"

#ifdef __cplusplus
extern "C"
{
#endif


/**@{*/
/**
 * @brief   sub arrays
 *
 * The function subtract one array from another
 * out[i*step_out] = input1[i*step1] - input2[i*step2]; i=[0..len)
 * The implementation use ANSI C and could be compiled and run on any platform
 *
 * @param[in] input1: input array 1
 * @param[in] input2: input array 2
 * @param output: output array
 * @param len: amount of operations for arrays
 * @param step1: step over input array 1 (by default should be 1)
 * @param step2: step over input array 2 (by default should be 1)
 * @param step_out: step over output array (by default should be 1)
 *
 * @return
 *      - ESP_OK on success
 *      - One of the error codes from DSP library
 */
esp_err_t dsps_sub_f32_ansi(const float *input1, const float *input2, float *output, int len, int step1, int step2, int step_out);
esp_err_t dsps_sub_f32_ae32(const float *input1, const float *input2, float *output, int len, int step1, int step2, int step_out);

esp_err_t dsps_sub_s16_ansi(const int16_t *input1, const int16_t *input2, int16_t *output, int len, int step1, int step2, int step_out, int shift);
esp_err_t dsps_sub_s16_ae32(const int16_t *input1, const int16_t *input2, int16_t *output, int len, int step1, int step2, int step_out, int shift);
esp_err_t dsps_sub_s16_aes3(const int16_t *input1, const int16_t *input2, int16_t *output, int len, int step1, int step2, int step_out, int shift);

esp_err_t dsps_sub_s8_ansi(const int8_t *input1, const int8_t *input2, int8_t *output, int len, int step1, int step2, int step_out, int shift);
esp_err_t dsps_sub_s8_aes3(const int8_t *input1, const int8_t *input2, int8_t *output, int len, int step1, int step2, int step_out, int shift);
/**@}*/

#ifdef __cplusplus
}
#endif

#if CONFIG_DSP_OPTIMIZED

#if (dsps_sub_f32_ae32_enabled == 1)
#define dsps_sub_f32 dsps_sub_f32_ae32
#else
#define dsps_sub_f32 dsps_sub_f32_ansi
#endif

#if (dsps_sub_s16_aes3_enabled == 1)
#define dsps_sub_s16 dsps_sub_s16_aes3
#define dsps_sub_s8 dsps_sub_s8_aes3
#elif (dsps_sub_s16_ae32_enabled == 1)
#define dsps_sub_s16 dsps_sub_s16_ae32
#define dsps_sub_s8 dsps_sub_s8_ansi
#else
#define dsps_sub_s16 dsps_sub_s16_ansi
#define dsps_sub_s8 dsps_sub_s8_ansi
#endif

#else // CONFIG_DSP_OPTIMIZED
#define dsps_sub_f32 dsps_sub_f32_ansi
#define dsps_sub_s16 dsps_sub_s16_ansi
#define dsps_sub_s8  dsps_sub_s8_ansi
#endif // CONFIG_DSP_OPTIMIZED

#endif // _dsps_sub_H_
