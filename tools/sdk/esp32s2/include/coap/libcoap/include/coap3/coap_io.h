/*
 * coap_io.h -- Default network I/O functions for libcoap
 *
 * Copyright (C) 2012-2013 Olaf Bergmann <bergmann@tzi.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

#ifndef COAP_IO_H_
#define COAP_IO_H_

#include <sys/types.h>

#include "address.h"

#ifdef RIOT_VERSION
#include "net/gnrc.h"
#endif /* RIOT_VERSION */

#ifndef COAP_RXBUFFER_SIZE
#define COAP_RXBUFFER_SIZE 1472
#endif /* COAP_RXBUFFER_SIZE */

/*
 * It may may make sense to define this larger on busy systems
 * (lots of sessions, large number of which are active), by using
 * -DCOAP_MAX_EPOLL_EVENTS=nn at compile time.
 */
#ifndef COAP_MAX_EPOLL_EVENTS
#define COAP_MAX_EPOLL_EVENTS 10
#endif /* COAP_MAX_EPOLL_EVENTS */

#ifdef _WIN32
typedef SOCKET coap_fd_t;
#define coap_closesocket closesocket
#define COAP_SOCKET_ERROR SOCKET_ERROR
#define COAP_INVALID_SOCKET INVALID_SOCKET
#else
typedef int coap_fd_t;
#define coap_closesocket close
#define COAP_SOCKET_ERROR (-1)
#define COAP_INVALID_SOCKET (-1)
#endif

typedef uint16_t coap_socket_flags_t;

typedef struct coap_addr_tuple_t {
  coap_address_t remote;       /**< remote address and port */
  coap_address_t local;        /**< local address and port */
} coap_addr_tuple_t;

const char *coap_socket_strerror( void );

/**
 * Check whether TCP is available.
 *
 * @return @c 1 if support for TCP is enabled, or @c 0 otherwise.
 */
int coap_tcp_is_supported(void);

typedef enum {
  COAP_NACK_TOO_MANY_RETRIES,
  COAP_NACK_NOT_DELIVERABLE,
  COAP_NACK_RST,
  COAP_NACK_TLS_FAILED,
  COAP_NACK_ICMP_ISSUE
} coap_nack_reason_t;

#endif /* COAP_IO_H_ */
