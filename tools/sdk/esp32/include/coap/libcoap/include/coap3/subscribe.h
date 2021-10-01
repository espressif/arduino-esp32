/*
 * subscribe.h -- subscription handling for CoAP
 *                see RFC7641
 *
 * Copyright (C) 2010-2012,2014-2021 Olaf Bergmann <bergmann@tzi.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

/**
 * @file subscribe.h
 * @brief Defines the application visible subscribe information
 */

#ifndef COAP_SUBSCRIBE_H_
#define COAP_SUBSCRIBE_H_

/**
 * @defgroup observe Resource Observation
 * API functions for interfacing with the observe handling (RFC7641)
 * @{
 */

/**
 * The value COAP_OBSERVE_ESTABLISH in a GET/FETCH request option
 * COAP_OPTION_OBSERVE indicates a new observe relationship for (sender
 * address, token) is requested.
 */
#define COAP_OBSERVE_ESTABLISH 0

/**
 * The value COAP_OBSERVE_CANCEL in a GET/FETCH request option
 * COAP_OPTION_OBSERVE indicates that the observe relationship for (sender
 * address, token) must be cancelled.
 */
#define COAP_OBSERVE_CANCEL 1

/**
 * Set whether a @p resource is observable.  If the resource is observable
 * and the client has set the COAP_OPTION_OBSERVE in a request packet, then
 * whenever the state of the resource changes (a call to
 * coap_resource_trigger_observe()), an Observer response will get sent.
 *
 * @param resource The CoAP resource to use.
 * @param mode     @c 1 if Observable is to be set, @c 0 otherwise.
 *
 */
void coap_resource_set_get_observable(coap_resource_t *resource, int mode);

/**
 * Initiate the sending of an Observe packet for all observers of @p resource,
 * optionally matching @p query if not NULL
 *
 * @param resource The CoAP resource to use.
 * @param query    The Query to match against or NULL
 *
 * @return         @c 1 if the Observe has been triggered, @c 0 otherwise.
 */
int
coap_resource_notify_observers(coap_resource_t *resource,
                               const coap_string_t *query);

/** @} */

#endif /* COAP_SUBSCRIBE_H_ */
