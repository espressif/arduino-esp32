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

#ifndef _DSP_SNR_H_
#define _DSP_SNR_H_

#include "dsp_err.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief   SNR
 *
 * The function calculates signal to noise ration in case if signal is sine tone.
 * The function makes FFT of the input, then search a spectrum maximum, and then calculated
 * SNR as sum of all harmonics to the maximum value.
 * This function have to be used for debug and unit tests only. It's not optimized for real-time processing.
 * The implementation use ANSI C and could be compiled and run on any platform
 *
 * @param input: input array.
 * @param len: length of the input signal
 * @param use_dc: this parameter define will be DC value used for calculation or not.
 *                0 - SNR will not include DC power
 *                1 - SNR will include DC power
 *
 * @return
 *      - SNR in dB
 */
float dsps_snr_f32(const float *input, int32_t len, uint8_t use_dc);
float dsps_snr_fc32(const float *input, int32_t len);


#ifdef __cplusplus
}
#endif

#endif // _DSP_SNR_H_
