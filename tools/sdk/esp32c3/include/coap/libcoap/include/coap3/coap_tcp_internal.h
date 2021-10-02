/*
 * coap_tcp_internal.h -- TCP functions for libcoap
 *
 * Copyright (C) 2019--2020 Olaf Bergmann <bergmann@tzi.org> and others
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

/**
 * @file coap_tcp_internal.h
 * @brief COAP tcp internal information
 */

#ifndef COAP_TCP_INTERNAL_H_
#define COAP_TCP_INTERNAL_H_

#include "coap_io.h"

/**
 * @defgroup tcp TCP Support (Internal)
 * CoAP TCP Structures, Enums and Functions that are not exposed to
 * applications
 * @{
 */

#if !COAP_DISABLE_TCP

/**
 * Create a new TCP socket and initiate the connection
 *
 * Internal function.
 *
 * @param sock Where socket information is to be filled in
 * @param local_if The local address to use or NULL
 * @param server The address to connect to
 * @param default_port The port to use if not set in @p server
 * @param local_addr Filled in after connection initiation with
 *                   the local address
 * @param remote_addr Filled in after connection initiation with
 *                    the remote address
 *
 * @return @c 1 if succesful, @c 0 if failure of some sort
*/
int
coap_socket_connect_tcp1(coap_socket_t *sock,
                         const coap_address_t *local_if,
                         const coap_address_t *server,
                         int default_port,
                         coap_address_t *local_addr,
                         coap_address_t *remote_addr);

/**
 * Complete the TCP Connection
 *
 * Internal function.
 *
 * @param sock The socket information to use
 * @param local_addr Filled in with the final local address
 * @param remote_addr Filled in with the final remote address
 *
 * @return @c 1 if succesful, @c 0 if failure of some sort
*/
int
coap_socket_connect_tcp2(coap_socket_t *sock,
                         coap_address_t *local_addr,
                         coap_address_t *remote_addr);

/**
 * Create a new TCP socket and then listen for new incoming TCP sessions
 *
 * Internal function.
 *
 * @param sock Where socket information is to be filled in
 * @param listen_addr The address to be listening for new incoming sessions
 * @param bound_addr Filled in with the address that the TCP layer
 *                   is listening on for new incoming TCP sessions
 *
 * @return @c 1 if succesful, @c 0 if failure of some sort
*/
int
coap_socket_bind_tcp(coap_socket_t *sock,
                     const coap_address_t *listen_addr,
                     coap_address_t *bound_addr);

/**
 * Accept a new incoming TCP session
 *
 * Internal function.
 *
 * @param server The socket information to use to accept the TCP connection
 * @param new_client Filled in socket information with the new incoming
 *                   session information
 * @param local_addr Filled in with the local address
 * @param remote_addr Filled in with the remote address
 *
 * @return @c 1 if succesful, @c 0 if failure of some sort
*/
int
coap_socket_accept_tcp(coap_socket_t *server,
                       coap_socket_t *new_client,
                       coap_address_t *local_addr,
                       coap_address_t *remote_addr);

#endif /* !COAP_DISABLE_TCP */

/** @} */

#endif /* COAP_TCP_INTERNAL_H_ */
