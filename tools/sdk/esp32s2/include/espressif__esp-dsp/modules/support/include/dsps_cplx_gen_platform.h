/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _dsps_cplx_gen_platform_H_
#define _dsps_cplx_gen_platform_H_

#include "sdkconfig.h"

#ifdef __XTENSA__
#include <xtensa/config/core-isa.h>
#include <xtensa/config/core-matmap.h>


#if ((XCHAL_HAVE_FP == 1) && (XCHAL_HAVE_LOOPS == 1))

#if CONFIG_IDF_TARGET_ESP32S3
#define dsps_cplx_gen_aes3_enbled  1
#define dsps_cplx_gen_ae32_enbled  0

#elif CONFIG_IDF_TARGET_ESP32
#define dsps_cplx_gen_ae32_enbled  1
#define dsps_cplx_gen_aes3_enbled  0

#endif // CONFIG_IDF_TARGET_ESP32S3 CONFIG_IDF_TARGET_ESP32
#endif //
#endif // __XTENSA__
#endif // _dsps_cplx_gen_platform_H_
