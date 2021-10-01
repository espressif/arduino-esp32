/*
 * encode.h -- encoding and decoding of CoAP data types
 *
 * Copyright (C) 2010-2012 Olaf Bergmann <bergmann@tzi.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

#ifndef COAP_ENCODE_H_
#define COAP_ENCODE_H_

#if (BSD >= 199103) || defined(WITH_CONTIKI) || defined(_WIN32)
# include <string.h>
#else
# include <strings.h>
#endif

#include <stdint.h>

#ifndef HAVE_FLS
/* include this only if fls() is not available */
extern int coap_fls(unsigned int i);
#else
#define coap_fls(i) fls(i)
#endif

#ifndef HAVE_FLSLL
 /* include this only if flsll() is not available */
extern int coap_flsll(long long i);
#else
#define coap_flsll(i) flsll(i)
#endif

/**
 * @defgroup encode Encode / Decode API
 * API functions for endoding/decoding CoAP options.
 * @{
 */

/**
 * Decodes multiple-length byte sequences. @p buf points to an input byte
 * sequence of length @p length. Returns the up to 4 byte decoded value.
 *
 * @param buf The input byte sequence to decode from
 * @param length The length of the input byte sequence
 *
 * @return      The decoded value
 */
unsigned int coap_decode_var_bytes(const uint8_t *buf, size_t length);

/**
 * Decodes multiple-length byte sequences. @p buf points to an input byte
 * sequence of length @p length. Returns the up to 8 byte decoded value.
 *
 * @param buf The input byte sequence to decode from
 * @param length The length of the input byte sequence
 *
 * @return      The decoded value
 */
uint64_t coap_decode_var_bytes8(const uint8_t *buf, size_t length);

/**
 * Encodes multiple-length byte sequences. @p buf points to an output buffer of
 * sufficient length to store the encoded bytes. @p value is the 4 byte value
 * to encode.
 * Returns the number of bytes used to encode @p value or 0 on error.
 *
 * @param buf    The output buffer to encode into
 * @param length The output buffer size to encode into (must be sufficient)
 * @param value  The value to encode into the buffer
 *
 * @return       The number of bytes used to encode @p value or @c 0 on error.
 */
unsigned int coap_encode_var_safe(uint8_t *buf,
                                  size_t length,
                                  unsigned int value);

/**
 * Encodes multiple-length byte sequences. @p buf points to an output buffer of
 * sufficient length to store the encoded bytes. @p value is the 8 byte value
 * to encode.
 * Returns the number of bytes used to encode @p value or 0 on error.
 *
 * @param buf    The output buffer to encode into
 * @param length The output buffer size to encode into (must be sufficient)
 * @param value  The value to encode into the buffer
 *
 * @return       The number of bytes used to encode @p value or @c 0 on error.
 */
unsigned int coap_encode_var_safe8(uint8_t *buf,
                                  size_t length,
                                  uint64_t value);

/** @} */

/**
 * @deprecated Use coap_encode_var_safe() instead.
 * Provided for backward compatibility.  As @p value has a
 * maximum value of 0xffffffff, and buf is usually defined as an array, it
 * is unsafe to continue to use this variant if buf[] is less than buf[4].
 *
 * For example
 *  char buf[1],oops;
 *  ..
 *  coap_encode_var_bytes(buf, 0xfff);
 * would cause oops to get overwritten.  This error can only be found by code
 * inspection.
 *   coap_encode_var_safe(buf, sizeof(buf), 0xfff);
 * would catch this error at run-time and should be used instead.
 */
COAP_STATIC_INLINE COAP_DEPRECATED int
coap_encode_var_bytes(uint8_t *buf, unsigned int value
) {
  return (int)coap_encode_var_safe(buf, sizeof(value), value);
}

#endif /* COAP_ENCODE_H_ */
