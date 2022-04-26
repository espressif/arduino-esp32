/*
 * libcoap.h -- platform specific header file for CoAP stack
 *
 * Copyright (C) 2015 Carsten Schoenert <c.schoenert@t-online.de>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

#ifndef COAP_LIBCOAP_H_
#define COAP_LIBCOAP_H_

/* The non posix embedded platforms like Contiki, TinyOS, RIOT, ... doesn't have
 * a POSIX compatible header structure so we have to slightly do some platform
 * related things. Currently there is only Contiki available so we check for a
 * CONTIKI environment and do *not* include the POSIX related network stuff. If
 * there are other platforms in future there need to be analogous environments.
 *
 * The CONTIKI variable is within the Contiki build environment! */

#if defined(_WIN32)
#pragma comment(lib,"Ws2_32.lib")
#include <ws2tcpip.h>
typedef SSIZE_T ssize_t;
typedef USHORT in_port_t;
#elif !defined (CONTIKI)
#include <netinet/in.h>
#include <sys/socket.h>
#endif /* CONTIKI */

#ifndef COAP_STATIC_INLINE
#  if defined(__cplusplus)
#    define COAP_STATIC_INLINE inline
#  else
#    if defined(_MSC_VER)
#      define COAP_STATIC_INLINE static __inline
#    else
#      define COAP_STATIC_INLINE static inline
#    endif
#  endif
#endif
#ifndef COAP_DEPRECATED
#  if defined(_MSC_VER)
#    define COAP_DEPRECATED __declspec(deprecated)
#  else
#    define COAP_DEPRECATED __attribute__ ((deprecated))
#  endif
#endif
#ifndef COAP_UNUSED
#  ifdef __GNUC__
#    define COAP_UNUSED __attribute__((unused))
#  else /* __GNUC__ */
#    define COAP_UNUSED
#  endif /* __GNUC__ */
#endif /* COAP_UNUSED */

void coap_startup(void);

void coap_cleanup(void);

#endif /* COAP_LIBCOAP_H_ */
