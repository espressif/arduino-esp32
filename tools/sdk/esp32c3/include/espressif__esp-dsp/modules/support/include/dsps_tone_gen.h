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

#ifndef _dsps_tone_gen_H_
#define _dsps_tone_gen_H_
#include "dsp_err.h"


#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief   tone
 *
 * The function generate a tone signal.
 * x[i]=A*sin(2*PI*i + ph/180*PI)
 * The implementation use ANSI C and could be compiled and run on any platform
 *
 * @param output: output array.
 * @param len: length of the input signal
 * @param Ampl: amplitude
 * @param freq: Naiquist frequency -1..1
 * @param phase: phase in degree
 *
 * @return
 *      - ESP_OK on success
 *      - One of the error codes from DSP library
 */
esp_err_t dsps_tone_gen_f32(float *output, int len, float Ampl, float freq, float phase);

#ifdef __cplusplus
}
#endif

#endif // _dsps_tone_gen_H_
