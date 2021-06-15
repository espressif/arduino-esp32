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


#ifndef _dsps_biquad_H_
#define _dsps_biquad_H_

#include "dsp_err.h"

#include "dsps_add_platform.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**@{*/
/**
 * @brief   IIR filter
 *
 * IIR filter 2nd order direct form II (bi quad)
 * The extension (_ansi) use ANSI C and could be compiled and run on any platform.
 * The extension (_ae32) is optimized for ESP32 chip.
 *
 * @param[in] input: input array
 * @param output: output array
 * @param len: length of input and output vectors
 * @param coef: array of coefficients. b0,b1,b2,a1,a2
 *              expected that a0 = 1. b0..b2 - numerator, a0..a2 - denominator
 * @param w: delay line w0,w1. Length of 2.
 * @return
 *      - ESP_OK on success
 *      - One of the error codes from DSP library
 */
esp_err_t dsps_biquad_f32_ansi(const float *input, float *output, int len, float *coef, float *w);
esp_err_t dsps_biquad_f32_ae32(const float *input, float *output, int len, float *coef, float *w);
/**@}*/


#ifdef __cplusplus
}
#endif

#if CONFIG_DSP_OPTIMIZED
#if (dsps_biquad_f32_ae32_enabled == 1)
#define dsps_biquad_f32 dsps_biquad_f32_ae32
#else
#define dsps_biquad_f32 dsps_biquad_f32_ansi
#endif
#else // CONFIG_DSP_OPTIMIZED
#define dsps_biquad_f32 dsps_biquad_f32_ansi
#endif // CONFIG_DSP_OPTIMIZED


#endif // _dsps_biquad_H_