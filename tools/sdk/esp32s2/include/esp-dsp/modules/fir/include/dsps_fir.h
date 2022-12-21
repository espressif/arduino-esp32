// Copyright 2018-2022 Espressif Systems (Shanghai) PTE LTD
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
 * This structure is used by a filter internally. A user should access this structure only in case of
 * extensions for the DSP Library.
 * All fields of this structure are initialized by the dsps_fir_init_f32(...) function.
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
 * @brief Data struct of s16 fir filter
 *
 * This structure is used by a filter internally. A user should access this structure only in case of
 * extensions for the DSP Library.
 * All fields of this structure are initialized by the dsps_fir_init_s16(...) function.
 */
typedef struct fir_s16_s{
    int16_t    *coeffs;     /*!< Pointer to the coefficient buffer.*/
    int16_t    *delay;      /*!< Pointer to the delay line buffer.*/
    int16_t     coeffs_len; /*!< FIR filter coefficients amount.*/
    int16_t     pos;        /*!< Position in delay line.*/
    int16_t     decim;      /*!< Decimation factor.*/
    int16_t     d_pos;      /*!< Actual decimation counter.*/
    int16_t     shift;      /*!< shift value of the result.*/
}fir_s16_t;

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

/**
 * @brief   initialize structure for 16 bit Decimation FIR filter
 * Function initialize structure for 16 bit signed fixed point FIR filter with decimation
 * The implementation use ANSI C and could be compiled and run on any platform
 *
 * @param fir: pointer to fir filter structure, that must be preallocated
 * @param coeffs: array with FIR filter coefficients. Must be length N
 * @param delay: array for FIR filter delay line. Must be length N
 * @param coeffs_len: FIR filter length. Length of coeffs and delay arrays.
 * @param decim: decimation factor.
 * @param start_pos: initial value of decimation counter. Must be [0..d)
 * @param shift: shift position of the result
 *
 * @return
 *      - ESP_OK on success
 *      - One of the error codes from DSP library
 */
esp_err_t dsps_fird_init_s16(fir_s16_t *fir, int16_t *coeffs, int16_t *delay, int16_t coeffs_len, int16_t decim, int16_t start_pos, int16_t shift);


/**@{*/
/**
 * @brief   32 bit floating point FIR filter
 *
 * Function implements FIR filter
 * The extension (_ansi) uses ANSI C and could be compiled and run on any platform.
 * The extension (_ae32) is optimized for ESP32 chip.
 *
 * @param fir: pointer to fir filter structure, that must be initialized before
 * @param[in] input: input array
 * @param[out] output: array with the result of FIR filter
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
 *  @brief   32 bit floating point Decimation FIR filter
 *
 * Function implements FIR filter with decimation
 * The extension (_ansi) uses ANSI C and could be compiled and run on any platform.
 * The extension (_ae32) is optimized for ESP32 chip.
 *
 * @param fir: pointer to fir filter structure, that must be initialized before
 * @param input: input array
 * @param output: array with the result of FIR filter
 * @param len: length of input and result arrays
 *
 * @return: function returns the number of samples stored in the output array
 *          depends on the previous state value could be [0..len/decimation]
 */
int dsps_fird_f32_ansi(fir_f32_t *fir, const float *input, float *output, int len);
int dsps_fird_f32_ae32(fir_f32_t *fir, const float *input, float *output, int len);
/**@}*/

/**@{*/
/**
 *  @brief   16 bit signed fixed point Decimation FIR filter
 *
 * Function implements FIR filter with decimation
 * The extension (_ansi) uses ANSI C and could be compiled and run on any platform.
 * The extension (_ae32) is optimized for ESP32 chip.
 *
 * @param fir: pointer to fir filter structure, that must be initialized before
 * @param input: input array
 * @param output: array with the result of the FIR filter
 * @param len: length of the result array
 *
 * @return: function returns the number of samples stored in the output array
 *          depends on the previous state value could be [0..len/decimation]
 */
int32_t dsps_fird_s16_ansi(fir_s16_t *fir, const int16_t *input, int16_t *output, int32_t len);
int32_t dsps_fird_s16_ae32(fir_s16_t *fir, const int16_t *input, int16_t *output, int32_t len);
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

#if (dsps_fird_s16_ae32_enabled == 1)
#define dsps_fird_s16 dsps_fird_s16_ae32
#else
#define dsps_fird_s16 dsps_fird_s16_ansi
#endif

#else // CONFIG_DSP_OPTIMIZED
#define dsps_fir_f32 dsps_fir_f32_ansi
#define dsps_fird_f32 dsps_fird_f32_ansi
#define dsps_fird_s16 dsps_fird_s16_ansi
#endif // CONFIG_DSP_OPTIMIZED

#endif // _dsps_fir_H_