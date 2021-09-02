/*
 * address.h -- representation of network addresses
 *
 * Copyright (C) 2010-2011,2015-2016 Olaf Bergmann <bergmann@tzi.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

/**
 * @file address.h
 * @brief Representation of network addresses
 */

#ifndef COAP_ADDRESS_H_
#define COAP_ADDRESS_H_

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include "libcoap.h"

#if defined(WITH_LWIP)

#include <lwip/ip_addr.h>

typedef struct coap_address_t {
  uint16_t port;
  ip_addr_t addr;
} coap_address_t;

/**
 * Returns the port from @p addr in host byte order.
 */
COAP_STATIC_INLINE uint16_t
coap_address_get_port(const coap_address_t *addr) {
  return ntohs(addr->port);
}

/**
 * Sets the port field of @p addr to @p port (in host byte order).
 */
COAP_STATIC_INLINE void
coap_address_set_port(coap_address_t *addr, uint16_t port) {
  addr->port = htons(port);
}

#define _coap_address_equals_impl(A, B) \
        ((A)->port == (B)->port        \
        && (!!ip_addr_cmp(&(A)->addr,&(B)->addr)))

#define _coap_address_isany_impl(A)  ip_addr_isany(&(A)->addr)

#define _coap_is_mcast_impl(Address) ip_addr_ismulticast(&(Address)->addr)

#elif defined(WITH_CONTIKI)

#include "uip.h"

typedef struct coap_address_t {
  uip_ipaddr_t addr;
  uint16_t port;
} coap_address_t;

/**
 * Returns the port from @p addr in host byte order.
 */
COAP_STATIC_INLINE uint16_t
coap_address_get_port(const coap_address_t *addr) {
  return uip_ntohs(addr->port);
}

/**
 * Sets the port field of @p addr to @p port (in host byte order).
 */
COAP_STATIC_INLINE void
coap_address_set_port(coap_address_t *addr, uint16_t port) {
  addr->port = uip_htons(port);
}

#define _coap_address_equals_impl(A,B) \
        ((A)->port == (B)->port        \
        && uip_ipaddr_cmp(&((A)->addr),&((B)->addr)))

/** @todo implementation of _coap_address_isany_impl() for Contiki */
#define _coap_address_isany_impl(A)  0

#define _coap_is_mcast_impl(Address) uip_is_addr_mcast(&((Address)->addr))

#else /* WITH_LWIP || WITH_CONTIKI */

 /** multi-purpose address abstraction */
typedef struct coap_address_t {
  socklen_t size;           /**< size of addr */
  union {
    struct sockaddr         sa;
    struct sockaddr_in      sin;
    struct sockaddr_in6     sin6;
  } addr;
} coap_address_t;

/**
 * Returns the port from @p addr in host byte order.
 */
uint16_t coap_address_get_port(const coap_address_t *addr);

/**
 * Set the port field of @p addr to @p port (in host byte order).
 */
void coap_address_set_port(coap_address_t *addr, uint16_t port);

/**
 * Compares given address objects @p a and @p b. This function returns @c 1 if
 * addresses are equal, @c 0 otherwise. The parameters @p a and @p b must not be
 * @c NULL;
 */
int coap_address_equals(const coap_address_t *a, const coap_address_t *b);

COAP_STATIC_INLINE int
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
#endif /* WITH_LWIP || WITH_CONTIKI */

/**
 * Resets the given coap_address_t object @p addr to its default values. In
 * particular, the member size must be initialized to the available size for
 * storing addresses.
 *
 * @param addr The coap_address_t object to initialize.
 */
void coap_address_init(coap_address_t *addr);

/* Convenience function to copy IPv6 addresses without garbage. */

COAP_STATIC_INLINE void
coap_address_copy( coap_address_t *dst, const coap_address_t *src ) {
#if defined(WITH_LWIP) || defined(WITH_CONTIKI)
  memcpy( dst, src, sizeof( coap_address_t ) );
#else
  memset( dst, 0, sizeof( coap_address_t ) );
  dst->size = src->size;
  if ( src->addr.sa.sa_family == AF_INET6 ) {
    dst->addr.sin6.sin6_family = src->addr.sin6.sin6_family;
    dst->addr.sin6.sin6_addr = src->addr.sin6.sin6_addr;
    dst->addr.sin6.sin6_port = src->addr.sin6.sin6_port;
    dst->addr.sin6.sin6_scope_id = src->addr.sin6.sin6_scope_id;
  } else if ( src->addr.sa.sa_family == AF_INET ) {
    dst->addr.sin = src->addr.sin;
  } else {
    memcpy( &dst->addr, &src->addr, src->size );
  }
#endif
}

#if defined(WITH_LWIP) || defined(WITH_CONTIKI)
/**
 * Compares given address objects @p a and @p b. This function returns @c 1 if
 * addresses are equal, @c 0 otherwise. The parameters @p a and @p b must not be
 * @c NULL;
 */
COAP_STATIC_INLINE int
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
COAP_STATIC_INLINE int
coap_address_isany(const coap_address_t *a) {
  assert(a);
  return _coap_address_isany_impl(a);
}

#if !defined(WITH_LWIP) && !defined(WITH_CONTIKI)

/**
 * Checks if given address @p a denotes a multicast address. This function
 * returns @c 1 if @p a is multicast, @c 0 otherwise.
 */
int coap_is_mcast(const coap_address_t *a);
#else /* !WITH_LWIP && !WITH_CONTIKI */
/**
 * Checks if given address @p a denotes a multicast address. This function
 * returns @c 1 if @p a is multicast, @c 0 otherwise.
 */
COAP_STATIC_INLINE int
coap_is_mcast(const coap_address_t *a) {
  return a && _coap_is_mcast_impl(a);
}
#endif /* !WITH_LWIP && !WITH_CONTIKI */

#endif /* COAP_ADDRESS_H_ */
