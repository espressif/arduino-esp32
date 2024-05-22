/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#ifndef _dspm_add_H_
#define _dspm_add_H_
#include "dsp_err.h"

#include "dspm_add_platform.h"


#ifdef __cplusplus
extern "C"
{
#endif


/**@{*/
/**
 * @brief   add two arrays with paddings (add two sub-matrices)
 *
 * The function adds two arrays defined as sub-matrices with paddings
 * out[row * ptr_step_out + col * step_out] = in1[row * ptr_step_in1 + col * step1] + in2[row * ptr_step_in2 + col * step2];
 * The implementation use ANSI C and could be compiled and run on any platform
 *
 * @param[in]  input1: input array 1
 * @param[in]  input2: input array 2
 * @param[out] output: output array
 * @param[in]  rows: matrix rows
 * @param[in]  cols: matrix cols
 * @param[in]  padd1: input array 1 padding
 * @param[in]  padd2: input array 2 padding
 * @param[in]  padd_out: output array padding
 * @param[in]  step1: step over input array 1 (by default should be 1)
 * @param[in]  step2: step over input array 2 (by default should be 1)
 * @param[in]  step_out: step over output array (by default should be 1)
 *
 * @return
 *      - ESP_OK on success
 *      - One of the error codes from DSP library
 */
esp_err_t dspm_add_f32_ansi(const float *input1, const float *input2, float *output, int rows, int cols, int padd1, int padd2, int padd_out, int step1, int step2, int step_out);
esp_err_t dspm_add_f32_ae32(const float *input1, const float *input2, float *output, int rows, int cols, int padd1, int padd2, int padd_out, int step1, int step2, int step_out);
/**@}*/

#ifdef __cplusplus
}
#endif

#if CONFIG_DSP_OPTIMIZED

#if (dspm_add_f32_ae32_enabled == 1)
#define dspm_add_f32 dspm_add_f32_ae32
#else
#define dspm_add_f32 dspm_add_f32_ansi
#endif

#else // CONFIG_DSP_OPTIMIZED
#define dspm_add_f32 dspm_add_f32_ansi
#endif // CONFIG_DSP_OPTIMIZED

#endif // _dspm_add_H_
