/*
 * coap_subscribe_internal.h -- Structures, Enums & Functions that are not
 * exposed to application programming
 *
 * Copyright (C) 2010-2021 Olaf Bergmann <bergmann@tzi.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

/**
 * @file coap_subscribe_internal.h
 * @brief COAP subscribe internal information
 */

#ifndef COAP_SUBSCRIBE_INTERNAL_H_
#define COAP_SUBSCRIBE_INTERNAL_H_

/**
 * @defgroup subscribe_internal Observe Subscription (Internal)
 * CoAP Observe Subscription Structures, Enums and Functions that are not
 * exposed to applications
 * @{
 */

#ifndef COAP_OBS_MAX_NON
/**
 * Number of notifications that may be sent non-confirmable before a confirmable
 * message is sent to detect if observers are alive. The maximum allowed value
 * here is @c 15.
 */
#define COAP_OBS_MAX_NON   5
#endif /* COAP_OBS_MAX_NON */

#ifndef COAP_OBS_MAX_FAIL
/**
 * Number of confirmable notifications that may fail (i.e. time out without
 * being ACKed) before an observer is removed. The maximum value for
 * COAP_OBS_MAX_FAIL is @c 3.
 */
#define COAP_OBS_MAX_FAIL  3
#endif /* COAP_OBS_MAX_FAIL */

/** Subscriber information */
struct coap_subscription_t {
  struct coap_subscription_t *next; /**< next element in linked list */
  struct coap_session_t *session;   /**< subscriber session */

  unsigned int non_cnt:4;  /**< up to 15 non-confirmable notifies allowed */
  unsigned int fail_cnt:2; /**< up to 3 confirmable notifies can fail */
  unsigned int dirty:1;    /**< set if the notification temporarily could not be
                            *   sent (in that case, the resource's partially
                            *   dirty flag is set too) */
  coap_cache_key_t *cache_key; /** cache_key to identify requester */
  coap_pdu_t *pdu;         /**< PDU to use for additional requests */
};

void coap_subscription_init(coap_subscription_t *);

/**
 * Handles a failed observe notify.
 *
 * @param context The context holding the resource.
 * @param session The session that the observe notify failed on.
 * @param token The token used when the observe notify failed.
 */
void
coap_handle_failed_notify(coap_context_t *context,
                          coap_session_t *session,
                          const coap_binary_t *token);

/**
 * Checks all known resources to see if they are dirty and then notifies
 * subscribed observers.
 *
 * @param context The context to check for dirty resources.
 */
void coap_check_notify(coap_context_t *context);

/**
 * Adds the specified peer as observer for @p resource. The subscription is
 * identified by the given @p token. This function returns the registered
 * subscription information if the @p observer has been added, or @c NULL on
 * error.
 *
 * @param resource        The observed resource.
 * @param session         The observer's session
 * @param token           The token that identifies this subscription.
 * @param pdu             The requesting pdu.
 *
 * @return                A pointer to the added/updated subscription
 *                        information or @c NULL on error.
 */
coap_subscription_t *coap_add_observer(coap_resource_t *resource,
                                       coap_session_t *session,
                                       const coap_binary_t *token,
                                       const coap_pdu_t *pdu);

/**
 * Returns a subscription object for given @p peer.
 *
 * @param resource The observed resource.
 * @param session  The observer's session
 * @param token    The token that identifies this subscription or @c NULL for
 *                 any token.
 * @return         A valid subscription if exists or @c NULL otherwise.
 */
coap_subscription_t *coap_find_observer(coap_resource_t *resource,
                                        coap_session_t *session,
                                        const coap_binary_t *token);

/**
 * Flags that data is ready to be sent to observers.
 *
 * @param context  The CoAP context to use.
 * @param session  The observer's session
 * @param token    The corresponding token that has been used for the
 *                 subscription.
 */
void coap_touch_observer(coap_context_t *context,
                         coap_session_t *session,
                         const coap_binary_t *token);

/**
 * Removes any subscription for @p observer from @p resource and releases the
 * allocated storage. The result is @c 1 if an observation relationship with @p
 * observer and @p token existed, @c 0 otherwise.
 *
 * @param resource The observed resource.
 * @param session  The observer's session.
 * @param token    The token that identifies this subscription or @c NULL for
 *                 any token.
 * @return         @c 1 if the observer has been deleted, @c 0 otherwise.
 */
int coap_delete_observer(coap_resource_t *resource,
                         coap_session_t *session,
                         const coap_binary_t *token);

/**
 * Removes any subscription for @p session and releases the allocated storage.
 *
 * @param context  The CoAP context to use.
 * @param session  The observer's session.
 */
void coap_delete_observers(coap_context_t *context, coap_session_t *session);

/** @} */

#endif /* COAP_SUBSCRIBE_INTERNAL_H_ */
