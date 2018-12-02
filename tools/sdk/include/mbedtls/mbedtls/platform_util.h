/**
 * \file platform_util.h
 *
 * \brief Common and shared functions used by multiple modules in the Mbed TLS
 *        library.
 */
/*
 *  Copyright (C) 2018, Arm Limited, All Rights Reserved
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  This file is part of Mbed TLS (https://tls.mbed.org)
 */
#ifndef MBEDTLS_PLATFORM_UTIL_H
#define MBEDTLS_PLATFORM_UTIL_H

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#include <stddef.h>
#if defined(MBEDTLS_HAVE_TIME_DATE)
#include "mbedtls/platform_time.h"
#include <time.h>
#endif /* MBEDTLS_HAVE_TIME_DATE */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief       Securely zeroize a buffer
 *
 *              The function is meant to wipe the data contained in a buffer so
 *              that it can no longer be recovered even if the program memory
 *              is later compromised. Call this function on sensitive data
 *              stored on the stack before returning from a function, and on
 *              sensitive data stored on the heap before freeing the heap
 *              object.
 *
 *              It is extremely difficult to guarantee that calls to
 *              mbedtls_platform_zeroize() are not removed by aggressive
 *              compiler optimizations in a portable way. For this reason, Mbed
 *              TLS provides the configuration option
 *              MBEDTLS_PLATFORM_ZEROIZE_ALT, which allows users to configure
 *              mbedtls_platform_zeroize() to use a suitable implementation for
 *              their platform and needs
 *
 * \param buf   Buffer to be zeroized
 * \param len   Length of the buffer in bytes
 *
 */
void mbedtls_platform_zeroize( void *buf, size_t len );

#if defined(MBEDTLS_HAVE_TIME_DATE)
/**
 * \brief      Platform-specific implementation of gmtime_r()
 *
 *             The function is a thread-safe abstraction that behaves
 *             similarly to the gmtime_r() function from Unix/POSIX.
 *
 *             Mbed TLS will try to identify the underlying platform and
 *             make use of an appropriate underlying implementation (e.g.
 *             gmtime_r() for POSIX and gmtime_s() for Windows). If this is
 *             not possible, then gmtime() will be used. In this case, calls
 *             from the library to gmtime() will be guarded by the mutex
 *             mbedtls_threading_gmtime_mutex if MBEDTLS_THREADING_C is
 *             enabled. It is recommended that calls from outside the library
 *             are also guarded by this mutex.
 *
 *             If MBEDTLS_PLATFORM_GMTIME_R_ALT is defined, then Mbed TLS will
 *             unconditionally use the alternative implementation for
 *             mbedtls_platform_gmtime_r() supplied by the user at compile time.
 *
 * \param tt     Pointer to an object containing time (in seconds) since the
 *               epoch to be converted
 * \param tm_buf Pointer to an object where the results will be stored
 *
 * \return      Pointer to an object of type struct tm on success, otherwise
 *              NULL
 */
struct tm *mbedtls_platform_gmtime_r( const mbedtls_time_t *tt,
                                      struct tm *tm_buf );
#endif /* MBEDTLS_HAVE_TIME_DATE */

#ifdef __cplusplus
}
#endif

#endif /* MBEDTLS_PLATFORM_UTIL_H */
