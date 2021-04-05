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

#ifndef _dsps_sfdr_H_
#define _dsps_sfdr_H_


#include "dsp_err.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief   SFDR
 * 
 * The function calculates Spurious-Free Dynamic Range.
 * The function makes FFT of the input, then search a spectrum maximum, and then compare
 * maximum value with all others. Result calculated as minimum value.
 * This function have to be used for debug and unit tests only. It's not optimized for real-time processing.
 * The implementation use ANSI C and could be compiled and run on any platform
 *
 * @param[in] input: input array.
 * @param len: length of the input signal
 * @param use_dc: this parameter define will be DC value used for calculation or not.
 *                0 - SNR will not include DC power
 *                1 - SNR will include DC power
 *
 * @return
 *      - SFDR in DB
 */
float dsps_sfdr_f32(const float *input, int32_t len, int8_t use_dc);
float dsps_sfdr_fc32(const float *input, int32_t len);

#ifdef __cplusplus
}
#endif

#endif // _dsps_sfdr_H_