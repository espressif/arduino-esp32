/*
 * encode.h -- encoding and decoding of CoAP data types
 *
 * Copyright (C) 2010-2012 Olaf Bergmann <bergmann@tzi.org>
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

#ifndef _COAP_ENCODE_H_
#define _COAP_ENCODE_H_

#if (BSD >= 199103) || defined(WITH_CONTIKI)
# include <string.h>
#else
# include <strings.h>
#endif

#define Nn 8  /* duplicate definition of N if built on sky motes */
#define ENCODE_HEADER_SIZE 4
#define HIBIT (1 << (Nn - 1))
#define EMASK ((1 << ENCODE_HEADER_SIZE) - 1)
#define MMASK ((1 << Nn) - 1 - EMASK)
#define MAX_VALUE ( (1 << Nn) - (1 << ENCODE_HEADER_SIZE) ) * (1 << ((1 << ENCODE_HEADER_SIZE) - 1))

#define COAP_PSEUDOFP_DECODE_8_4(r) (r < HIBIT ? r : (r & MMASK) << (r & EMASK))

#ifndef HAVE_FLS
/* include this only if fls() is not available */
extern int coap_fls(unsigned int i);
#else
#define coap_fls(i) fls(i)
#endif

/* ls and s must be integer variables */
#define COAP_PSEUDOFP_ENCODE_8_4_DOWN(v,ls) (v < HIBIT ? v : (ls = coap_fls(v) - Nn, (v >> ls) & MMASK) + ls)
#define COAP_PSEUDOFP_ENCODE_8_4_UP(v,ls,s) (v < HIBIT ? v : (ls = coap_fls(v) - Nn, (s = (((v + ((1<<ENCODE_HEADER_SIZE<<ls)-1)) >> ls) & MMASK)), s == 0 ? HIBIT + ls + 1 : s + ls))

/**
 * Decodes multiple-length byte sequences. buf points to an input byte sequence
 * of length len. Returns the decoded value.
 */
unsigned int coap_decode_var_bytes(unsigned char *buf,unsigned int len);

/**
 * Encodes multiple-length byte sequences. buf points to an output buffer of
 * sufficient length to store the encoded bytes. val is the value to encode.
 * Returns the number of bytes used to encode val or 0 on error.
 */
unsigned int coap_encode_var_bytes(unsigned char *buf, unsigned int val);

#endif /* _COAP_ENCODE_H_ */
