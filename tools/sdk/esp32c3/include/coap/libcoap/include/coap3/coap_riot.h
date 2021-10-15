/* coap_riot.h -- RIOT-specific definitions for libcoap
 *
 * Copyright (C) 2019 Olaf Bergmann <bergmann@tzi.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This file is part of the CoAP library libcoap. Please see
 * README for terms of use.
 */

#ifndef COAP_RIOT_H_
#define COAP_RIOT_H_

#ifndef LIBCOAP_MSG_QUEUE_SIZE
/**
 * Size of the queue for passing messages between the network
 * interface and the coap stack. */
#define LIBCOAP_MSG_QUEUE_SIZE (32U)
#endif /* LIBCOAP_MSG_QUEUE_SIZE */

#ifndef LIBCOAP_MAX_SOCKETS
/**
 * Maximum number of sockets that are simultaneously considered for
 * reading or writing. */
#define LIBCOAP_MAX_SOCKETS (16U)
#endif /* LIBCOAP_MAX_SOCKETS */

/**
 * This function must be called in the RIOT CoAP thread for
 * RIOT-specific initialization.
 */
void coap_riot_startup(void);

#endif /* COAP_RIOT_H_ */
