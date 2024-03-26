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

#ifndef _dsps_dct_H_
#define _dsps_dct_H_
#include "dsp_err.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**@{*/
/**
 * @brief      DCT of radix 2, unscaled
 *
 * DCT type II of radix 2, unscaled
 * Function is FFT based
 * The extension (_ansi) use ANSI C and could be compiled and run on any platform.
 * The extension (_ae32) is optimized for ESP32 chip.
 *
 * @param[inout] data: input/output array with size of N*2. An elements located: Re[0],Re[1], , ... Re[N-1], any data... up to N*2
 *               result of DCT will be stored to this array from 0...N-1.
 *               Size of data array must be N*2!!!
 * @param[in] N: Size of DCT transform. Size of data array must be N*2!!!
 *
 * @return
 *      - ESP_OK on success
 *      - One of the error codes from DSP library
 */
esp_err_t dsps_dct_f32(float *data, int N);

/**@}*/

/**@{*/
/**
 * @brief      Inverce DCT of radix 2
 *
 * Inverce DCT type III of radix 2, unscaled
 * Function is FFT based
 * The extension (_ansi) use ANSI C and could be compiled and run on any platform.
 * The extension (_ae32) is optimized for ESP32 chip.
 *
 * @param[inout] data: input/output array with size of N*2. An elements located: Re[0],Re[1], , ... Re[N-1], any data... up to N*2
 *               result of DCT will be stored to this array from 0...N-1.
 *               Size of data array must be N*2!!!
 * @param[in] N: Size of DCT transform. Size of data array must be N*2!!!
 *
 * @return
 *      - ESP_OK on success
 *      - One of the error codes from DSP library
 */
esp_err_t dsps_dct_inv_f32(float *data, int N);

/**@}*/

/**@{*/
/**
 * @brief      DCTs
 *
 * Direct DCT type II and Inverce DCT type III, unscaled
 * These functions used as a reference for general purpose. These functions are not optimyzed!
 * The extension (_ansi) use ANSI C and could be compiled and run on any platform.
 * The extension (_ae32) is optimized for ESP32 chip.
 *
 * @param[in] data: input/output array with size of N. An elements located: Re[0],Re[1], , ... Re[N-1]
 * @param[in] N: Size of DCT transform. Size of data array must be N*2!!!
 * @param[out] result: output result array with size of N.
 *
 * @return
 *      - ESP_OK on success
 *      - One of the error codes from DSP library
 */
esp_err_t dsps_dct_f32_ref(float *data, int N, float *result);
esp_err_t dsps_dct_inverce_f32_ref(float *data, int N, float *result);
/**@}*/


#ifdef __cplusplus
}
#endif

#endif // _dsps_dct_H_
