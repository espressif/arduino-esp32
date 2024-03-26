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

#ifndef _dsps_corr_H_
#define _dsps_corr_H_
#include "dsp_err.h"

#include "dsps_conv_platform.h"

#ifdef __cplusplus
extern "C"
{
#endif


/**@{*/
/**
 * @brief   Correlation with pattern
 *
 * The function correlate input sigla array with pattern array.
 * The implementation use ANSI C and could be compiled and run on any platform
 *
 * @param[in] Signal: input array with signal values
 * @param[in] siglen: length of the signal array
 * @param[in] Pattern: input array with pattern values
 * @param[in] patlen: length of the pattern array. The siglen must be bigger then patlen!
 * @param dest: output array with result of correlation
 *
 * @return
 *      - ESP_OK on success
 *      - One of the error codes from DSP library (one of the input array are NULL, or if (siglen < patlen))
 */
esp_err_t dsps_corr_f32_ansi(const float *Signal, const int siglen, const float *Pattern, const int patlen, float *dest);
esp_err_t dsps_corr_f32_ae32(const float *Signal, const int siglen, const float *Pattern, const int patlen, float *dest);
/**@}*/

#ifdef __cplusplus
}
#endif


#ifdef CONFIG_DSP_OPTIMIZED
#if (dsps_corr_f32_ae32_enabled == 1)
#define dsps_corr_f32 dsps_corr_f32_ae32
#else
#define dsps_corr_f32 dsps_corr_f32_ansi
#endif // dsps_corr_f32_ae32_enabled
#else
#define dsps_corr_f32 dsps_corr_f32_ansi
#endif

#endif // _dsps_corr_H_
