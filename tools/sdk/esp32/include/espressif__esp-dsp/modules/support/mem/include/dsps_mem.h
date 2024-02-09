/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _dsps_mem_H_
#define _dsps_mem_H_

#include "dsp_err.h"
#include "dsp_common.h"
#include "dsps_mem_platform.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**@{*/
/**
 *  @brief memory copy function using esp32s3 TIE
 *
 * The extension (_aes3) is optimized for esp32S3 chip.
 *
 * @param arr_dest: pointer to the destination array
 * @param arr_src: pointer to the source array
 * @param arr_len: count of bytes to be copied from arr_src to arr_dest
 *
 * @return: pointer to dest array
 */
void *dsps_memcpy_aes3(void *arr_dest, const void *arr_src, size_t arr_len);

/**@{*/
/**
 *  @brief memory set function using esp32s3 TIE
 *
 * The extension (_aes3) is optimized for esp32S3 chip.
 *
 * @param arr_dest: pointer to the destination array
 * @param set_val: byte value, the dest array will be set with
 * @param set_size: count of bytes, the dest array will be set with
 *
 * @return: pointer to dest array 
 */
void *dsps_memset_aes3(void *arr_dest, uint8_t set_val, size_t set_size);

#ifdef __cplusplus
}
#endif

#if CONFIG_DSP_OPTIMIZED

    #if dsps_mem_aes3_enbled
    #define dsps_memcpy dsps_memcpy_aes3
    #define dsps_memset dsps_memset_aes3
    #else
    #define dsps_memcpy memcpy
    #define dsps_memset memset
    #endif

#else // CONFIG_DSP_OPTIMIZED

    #define dsps_memcpy memcpy
    #define dsps_memset memset

#endif // CONFIG_DSP_OPTIMIZED
#endif // _dsps_mem_H_
