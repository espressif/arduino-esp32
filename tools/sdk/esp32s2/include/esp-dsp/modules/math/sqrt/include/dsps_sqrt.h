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

#ifndef _dsps_sqrt_H_
#define _dsps_sqrt_H_
#include "dsp_err.h"


#ifdef __cplusplus
extern "C"
{
#endif

/**@{*/
/**
 * @brief   square root approximation
 *
 * The function takes square root approximation
 * x[i] ~ sqrt(y[i]); i=[0..len)
 * The implementation use ANSI C and could be compiled and run on any platform
 *
 * @param[in] input: input array
 * @param output: output array
 * @param len: amount of operations for arrays
 *
 * @return
 *      - ESP_OK on success
 *      - One of the error codes from DSP library
 */
esp_err_t dsps_sqrt_f32_ansi(const float *input, float *output, int len);
//esp_err_t dsps_sqrt_s32_ansi(const int32_t *input, int16_t *output, int len);

/**@{*/
/**
 * @brief   square root approximation
 *
 * The function takes square root approximation
 * x ~ sqrt(y);
 * The implementation use ANSI C and could be compiled and run on any platform
 *
 * @param[in] data: input value
 *
 * @return
 *      - square root value
 */
float dsps_sqrtf_f32_ansi(const float data);


/**@{*/
/**
 * @brief   inverted square root approximation
 *
 * The function takes inverted square root approximation
 * x ~ 1/sqrt(y);
 * The implementation use ANSI C and could be compiled and run on any platform
 *
 * @param[in] data: input value
 *
 * @return
 *      - inverted square root value
 */
float dsps_inverted_sqrtf_f32_ansi(float data );
/**@}*/

#ifdef __cplusplus
}
#endif


#ifdef CONFIG_DSP_OPTIMIZED
#define dsps_sqrt_f32 dsps_sqrt_f32_ansi
#define dsps_sqrtf_f32 dsps_sqrtf_f32_ansi
#define dsps_inverted_sqrtf_f32 dsps_inverted_sqrtf_f32_ansi
#else
#define dsps_sqrt_f32 dsps_sqrt_f32_ansi
#define dsps_sqrtf_f32 dsps_sqrtf_f32_ansi
#define dsps_inverted_sqrtf_f32 dsps_inverted_sqrtf_f32_ansi
#endif

#endif // _dsps_sqrt_H_