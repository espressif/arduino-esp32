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

#ifndef _dsps_conv_H_
#define _dsps_conv_H_
#include "dsp_err.h"

#include "dsps_conv_platform.h"

#ifdef __cplusplus
extern "C"
{
#endif


/**@{*/
/**
 * @brief   Convolution
 *
 * The function convolve Signal array with Kernel array.
 * The implementation use ANSI C and could be compiled and run on any platform
 *
 * @param[in] Signal:  input array with signal
 * @param[in] siglen:  length of the input signal
 * @param[in] Kernel:  input array with convolution kernel
 * @param[in] kernlen: length of the Kernel array
 * @param convout: output array with convolution result length of (siglen + Kernel -1)
 *
 * @return
 *      - ESP_OK on success
 *      - One of the error codes from DSP library
 */
esp_err_t dsps_conv_f32_ae32(const float *Signal, const int siglen, const float *Kernel, const int kernlen, float *convout);
esp_err_t dsps_conv_f32_ansi(const float *Signal, const int siglen, const float *Kernel, const int kernlen, float *convout);
/**@}*/

#ifdef __cplusplus
}
#endif


#ifdef CONFIG_DSP_OPTIMIZED

#if (dsps_conv_f32_ae32_enabled == 1)
#define dsps_conv_f32 dsps_conv_f32_ae32
#else
#define dsps_conv_f32 dsps_conv_f32_ansi
#endif // dsps_conv_f32_ae32_enabled

#else
#define dsps_conv_f32 dsps_conv_f32_ansi
#endif

#endif // _dsps_conv_H_
