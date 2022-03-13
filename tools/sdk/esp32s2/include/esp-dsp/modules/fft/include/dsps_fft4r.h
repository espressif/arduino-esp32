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

#ifndef _dsps_fft4r_H_
#define _dsps_fft4r_H_
#include "dsp_err.h"
#include "sdkconfig.h"

#include "dsps_fft_tables.h"
#include "dsps_fft4r_platform.h"

#ifdef __cplusplus
extern "C"
{
#endif

extern float *dsps_fft4r_w_table_fc32;
extern int dsps_fft4r_w_table_size;
extern uint8_t dsps_fft4r_initialized;

extern int16_t *dsps_fft4r_w_table_sc16;
extern int dsps_fft4r_w_table_sc16_size;
extern uint8_t dsps_fft4r_sc16_initialized;

/**@{*/
/**
 * @brief      init fft tables
 *
 * Initialization of Complex FFT Radix-4. This function initialize coefficients table.
 * The implementation use ANSI C and could be compiled and run on any platform
 *
 * @param[inout] fft_table_buff: pointer to floating point buffer where sin/cos table will be stored
 *                          if this parameter set to NULL, and table_size value is more then 0, then
 *                          dsps_fft4r_init_fc32 will allocate buffer internally
 * @param[in] max_fft_size: maximum fft size. The buffer for sin/cos table that will be used for radix-4 it's
 *                          four times maximum length of FFT.
 *                          if fft_table_buff is NULL and table_size is not 0, buffer will be allocated internally.
 *                          If table_size is 0, buffer will not be allocated.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_DSP_PARAM_OUTOFRANGE if table_size > CONFIG_DSP_MAX_FFT_SIZE
 *      - ESP_ERR_DSP_REINITIALIZED if buffer already allocated internally by other function
 *      - One of the error codes from DSP library
 */
esp_err_t dsps_fft4r_init_fc32(float *fft_table_buff, int max_fft_size);
/**@}*/

/**@{*/
/**
 * @brief      deinit fft tables
 *
 * Free resources of Complex FFT Radix-4. This function delete coefficients table if it was allocated by dsps_fft4r_init_fc32.
 * The implementation use ANSI C and could be compiled and run on any platform
 *
 *
 * @return
 */
void dsps_fft4r_deinit_fc32(void);
/**@}*/

/**@{*/
/**
 * @brief      complex FFT of radix 4
 *
 * Complex FFT of radix 4
 * The extension (_ansi) use ANSI C and could be compiled and run on any platform.
 * The extension (_ae32) is optimized for ESP32 chip.
 *
 * @param[inout] data: input/output complex array. An elements located: Re[0], Im[0], ... Re[N-1], Im[N-1]
 *               result of FFT will be stored to this array.
 * @param[in] N: Number of complex elements in input array
 * @param[in] table: pointer to sin/cos table
 * @param[in] table_size: size of the sin/cos table
 *
 * @return
 *      - ESP_OK on success
 *      - One of the error codes from DSP library
 */
esp_err_t dsps_fft4r_fc32_ansi_(float *data, int N, float *table, int table_size);
esp_err_t dsps_fft4r_fc32_ae32_(float *data, int N, float *table, int table_size);
/**@}*/
// This is workaround because linker generates permanent error when assembler uses
// direct access to the table pointer
#define dsps_fft4r_fc32_ansi(data, N) dsps_fft4r_fc32_ansi_(data, N, dsps_fft4r_w_table_fc32, dsps_fft4r_w_table_size)
#define dsps_fft4r_fc32_ae32(data, N) dsps_fft4r_fc32_ae32_(data, N, dsps_fft4r_w_table_fc32, dsps_fft4r_w_table_size)

/**@{*/
/**
 * @brief      bit reverse operation for the complex input array radix-4
 *
 * Bit reverse operation for the complex input array
 * The implementation use ANSI C and could be compiled and run on any platform
 *
 * @param[inout] data: input/ complex array. An elements located: Re[0], Im[0], ... Re[N-1], Im[N-1]
 *               result of FFT will be stored to this array.
 * @param[in] N: Number of complex elements in input array
 *
 * @return
 *      - ESP_OK on success
 *      - One of the error codes from DSP library
 */
esp_err_t dsps_bit_rev4r_fc32(float *data, int N);
esp_err_t dsps_bit_rev4r_fc32_ae32(float *data, int N);
esp_err_t dsps_bit_rev4r_direct_fc32_ansi(float *data, int N);
esp_err_t dsps_bit_rev4r_sc16_ansi(int16_t *data, int N);


/**@{*/
/**
 * @brief      Convert complex FFT result to real array
 *
 * Convert FFT result of complex FFT for real input to real array.
 * This function have to be used if FFT used to process real data.
 * This function use tabels inside and can be used only it dsps_fft4r_init_fc32(...) was
 * called and FFT4 was initialized.
 * The implementation use ANSI C and could be compiled and run on any platform
 *
 * @param[inout] data: Input complex array and result of FFT2R/FFT4R.
 *               input has size of 2*N, because contains real and imaginary part.
 *               result will be stored to the same array.
 *               Input1: input[0..N-1], Input2: input[N..2*N-1]
 * @param[in] N: Number of complex elements in input array
 * @param[in] table: pointer to sin/cos table
 * @param[in] table_size: size of the sin/cos table
 *
 * @return
 *      - ESP_OK on success
 *      - One of the error codes from DSP library
 */
esp_err_t dsps_cplx2real_fc32_ansi_(float *data, int N, float *table, int table_size);
esp_err_t dsps_cplx2real_fc32_ae32_(float *data, int N, float *table, int table_size);
/**@}*/
#define dsps_cplx2real_fc32_ansi(data, N) dsps_cplx2real_fc32_ansi_(data, N, dsps_fft4r_w_table_fc32, dsps_fft4r_w_table_size)
#define dsps_cplx2real_fc32_ae32(data, N) dsps_cplx2real_fc32_ae32_(data, N, dsps_fft4r_w_table_fc32, dsps_fft4r_w_table_size)


esp_err_t dsps_gen_bitrev4r_table(int N, int step, char *name_ext);

#ifdef __cplusplus
}
#endif

#if CONFIG_DSP_OPTIMIZED
#if (dsps_fft4r_fc32_ae32_enabled == 1)
#define dsps_fft4r_fc32 dsps_fft4r_fc32_ae32
#else
#define dsps_fft4r_fc32 dsps_fft4r_fc32_ansi
#endif // dsps_fft4r_fc32_ae32_enabled

#define dsps_fft4r_sc16 dsps_fft4r_sc16_ae32
#define dsps_bit_rev4r_fc32 dsps_bit_rev4r_fc32_ae32

#if (dsps_cplx2real_fc32_ae32_enabled == 1)
#define dsps_cplx2real_fc32 dsps_cplx2real_fc32_ae32
#else
#define dsps_cplx2real_fc32 dsps_cplx2real_fc32_ansi
#endif // dsps_cplx2real_fc32_ae32_enabled

#else
#define dsps_fft4r_fc32 dsps_fft4r_fc32_ansi
#define dsps_fft4r_sc16 dsps_fft4r_sc16_ansi
#define dsps_bit_rev4r_fc32 dsps_bit_rev4r_fc32
#define dsps_cplx2real_fc32 dsps_cplx2real_fc32_ansi
#endif

#endif // _dsps_fft4r_H_