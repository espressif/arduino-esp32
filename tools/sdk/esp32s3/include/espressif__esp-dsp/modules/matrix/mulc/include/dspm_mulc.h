/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#ifndef _dspm_mulc_H_
#define _dspm_mulc_H_
#include "dsp_err.h"

#include "dspm_mulc_platform.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**@{*/
/**
 * @brief   multiply a constant and an array with padding
 *
 * The function multiplies a constant and an array defined as s sub-matrix with padding
 * out[row * ptr_step_out + col * step_out] = input[row * ptr_step_in + col * step_in] * C;
 * The implementation uses ANSI C and could be compiled and run on any platform
 *
 * @param[in]  input: input array
 * @param[out] output: output array
 * @param[in]  C: constant value
 * @param[in]  rows: input matrix rows
 * @param[in]  cols: input matrix cols
 * @param[in]  padd_in: input array padding
 * @param[in]  padd_out: output array padding
 * @param[in]  step_in: step over input array (by default should be 1)
 * @param[in]  step_out: step over output array (by default should be 1)
 *
 * @return
 *      - ESP_OK on success
 *      - One of the error codes from DSP library
 */
esp_err_t dspm_mulc_f32_ansi(const float *input, float *output, float C, int rows, int cols, int padd_in, int padd_out, int step_in, int step_out);
esp_err_t dspm_mulc_f32_ae32(const float *input, float *output, float C, int rows, int cols, int padd_in, int padd_out, int step_in, int step_out);
/**@}*/

#ifdef __cplusplus
}
#endif

#if CONFIG_DSP_OPTIMIZED
#if (dspm_mulc_f32_ae32_enabled == 1)
#define dspm_mulc_f32 dspm_mulc_f32_ae32
#else //
#define dspm_mulc_f32 dspm_mulc_f32_ansi
#endif

#else
#define dspm_mulc_f32 dspm_mulc_f32_ansi
#endif


#endif // _dspm_mulc_H_
