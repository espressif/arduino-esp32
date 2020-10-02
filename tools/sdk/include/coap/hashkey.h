/*
 * hashkey.h -- definition of hash key type and helper functions
 *
 * Copyright (C) 2010-2011 Olaf Bergmann <bergmann@tzi.org>
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

/**
 * @file hashkey.h
 * @brief definition of hash key type and helper functions
 */

#ifndef _COAP_HASHKEY_H_
#define _COAP_HASHKEY_H_

#include "str.h"

typedef unsigned char coap_key_t[4];

#ifndef coap_hash
/**
 * Calculates a fast hash over the given string @p s of length @p len and stores
 * the result into @p h. Depending on the exact implementation, this function
 * cannot be used as one-way function to check message integrity or simlar.
 *
 * @param s   The string used for hash calculation.
 * @param len The length of @p s.
 * @param h   The result buffer to store the calculated hash key.
 */
void coap_hash_impl(const unsigned char *s, unsigned int len, coap_key_t h);

#define coap_hash(String,Length,Result) \
  coap_hash_impl((String),(Length),(Result))

/* This is used to control the pre-set hash-keys for resources. */
#define __COAP_DEFAULT_HASH
#else
#undef __COAP_DEFAULT_HASH
#endif /* coap_hash */

/**
 * Calls coap_hash() with given @c str object as parameter.
 *
 * @param Str Must contain a pointer to a coap string object.
 * @param H   A coap_key_t object to store the result.
 *
 * @hideinitializer
 */
#define coap_str_hash(Str,H) {               \
    assert(Str);                             \
    memset((H), 0, sizeof(coap_key_t));      \
    coap_hash((Str)->s, (Str)->length, (H)); \
  }

#endif /* _COAP_HASHKEY_H_ */
