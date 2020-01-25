/*
 * subscribe.h -- subscription handling for CoAP
 *                see RFC7641
 *
 * Copyright (C) 2010-2012,2014-2015 Olaf Bergmann <bergmann@tzi.org>
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */


#ifndef COAP_SUBSCRIBE_H_
#define COAP_SUBSCRIBE_H_

#include "address.h"
#include "coap_io.h"
#include "block.h"

/**
 * @defgroup observe Resource observation
 * API functions for interfacing with the observe handling (RFC7641)
 * @{
 */

/**
 * The value COAP_OBSERVE_ESTABLISH in a GET request indicates a new observe
 * relationship for (sender address, token) is requested.
 */
#define COAP_OBSERVE_ESTABLISH 0

/**
 * The value COAP_OBSERVE_CANCEL in a GET request indicates that the observe
 * relationship for (sender address, token) must be cancelled.
 */
#define COAP_OBSERVE_CANCEL 1

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
typedef struct coap_subscription_t {
  struct coap_subscription_t *next; /**< next element in linked list */
  coap_session_t *session;          /**< subscriber session */

  unsigned int non_cnt:4;  /**< up to 15 non-confirmable notifies allowed */
  unsigned int fail_cnt:2; /**< up to 3 confirmable notifies can fail */
  unsigned int dirty:1;    /**< set if the notification temporarily could not be
                            *   sent (in that case, the resource's partially
                            *   dirty flag is set too) */
  unsigned int has_block2:1; /**< GET request had Block2 definition */
  coap_block_t block2;     /**< GET request Block2 definition */
  size_t token_length;     /**< actual length of token */
  unsigned char token[8];  /**< token used for subscription */
  coap_string_t *query;    /**< query string used for subscription, if any */
} coap_subscription_t;

void coap_subscription_init(coap_subscription_t *);

/** @} */

#endif /* COAP_SUBSCRIBE_H_ */
