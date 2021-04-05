// Copyright 2018-2020 Espressif Systems (Shanghai) PTE LTD
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

#ifndef _dsps_ccorr_H_
#define _dsps_ccorr_H_
#include "dsp_err.h"

#include "dsps_conv_platform.h"

#ifdef __cplusplus
extern "C"
{
#endif


/**@{*/
/**
 * @brief   Cross correlation
 *
 * The function make cross correlate between two ignals.
 * The implementation use ANSI C and could be compiled and run on any platform
 *
 * @param[in] Signal1: input array with input 1 signal values
 * @param[in] siglen1: length of the input 1 signal array
 * @param[in] Signal2: input array with input 2 signal values
 * @param[in] siglen2: length of the input  signal array
 * @param corrout: output array with result of cross correlation. The size of dest array must be (siglen1 + siglen2 - 1) !!!
 *
 * @return
 *      - ESP_OK on success
 *      - One of the error codes from DSP library (one of the input array are NULL, or if (siglen < patlen))
 */
esp_err_t dsps_ccorr_f32_ansi(const float *Signal, const int siglen, const float *Pattern, const int patlen, float *corrout);
esp_err_t dsps_ccorr_f32_ae32(const float *Signal, const int siglen, const float *Pattern, const int patlen, float *corrout);
/**}@*/

#ifdef __cplusplus
}
#endif


#ifdef CONFIG_DSP_OPTIMIZED
#if (dsps_ccorr_f32_ae32_enabled == 1)
#define dsps_ccorr_f32 dsps_ccorr_f32_ae32
#else
#define dsps_ccorr_f32 dsps_ccorr_f32_ansi
#endif // dsps_ccorr_f32_ae32_enabled
#else
#define dsps_ccorr_f32 dsps_ccorr_f32_ansi
#endif

#endif // _dsps_conv_H_