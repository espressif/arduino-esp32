/*
 * str.h -- strings to be used in the CoAP library
 *
 * Copyright (C) 2010-2011 Olaf Bergmann <bergmann@tzi.org>
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

#ifndef COAP_STR_H_
#define COAP_STR_H_

#include <string.h>


/**
 * @defgroup string String handling support
 * API functions for handling strings
 * @{
 */

/**
 * Coap string data definition
 */
typedef struct coap_string_t {
  size_t length;    /**< length of string */
  uint8_t *s;       /**< string data */
} coap_string_t;

/**
 * Coap string data definition with const data
 */
typedef struct coap_str_const_t {
  size_t length;    /**< length of string */
  const uint8_t *s; /**< string data */
} coap_str_const_t;

#define COAP_SET_STR(st,l,v) { (st)->length = (l), (st)->s = (v); }

/**
 * Coap binary data definition
 */
typedef struct coap_binary_t {
  size_t length;    /**< length of binary data */
  uint8_t *s;       /**< binary data */
} coap_binary_t;

/**
 * Returns a new string object with at least size+1 bytes storage allocated.
 * The string must be released using coap_delete_string().
 *
 * @param size The size to allocate for the binary string data.
 *
 * @return       A pointer to the new object or @c NULL on error.
 */
coap_string_t *coap_new_string(size_t size);

/**
 * Deletes the given string and releases any memory allocated.
 *
 * @param string The string to free off.
 */
void coap_delete_string(coap_string_t *string);

/**
 * Returns a new const string object with at least size+1 bytes storage
 * allocated, and the provided data copied into the string object.
 * The string must be released using coap_delete_str_const().
 *
 * @param data The data to put in the new string object.
 * @param size The size to allocate for the binary string data.
 *
 * @return       A pointer to the new object or @c NULL on error.
 */
coap_str_const_t *coap_new_str_const(const uint8_t *data, size_t size);

/**
 * Deletes the given const string and releases any memory allocated.
 *
 * @param string The string to free off.
 */
void coap_delete_str_const(coap_str_const_t *string);

/**
 * Take the specified byte array (text) and create a coap_str_const_t *
 *
 * WARNING: The byte array must be in the local scope and not a
 * parameter in the function call as sizeof() will return the size of the
 * pointer, not the size of the byte array, leading to unxepected results.
 *
 * @param string The const byte array to convert to a coap_str_const_t *
 */
#ifdef __cplusplus
namespace libcoap {
  struct CoAPStrConst : coap_str_const_t {
    operator coap_str_const_t *() { return this; }
  };
}
#define coap_make_str_const(CStr)                                       \
  libcoap::CoAPStrConst{sizeof(CStr)-1, reinterpret_cast<const uint8_t *>(CStr)}
#else /* __cplusplus */
#define coap_make_str_const(string)                                     \
  (&(coap_str_const_t){sizeof(string)-1,(const uint8_t *)(string)})
#endif  /* __cplusplus */

/**
 * Compares the two strings for equality
 *
 * @param string1 The first string.
 * @param string2 The second string.
 *
 * @return         @c 1 if the strings are equal
 *                 @c 0 otherwise.
 */
#define coap_string_equal(string1,string2) \
        ((string1)->length == (string2)->length && ((string1)->length == 0 || \
         memcmp((string1)->s, (string2)->s, (string1)->length) == 0))

/** @} */

#endif /* COAP_STR_H_ */
