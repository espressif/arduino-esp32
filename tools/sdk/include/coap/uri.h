/*
 * uri.h -- helper functions for URI treatment
 *
 * Copyright (C) 2010-2011,2016 Olaf Bergmann <bergmann@tzi.org>
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

#ifndef _COAP_URI_H_
#define _COAP_URI_H_

#include "hashkey.h"
#include "str.h"

/**
 * Representation of parsed URI. Components may be filled from a string with
 * coap_split_uri() and can be used as input for option-creation functions.
 */
typedef struct {
  str host;             /**< host part of the URI */
  unsigned short port;  /**< The port in host byte order */
  str path;             /**< Beginning of the first path segment. 
                             Use coap_split_path() to create Uri-Path options */
  str query;            /**<  The query part if present */
} coap_uri_t;

/**
 * Creates a new coap_uri_t object from the specified URI. Returns the new
 * object or NULL on error. The memory allocated by the new coap_uri_t
 * must be released using coap_free().
 *
 * @param uri The URI path to copy.
 * @param length The length of uri.
 *
 * @return New URI object or NULL on error.
 */
coap_uri_t *coap_new_uri(const unsigned char *uri, unsigned int length);

/**
 * Clones the specified coap_uri_t object. Thie function allocates sufficient
 * memory to hold the coap_uri_t structure and its contents. The object must
 * be released with coap_free(). */
coap_uri_t *coap_clone_uri(const coap_uri_t *uri);

/**
 * Calculates a hash over the given path and stores the result in 
 * @p key. This function returns @c 0 on error or @c 1 on success.
 * 
 * @param path The URI path to generate hash for.
 * @param len  The length of @p path.
 * @param key  The output buffer.
 * 
 * @return @c 1 if @p key was set, @c 0 otherwise.
 */
int coap_hash_path(const unsigned char *path, size_t len, coap_key_t key);

/**
 * @defgroup uri_parse URI Parsing Functions
 *
 * CoAP PDUs contain normalized URIs with their path and query split into
 * multiple segments. The functions in this module help splitting strings.
 * @{
 */

/**
 * Parses a given string into URI components. The identified syntactic
 * components are stored in the result parameter @p uri. Optional URI
 * components that are not specified will be set to { 0, 0 }, except for the
 * port which is set to @c COAP_DEFAULT_PORT. This function returns @p 0 if
 * parsing succeeded, a value less than zero otherwise.
 * 
 * @param str_var The string to split up.
 * @param len     The actual length of @p str_var
 * @param uri     The coap_uri_t object to store the result.
 * @return        @c 0 on success, or < 0 on error.
 *
 */
int coap_split_uri(const unsigned char *str_var, size_t len, coap_uri_t *uri);

/**
 * Splits the given URI path into segments. Each segment is preceded
 * by an option pseudo-header with delta-value 0 and the actual length
 * of the respective segment after percent-decoding.
 * 
 * @param s      The path string to split. 
 * @param length The actual length of @p s.
 * @param buf    Result buffer for parsed segments. 
 * @param buflen Maximum length of @p buf. Will be set to the actual number
 *               of bytes written into buf on success.
 * 
 * @return       The number of segments created or @c -1 on error.
 */
int coap_split_path(const unsigned char *s,
                    size_t length,
                    unsigned char *buf,
                    size_t *buflen);

/** 
 * Splits the given URI query into segments. Each segment is preceded
 * by an option pseudo-header with delta-value 0 and the actual length
 * of the respective query term.
 * 
 * @param s      The query string to split. 
 * @param length The actual length of @p s.
 * @param buf    Result buffer for parsed segments. 
 * @param buflen Maximum length of @p buf. Will be set to the actual number
 *               of bytes written into buf on success.
 * 
 * @return       The number of segments created or @c -1 on error.
 *
 * @bug This function does not reserve additional space for delta > 12.
 */
int coap_split_query(const unsigned char *s,
                     size_t length,
                     unsigned char *buf,
                     size_t *buflen);

/** @} */

#endif /* _COAP_URI_H_ */
