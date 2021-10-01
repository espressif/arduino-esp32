/*
 * coap_async_internal.h -- state management for asynchronous messages
 *
 * Copyright (C) 2010-2021 Olaf Bergmann <bergmann@tzi.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

/**
 * @file coap_async_internal.h
 * @brief CoAP async internal information
 */

#ifndef COAP_ASYNC_INTERNAL_H_
#define COAP_ASYNC_INTERNAL_H_

#include "coap3/net.h"

#ifndef WITHOUT_ASYNC

/**
 * @defgroup coap_async_internal Asynchronous Messaging (Internal)
 * @{
 * CoAP Async Structures, Enums and Functions that are not exposed to
 * applications.
 * A coap_context_t object holds a list of coap_async_t objects that can be
 * used to generate a separate response in the case a result of a request cannot
 * be delivered immediately.
 */
struct coap_async_t {
  struct coap_async_t *next; /**< internally used for linking */
  coap_tick_t delay;    /**< When to delay to before triggering the response
                             0 indicates never trigger */
  coap_session_t *session;         /**< transaction session */
  coap_pdu_t *pdu;                 /**< copy of request pdu */
  void* appdata;                   /** User definable data pointer */
};

/**
 * Checks if there are any pending Async requests - if so, send them off.
 * Otherewise return the time remaining for the next Async to be triggered
 * or 0 if nothing to do.
 *
 * @param context The current context.
 * @param now     The current time in ticks.
 *
 * @return The tick time before the next Async needs to go, else 0 if
 *         nothing to do.
 */
coap_tick_t coap_check_async(coap_context_t *context, coap_tick_t now);

/**
 * Removes and frees off all of the async entries for the given context.
 *
 * @param context The context to remove all async entries from.
 */
void
coap_delete_all_async(coap_context_t *context);

/** @} */

#endif /*  WITHOUT_ASYNC */

#endif /* COAP_ASYNC_INTERNAL_H_ */
