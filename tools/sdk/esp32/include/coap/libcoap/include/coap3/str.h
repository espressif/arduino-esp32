/*
 * str.h -- strings to be used in the CoAP library
 *
 * Copyright (C) 2010-2011 Olaf Bergmann <bergmann@tzi.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

#ifndef COAP_STR_H_
#define COAP_STR_H_

#include <string.h>


/**
 * @defgroup string String handling support
 * API functions for handling strings and binary data
 * @{
 */

/*
 * Note: string and binary use equivalent objects.
 * string is likely to contain readable textual information, binary will not.
 */

/**
 * CoAP string data definition
 */
typedef struct coap_string_t {
  size_t length;    /**< length of string */
  uint8_t *s;       /**< string data */
} coap_string_t;

/**
 * CoAP string data definition with const data
 */
typedef struct coap_str_const_t {
  size_t length;    /**< length of string */
  const uint8_t *s; /**< read-only string data */
} coap_str_const_t;

#define COAP_SET_STR(st,l,v) { (st)->length = (l), (st)->s = (v); }

/**
 * CoAP binary data definition
 */
typedef struct coap_binary_t {
  size_t length;    /**< length of binary data */
  uint8_t *s;       /**< binary data */
} coap_binary_t;

/**
 * CoAP binary data definition with const data
 */
typedef struct coap_bin_const_t {
  size_t length;    /**< length of binary data */
  const uint8_t *s; /**< read-only binary data */
} coap_bin_const_t;

/**
 * Returns a new string object with at least size+1 bytes storage allocated.
 * It is the responsibility of the caller to fill in all the appropriate
 * information.
 * The string must be released using coap_delete_string().
 *
 * @param size The size to allocate for the string data.
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
 * Returns a new binary object with at least size bytes storage allocated.
 * It is the responsibility of the caller to fill in all the appropriate
 * information.
 * The coap_binary_t object must be released using coap_delete_binary().
 *
 * @param size The size to allocate for the binary data.
 *
 * @return       A pointer to the new object or @c NULL on error.
 */
coap_binary_t *coap_new_binary(size_t size);

/**
 * Deletes the given coap_binary_t object and releases any memory allocated.
 *
 * @param binary The coap_binary_t object to free off.
 */
void coap_delete_binary(coap_binary_t *binary);

/**
 * Resizes the given coap_binary_t object.
 * It is the responsibility of the caller to fill in all the appropriate
 * additional information.
 *
 * Note: If there is an error, @p binary will separately need to be released by
 * coap_delete_binary().
 *
 * @param binary The coap_binary_t object to resize.
 * @param new_size The new size to allocate for the binary data.
 *
 * @return       A pointer to the new object or @c NULL on error.
 */
coap_binary_t *coap_resize_binary(coap_binary_t *binary, size_t new_size);

/**
 * Take the specified byte array (text) and create a coap_bin_const_t *
 * Returns a new const binary object with at least size bytes storage
 * allocated, and the provided data copied into the binary object.
 * The binary data must be released using coap_delete_bin_const().
 *
 * @param data The data to put in the new string object.
 * @param size The size to allocate for the binary data.
 *
 * @return       A pointer to the new object or @c NULL on error.
 */
coap_bin_const_t *coap_new_bin_const(const uint8_t *data, size_t size);

/**
 * Deletes the given const binary data and releases any memory allocated.
 *
 * @param binary The binary data to free off.
 */
void coap_delete_bin_const(coap_bin_const_t *binary);

#ifndef COAP_MAX_STR_CONST_FUNC
#define COAP_MAX_STR_CONST_FUNC 2
#endif /* COAP_MAX_STR_CONST_FUNC */

/**
 * Take the specified byte array (text) and create a coap_str_const_t *
 *
 * Note: the array is 2 deep as there are up to two callings of
 * coap_make_str_const in a function call. e.g. coap_add_attr().
 * Caution: If there are local variable assignments, these will cycle around
 * the var[COAP_MAX_STR_CONST_FUNC] set.  No current examples do this.
 *
 * @param string The const string to convert to a coap_str_const_t *
 *
 * @return       A pointer to one of two static variables containing the
 *               coap_str_const_t * result
 */
coap_str_const_t *coap_make_str_const(const char *string);

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
         ((string1)->s && (string2)->s && \
          memcmp((string1)->s, (string2)->s, (string1)->length) == 0)))

/**
 * Compares the two binary data for equality
 *
 * @param binary1 The first binary data.
 * @param binary2 The second binary data.
 *
 * @return         @c 1 if the binary data is equal
 *                 @c 0 otherwise.
 */
#define coap_binary_equal(binary1,binary2) \
        ((binary1)->length == (binary2)->length && ((binary1)->length == 0 || \
         ((binary1)->s && (binary2)->s && \
          memcmp((binary1)->s, (binary2)->s, (binary1)->length) == 0)))

/** @} */

#endif /* COAP_STR_H_ */
