/*
 * coap_event.h -- libcoap Event API
 *
 * Copyright (C) 2016 Olaf Bergmann <bergmann@tzi.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

#ifndef COAP_EVENT_H_
#define COAP_EVENT_H_

#include "libcoap.h"

/**
 * @defgroup events Event API
 * API functions for event delivery from lower-layer library functions.
 * @{
 */

/**
 * Scalar type to represent different events, e.g. DTLS events or
 * retransmission timeouts.
 */
typedef enum coap_event_t {
/**
 * (D)TLS events for COAP_PROTO_DTLS and COAP_PROTO_TLS
 */
  COAP_EVENT_DTLS_CLOSED       = 0x0000,
  COAP_EVENT_DTLS_CONNECTED    = 0x01DE,
  COAP_EVENT_DTLS_RENEGOTIATE  = 0x01DF,
  COAP_EVENT_DTLS_ERROR        = 0x0200,

/**
 * TCP events for COAP_PROTO_TCP and COAP_PROTO_TLS
 */
  COAP_EVENT_TCP_CONNECTED     = 0x1001,
  COAP_EVENT_TCP_CLOSED        = 0x1002,
  COAP_EVENT_TCP_FAILED        = 0x1003,

/**
 * CSM exchange events for reliable protocols only
 */
  COAP_EVENT_SESSION_CONNECTED = 0x2001,
  COAP_EVENT_SESSION_CLOSED    = 0x2002,
  COAP_EVENT_SESSION_FAILED    = 0x2003,

/**
 * (Q-)BLOCK receive errors
 */
  COAP_EVENT_PARTIAL_BLOCK     = 0x3001
} coap_event_t;

/**
 * Type for event handler functions that can be registered with a CoAP
 * context using the unction coap_set_event_handler(). When called by
 * the library, the first argument will be the current coap_session_t object
 * which is associated with the original CoAP context. The second parameter
 * is the event type.
 */
typedef int (*coap_event_handler_t)(coap_session_t *session,
                                    const coap_event_t event);

/**
 * Registers the function @p hnd as callback for events from the given
 * CoAP context @p context. Any event handler that has previously been
 * registered with @p context will be overwritten by this operation.
 *
 * @param context The CoAP context to register the event handler with.
 * @param hnd     The event handler to be registered.  @c NULL if to be
 *                de-registered.
 */
void coap_register_event_handler(coap_context_t *context,
                            coap_event_handler_t hnd);

/** @} */

/**
 * Registers the function @p hnd as callback for events from the given
 * CoAP context @p context. Any event handler that has previously been
 * registered with @p context will be overwritten by this operation.
 *
 * @deprecated Use coap_register_event_handler() instead.
 *
 * @param context The CoAP context to register the event handler with.
 * @param hnd     The event handler to be registered.
 */
COAP_DEPRECATED
void coap_set_event_handler(coap_context_t *context,
                            coap_event_handler_t hnd);

/**
 * Clears the event handler registered with @p context.
 *
 * @deprecated Use coap_register_event_handler() instead with NULL for hnd.
 *
 * @param context The CoAP context whose event handler is to be removed.
 */
COAP_DEPRECATED
void coap_clear_event_handler(coap_context_t *context);

#endif /* COAP_EVENT_H */
