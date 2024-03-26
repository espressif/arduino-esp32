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

#ifndef _DSPI_DOTPROD_H_
#define _DSPI_DOTPROD_H_

#include "esp_log.h"
#include "dsp_err.h"

#include "dsps_dotprod_platform.h"

#ifdef __cplusplus
extern "C"
{
#endif
// These functions calculates dotproduct of two vectors.

/**@{*/
/**
 * @brief      dot product of two 16 bit vectors
 * Dot product calculation for two signed 16 bit arrays: *dest += (src1[i] * src2[i]) >> (15-shift); i= [0..N)
 * The extension (_ansi) use ANSI C and could be compiled and run on any platform.
 * The extension (_ae32) is optimized for ESP32 chip.
 *
 * @param[in] src1  source array 1
 * @param[in] src2  source array 2
 * @param dest  destination pointer
 * @param[in] len   length of input arrays
 * @param[in] shift shift of the result.
 * @return
 *      - ESP_OK on success
 *      - One of the error codes from DSP library
 */
esp_err_t dsps_dotprod_s16_ansi(const int16_t *src1, const int16_t *src2, int16_t *dest, int len, int8_t shift);
esp_err_t dsps_dotprod_s16_ae32(const int16_t *src1, const int16_t *src2, int16_t *dest, int len, int8_t shift);
/**@}*/


/**@{*/
/**
 * @brief      dot product of two float vectors
 * Dot product calculation for two floating point arrays: *dest += (src1[i] * src2[i]); i= [0..N)
 * The extension (_ansi) use ANSI C and could be compiled and run on any platform.
 * The extension (_ae32) is optimized for ESP32 chip.
 *
 * @param[in] src1  source array 1
 * @param[in] src2  source array 2
 * @param dest  destination pointer
 * @param[in] len   length of input arrays
 * @return
 *      - ESP_OK on success
 *      - One of the error codes from DSP library
 */
esp_err_t dsps_dotprod_f32_ansi(const float *src1, const float *src2, float *dest, int len);
esp_err_t dsps_dotprod_f32_ae32(const float *src1, const float *src2, float *dest, int len);
esp_err_t dsps_dotprod_f32_aes3(const float *src1, const float *src2, float *dest, int len);
/**@}*/

/**@{*/
/**
 * @brief      dot product of two float vectors with step
 * Dot product calculation for two floating point arrays: *dest += (src1[i*step1] * src2[i*step2]); i= [0..N)
 * The extension (_ansi) use ANSI C and could be compiled and run on any platform.
 * The extension (_ae32) is optimized for ESP32 chip.
 *
 * @param[in] src1  source array 1
 * @param[in] src2  source array 2
 * @param dest  destination pointer
 * @param[in] len   length of input arrays
 * @param[in] step1 step over elements in first array
 * @param[in] step2 step over elements in second array
 * @return
 *      - ESP_OK on success
 *      - One of the error codes from DSP library
 */
esp_err_t dsps_dotprode_f32_ansi(const float *src1, const float *src2, float *dest, int len, int step1, int step2);
esp_err_t dsps_dotprode_f32_ae32(const float *src1, const float *src2, float *dest, int len, int step1, int step2);
/**@}*/

#ifdef __cplusplus
}
#endif

#if CONFIG_DSP_OPTIMIZED

#if (dsps_dotprod_s16_ae32_enabled == 1)
#define dsps_dotprod_s16 dsps_dotprod_s16_ae32
#else
#define dsps_dotprod_s16 dsps_dotprod_s16_ansi
#endif // dsps_dotprod_s16_ae32_enabled

#if (dsps_dotprod_f32_aes3_enabled == 1)
#define dsps_dotprod_f32 dsps_dotprod_f32_aes3
#define dsps_dotprode_f32 dsps_dotprode_f32_ae32
#elif (dotprod_f32_ae32_enabled == 1)
#define dsps_dotprod_f32 dsps_dotprod_f32_ae32
#define dsps_dotprode_f32 dsps_dotprode_f32_ae32
#else
#define dsps_dotprod_f32 dsps_dotprod_f32_ansi
#define dsps_dotprode_f32 dsps_dotprode_f32_ansi
#endif // dsps_dotprod_f32_ae32_enabled

#else // CONFIG_DSP_OPTIMIZED
#define dsps_dotprod_s16 dsps_dotprod_s16_ansi
#define dsps_dotprod_f32 dsps_dotprod_f32_ansi
#define dsps_dotprode_f32 dsps_dotprode_f32_ansi
#endif // CONFIG_DSP_OPTIMIZED

#endif // _DSPI_DOTPROD_H_
