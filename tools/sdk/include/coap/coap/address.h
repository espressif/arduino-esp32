/*
 * address.h -- representation of network addresses
 *
 * Copyright (C) 2010-2011,2015-2016 Olaf Bergmann <bergmann@tzi.org>
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

/**
 * @file address.h
 * @brief Representation of network addresses
 */

#ifndef _COAP_ADDRESS_H_
#define _COAP_ADDRESS_H_

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include "libcoap.h"

#ifdef WITH_LWIP
#include <lwip/ip_addr.h>

typedef struct coap_address_t {
  uint16_t port;
  ip_addr_t addr;
} coap_address_t;

#define _coap_address_equals_impl(A, B)   (!!ip_addr_cmp(&(A)->addr,&(B)->addr))

#define _coap_address_isany_impl(A)  ip_addr_isany(&(A)->addr)

#define _coap_is_mcast_impl(Address) ip_addr_ismulticast(&(Address)->addr)
#endif /* WITH_LWIP */

#ifdef WITH_CONTIKI
#include "uip.h"

typedef struct coap_address_t {
  uip_ipaddr_t addr;
  unsigned short port;
} coap_address_t;

#define _coap_address_equals_impl(A,B) \
        ((A)->port == (B)->port        \
        && uip_ipaddr_cmp(&((A)->addr),&((B)->addr)))

/** @todo implementation of _coap_address_isany_impl() for Contiki */
#define _coap_address_isany_impl(A)  0

#define _coap_is_mcast_impl(Address) uip_is_addr_mcast(&((Address)->addr))
#endif /* WITH_CONTIKI */

#ifdef WITH_POSIX
/** multi-purpose address abstraction */
typedef struct coap_address_t {
  socklen_t size;           /**< size of addr */
  union {
    struct sockaddr         sa;
    struct sockaddr_storage st;
    struct sockaddr_in      sin;
    struct sockaddr_in6     sin6;
  } addr;
} coap_address_t;

/**
 * Compares given address objects @p a and @p b. This function returns @c 1 if
 * addresses are equal, @c 0 otherwise. The parameters @p a and @p b must not be
 * @c NULL;
 */
int coap_address_equals(const coap_address_t *a, const coap_address_t *b);

static inline int
_coap_address_isany_impl(const coap_address_t *a) {
  /* need to compare only relevant parts of sockaddr_in6 */
  switch (a->addr.sa.sa_family) {
  case AF_INET:
    return a->addr.sin.sin_addr.s_addr == INADDR_ANY;
  case AF_INET6:
    return memcmp(&in6addr_any,
                  &a->addr.sin6.sin6_addr,
                  sizeof(in6addr_any)) == 0;
  default:
    ;
  }

  return 0;
}
#endif /* WITH_POSIX */

/**
 * Resets the given coap_address_t object @p addr to its default values. In
 * particular, the member size must be initialized to the available size for
 * storing addresses.
 *
 * @param addr The coap_address_t object to initialize.
 */
static inline void
coap_address_init(coap_address_t *addr) {
  assert(addr);
  memset(addr, 0, sizeof(coap_address_t));
#ifdef WITH_POSIX
  /* lwip and Contiki have constant address sizes and doesn't need the .size part */
  addr->size = sizeof(addr->addr);
#endif
}

#ifndef WITH_POSIX
/**
 * Compares given address objects @p a and @p b. This function returns @c 1 if
 * addresses are equal, @c 0 otherwise. The parameters @p a and @p b must not be
 * @c NULL;
 */
static inline int
coap_address_equals(const coap_address_t *a, const coap_address_t *b) {
  assert(a); assert(b);
  return _coap_address_equals_impl(a, b);
}
#endif

/**
 * Checks if given address object @p a denotes the wildcard address. This
 * function returns @c 1 if this is the case, @c 0 otherwise. The parameters @p
 * a must not be @c NULL;
 */
static inline int
coap_address_isany(const coap_address_t *a) {
  assert(a);
  return _coap_address_isany_impl(a);
}

#ifdef WITH_POSIX
/**
 * Checks if given address @p a denotes a multicast address. This function
 * returns @c 1 if @p a is multicast, @c 0 otherwise.
 */
int coap_is_mcast(const coap_address_t *a);
#else /* WITH_POSIX */
/**
 * Checks if given address @p a denotes a multicast address. This function
 * returns @c 1 if @p a is multicast, @c 0 otherwise.
 */
static inline int
coap_is_mcast(const coap_address_t *a) {
  return a && _coap_is_mcast_impl(a);
}
#endif /* WITH_POSIX */

#endif /* _COAP_ADDRESS_H_ */
