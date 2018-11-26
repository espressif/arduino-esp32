/*
 * str.h -- strings to be used in the CoAP library
 *
 * Copyright (C) 2010-2011 Olaf Bergmann <bergmann@tzi.org>
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

#ifndef _COAP_STR_H_
#define _COAP_STR_H_

#include <string.h>

typedef struct {
  size_t length;    /* length of string */
  unsigned char *s; /* string data */
} str;

#define COAP_SET_STR(st,l,v) { (st)->length = (l), (st)->s = (v); }

/**
 * Returns a new string object with at least size bytes storage allocated. The
 * string must be released using coap_delete_string();
 */
str *coap_new_string(size_t size);

/**
 * Deletes the given string and releases any memory allocated.
 */
void coap_delete_string(str *);

#endif /* _COAP_STR_H_ */
