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

#ifndef _dsps_fir_H_
#define _dsps_fir_H_


#include "dsp_err.h"

#include "dsps_fir_platform.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Data struct of f32 fir filter
 *
 * This structure used by filter internally. User should access this structure only in case of
 * extensions for the DSP Library.
 * All fields of this structure initialized by dsps_fir_init_f32(...) function.
 */
typedef struct fir_f32_s {
    float  *coeffs;     /*!< Pointer to the coefficient buffer.*/
    float  *delay;      /*!< Pointer to the delay line buffer.*/
    int     N;          /*!< FIR filter coefficients amount.*/
    int     pos;        /*!< Position in delay line.*/
    int     decim;      /*!< Decimation factor.*/
    int     d_pos;      /*!< Actual decimation counter.*/
} fir_f32_t;

/**
 * @brief   initialize structure for 32 bit FIR filter
 *
 * Function initialize structure for 32 bit floating point FIR filter
 * The implementation use ANSI C and could be compiled and run on any platform
 *
 * @param fir: pointer to fir filter structure, that must be preallocated
 * @param coeffs: array with FIR filter coefficients. Must be length N
 * @param delay: array for FIR filter delay line. Must be length N
 * @param N: FIR filter length. Length of coeffs and delay arrays.
 *
 * @return
 *      - ESP_OK on success
 *      - One of the error codes from DSP library
 */
esp_err_t dsps_fir_init_f32(fir_f32_t *fir, float *coeffs, float *delay, int N);

/**
 * @brief   initialize structure for 32 bit Decimation FIR filter
 * Function initialize structure for 32 bit floating point FIR filter with decimation
 * The implementation use ANSI C and could be compiled and run on any platform
 *
 * @param fir: pointer to fir filter structure, that must be preallocated
 * @param coeffs: array with FIR filter coefficients. Must be length N
 * @param delay: array for FIR filter delay line. Must be length N
 * @param N: FIR filter length. Length of coeffs and delay arrays.
 * @param decim: decimation factor.
 * @param start_pos: initial value of decimation counter. Must be [0..d)
 *
 * @return
 *      - ESP_OK on success
 *      - One of the error codes from DSP library
 */
esp_err_t dsps_fird_init_f32(fir_f32_t *fir, float *coeffs, float *delay, int N, int decim, int start_pos);


/**@{*/
/**
 * @brief   FIR filter
 *
 * Function implements FIR filter
 * The extension (_ansi) use ANSI C and could be compiled and run on any platform.
 * The extension (_ae32) is optimized for ESP32 chip.
 *
 * @param fir: pointer to fir filter structure, that must be initialized before
 * @param[in] input: input array
 * @param[out] output: array with result of FIR filter
 * @param[in] len: length of input and result arrays
 *
 * @return
 *      - ESP_OK on success
 *      - One of the error codes from DSP library
 */
esp_err_t dsps_fir_f32_ansi(fir_f32_t *fir, const float *input, float *output, int len);
esp_err_t dsps_fir_f32_ae32(fir_f32_t *fir, const float *input, float *output, int len);
esp_err_t dsps_fir_f32_aes3(fir_f32_t *fir, const float *input, float *output, int len);
/**@}*/

/**@{*/
/**
 *  @brief   Decimation FIR filter
 *
 * Function implements FIR filter with decimation
 * The extension (_ansi) use ANSI C and could be compiled and run on any platform.
 * The extension (_ae32) is optimized for ESP32 chip.
 *
 * @param fir: pointer to fir filter structure, that must be initialized before
 * @param input: input array
 * @param output: array with result of FIR filter
 * @param len: length of input and result arrays
 *
 * @return: function returns amount of samples stored to the output array
 *          depends on the previous state value could be [0..len/decimation]
 */
int dsps_fird_f32_ansi(fir_f32_t *fir, const float *input, float *output, int len);
int dsps_fird_f32_ae32(fir_f32_t *fir, const float *input, float *output, int len);
/**@}*/


#ifdef __cplusplus
}
#endif


#if CONFIG_DSP_OPTIMIZED

#if (dsps_fir_f32_ae32_enabled == 1)
#define dsps_fir_f32 dsps_fir_f32_ae32
#else
#define dsps_fir_f32 dsps_fir_f32_ansi
#endif

#if (dsps_fird_f32_ae32_enabled == 1)
#define dsps_fird_f32 dsps_fird_f32_ae32
#else
#define dsps_fird_f32 dsps_fird_f32_ansi
#endif

#else // CONFIG_DSP_OPTIMIZED
#define dsps_fir_f32 dsps_fir_f32_ansi
#define dsps_fird_f32 dsps_fird_f32_ansi
#endif // CONFIG_DSP_OPTIMIZED

#endif // _dsps_fir_H_