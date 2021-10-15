/* coap_session.h -- Session management for libcoap
*
* Copyright (C) 2017 Jean-Claue Michelou <jcm@spinetix.com>
*
 * SPDX-License-Identifier: BSD-2-Clause
 *
* This file is part of the CoAP library libcoap. Please see
* README for terms of use.
*/

/**
 * @file coap_session.h
 * @brief Defines the application visible session information
 */

#ifndef COAP_SESSION_H_
#define COAP_SESSION_H_

/**
 * @defgroup session Sessions
 * API functions for CoAP Sessions
 * @{
 */

/**
* Abstraction of a fixed point number that can be used where necessary instead
* of a float.  1,000 fractional bits equals one integer
*/
typedef struct coap_fixed_point_t {
  uint16_t integer_part;    /**< Integer part of fixed point variable */
  uint16_t fractional_part; /**< Fractional part of fixed point variable
                                1/1000 (3 points) precision */
} coap_fixed_point_t;

#define COAP_PROTO_NOT_RELIABLE(p) ((p)==COAP_PROTO_UDP || (p)==COAP_PROTO_DTLS)
#define COAP_PROTO_RELIABLE(p) ((p)==COAP_PROTO_TCP || (p)==COAP_PROTO_TLS)

/**
 * coap_session_type_t values
 */
typedef enum coap_session_type_t {
  COAP_SESSION_TYPE_NONE = 0, /**< Not defined */
  COAP_SESSION_TYPE_CLIENT,   /**< client-side */
  COAP_SESSION_TYPE_SERVER,   /**< server-side */
  COAP_SESSION_TYPE_HELLO,    /**< server-side ephemeral session for
                                   responding to a client hello */
} coap_session_type_t;

/**
 * coap_session_state_t values
 */
typedef enum coap_session_state_t {
  COAP_SESSION_STATE_NONE = 0,
  COAP_SESSION_STATE_CONNECTING,
  COAP_SESSION_STATE_HANDSHAKE,
  COAP_SESSION_STATE_CSM,
  COAP_SESSION_STATE_ESTABLISHED,
} coap_session_state_t;

/**
 * Increment reference counter on a session.
 *
 * @param session The CoAP session.
 * @return same as session
 */
coap_session_t *coap_session_reference(coap_session_t *session);

/**
 * Decrement reference counter on a session.
 * Note that the session may be deleted as a result and should not be used
 * after this call.
 *
 * @param session The CoAP session.
 */
void coap_session_release(coap_session_t *session);

/**
 * Notify session that it has failed.  This cleans up any outstanding / queued
 * transmissions, observations etc..
 *
 * @param session The CoAP session.
 * @param reason The reason why the session was disconnected.
 */
void coap_session_disconnected(coap_session_t *session,
                               coap_nack_reason_t reason);

/**
 * Stores @p data with the given session. This function overwrites any value
 * that has previously been stored with @p session.
 *
 * @param session The CoAP session.
 * @param data The pointer to the data to store.
 */
void coap_session_set_app_data(coap_session_t *session, void *data);

/**
 * Returns any application-specific data that has been stored with @p
 * session using the function coap_session_set_app_data(). This function will
 * return @c NULL if no data has been stored.
 *
 * @param session The CoAP session.
 *
 * @return Pointer to the stored data or @c NULL.
 */
void *coap_session_get_app_data(const coap_session_t *session);

/**
 * Get the remote IP address from the session.
 *
 * @param session The CoAP session.
 *
 * @return The session's remote address or @c NULL on failure.
 */
const coap_address_t *coap_session_get_addr_remote(
                                               const coap_session_t *session);

/**
 * Get the local IP address from the session.
 *
 * @param session The CoAP session.
 *
 * @return The session's local address or @c NULL on failure.
 */
const coap_address_t *coap_session_get_addr_local(
                                               const coap_session_t *session);

/**
 * Get the session protocol type
 *
 * @param session The CoAP session.
 *
 * @return The session's protocol type
 */
coap_proto_t coap_session_get_proto(const coap_session_t *session);

/**
 * Get the session type
 *
 * @param session The CoAP session.
 *
 * @return The session's type
 */
coap_session_type_t coap_session_get_type(const coap_session_t *session);

/**
 * Get the session state
 *
 * @param session The CoAP session.
 *
 * @return The session's state
 */
coap_session_state_t coap_session_get_state(const coap_session_t *session);

/**
 * Get the session if index
 *
 * @param session The CoAP session.
 *
 * @return The session's if index, or @c -1 on error.
 */
int coap_session_get_ifindex(const coap_session_t *session);

/**
 * Get the session TLS security ptr (TLS type dependent)
 *
 * OpenSSL:  SSL*
 * GnuTLS:   gnutls_session_t (implicit *)
 * Mbed TLS: mbedtls_ssl_context*
 * TinyDTLS: struct dtls_context*
 *
 * @param session The CoAP session.
 * @param tls_lib Updated with the library type.
 *
 * @return The session TLS ptr or @c NULL if not set up
 */
void *coap_session_get_tls(const coap_session_t *session,
                           coap_tls_library_t *tls_lib);

/**
 * Get the session context
 *
 * @param session The CoAP session.
 *
 * @return The session's context
 */
coap_context_t *coap_session_get_context(const coap_session_t *session);

/**
 * Set the session type to client. Typically used in a call-home server.
 * The session needs to be of type COAP_SESSION_TYPE_SERVER.
 * Note: If this function is successful, the session reference count is
 * incremented and a subsequent coap_session_release() taking the
 * reference count to 0 will cause the session to be freed off.
 *
 * @param session The CoAP session.
 *
 * @return @c 1 if updated, @c 0 on failure.
 */
int coap_session_set_type_client(coap_session_t *session);

/**
 * Set the session MTU. This is the maximum message size that can be sent,
 * excluding IP and UDP overhead.
 *
 * @param session The CoAP session.
 * @param mtu maximum message size
 */
void coap_session_set_mtu(coap_session_t *session, unsigned mtu);

/**
 * Get maximum acceptable PDU size
 *
 * @param session The CoAP session.
 * @return maximum PDU size, not including header (but including token).
 */
size_t coap_session_max_pdu_size(const coap_session_t *session);

/**
* Creates a new client session to the designated server.
* @param ctx The CoAP context.
* @param local_if Address of local interface. It is recommended to use NULL to let the operating system choose a suitable local interface. If an address is specified, the port number should be zero, which means that a free port is automatically selected.
* @param server The server's address. If the port number is zero, the default port for the protocol will be used.
* @param proto Protocol.
*
* @return A new CoAP session or NULL if failed. Call coap_session_release to free.
*/
coap_session_t *coap_new_client_session(
  coap_context_t *ctx,
  const coap_address_t *local_if,
  const coap_address_t *server,
  coap_proto_t proto
);

/**
* Creates a new client session to the designated server with PSK credentials
* @param ctx The CoAP context.
* @param local_if Address of local interface. It is recommended to use NULL to let the operating system choose a suitable local interface. If an address is specified, the port number should be zero, which means that a free port is automatically selected.
* @param server The server's address. If the port number is zero, the default port for the protocol will be used.
* @param proto Protocol.
* @param identity PSK client identity
* @param key PSK shared key
* @param key_len PSK shared key length
*
* @return A new CoAP session or NULL if failed. Call coap_session_release to free.
*/
coap_session_t *coap_new_client_session_psk(
  coap_context_t *ctx,
  const coap_address_t *local_if,
  const coap_address_t *server,
  coap_proto_t proto,
  const char *identity,
  const uint8_t *key,
  unsigned key_len
);

/**
* Creates a new client session to the designated server with PSK credentials
* @param ctx The CoAP context.
* @param local_if Address of local interface. It is recommended to use NULL to
*                 let the operating system choose a suitable local interface.
*                 If an address is specified, the port number should be zero,
*                 which means that a free port is automatically selected.
* @param server The server's address. If the port number is zero, the default
*               port for the protocol will be used.
* @param proto CoAP Protocol.
* @param setup_data PSK parameters.
*
* @return A new CoAP session or NULL if failed. Call coap_session_release()
*         to free.
*/
coap_session_t *coap_new_client_session_psk2(
  coap_context_t *ctx,
  const coap_address_t *local_if,
  const coap_address_t *server,
  coap_proto_t proto,
  coap_dtls_cpsk_t *setup_data
);

/**
 * Get the server session's current Identity Hint (PSK).
 *
 * @param session  The current coap_session_t object.
 *
 * @return @c hint if successful, else @c NULL.
 */
const coap_bin_const_t * coap_session_get_psk_hint(
                                               const coap_session_t *session);

/**
 * Get the session's current pre-shared key (PSK).
 *
 * @param session  The current coap_session_t object.
 *
 * @return @c psk_key if successful, else @c NULL.
 */
const coap_bin_const_t * coap_session_get_psk_key(
                                               const coap_session_t *session);

/**
* Creates a new client session to the designated server with PKI credentials
* @param ctx The CoAP context.
* @param local_if Address of local interface. It is recommended to use NULL to
*                 let the operating system choose a suitable local interface.
*                 If an address is specified, the port number should be zero,
*                 which means that a free port is automatically selected.
* @param server The server's address. If the port number is zero, the default
*               port for the protocol will be used.
* @param proto CoAP Protocol.
* @param setup_data PKI parameters.
*
* @return A new CoAP session or NULL if failed. Call coap_session_release()
*         to free.
*/
coap_session_t *coap_new_client_session_pki(
  coap_context_t *ctx,
  const coap_address_t *local_if,
  const coap_address_t *server,
  coap_proto_t proto,
  coap_dtls_pki_t *setup_data
);

/**
 * Initializes the token value to use as a starting point.
 *
 * @param session The current coap_session_t object.
 * @param length  The length of the token (0 - 8 bytes).
 * @param token   The token data.
 *
 */
void coap_session_init_token(coap_session_t *session, size_t length,
                             const uint8_t *token);

/**
 * Creates a new token for use.
 *
 * @param session The current coap_session_t object.
 * @param length  Updated with the length of the new token.
 * @param token   Updated with the new token data (must be 8 bytes long).
 *
 */
void coap_session_new_token(coap_session_t *session, size_t *length,
                                      uint8_t *token);

/**
 * @ingroup logging
 * Get session description.
 *
 * @param session  The CoAP session.
 * @return description string.
 */
const char *coap_session_str(const coap_session_t *session);

/**
* Create a new endpoint for communicating with peers.
*
* @param context        The coap context that will own the new endpoint
* @param listen_addr    Address the endpoint will listen for incoming requests on or originate outgoing requests from. Use NULL to specify that no incoming request will be accepted and use a random endpoint.
* @param proto          Protocol used on this endpoint
*/

coap_endpoint_t *coap_new_endpoint(coap_context_t *context, const coap_address_t *listen_addr, coap_proto_t proto);

/**
* Set the endpoint's default MTU. This is the maximum message size that can be
* sent, excluding IP and UDP overhead.
*
* @param endpoint The CoAP endpoint.
* @param mtu maximum message size
*/
void coap_endpoint_set_default_mtu(coap_endpoint_t *endpoint, unsigned mtu);

void coap_free_endpoint(coap_endpoint_t *ep);

/** @} */

/**
 * @ingroup logging
* Get endpoint description.
*
* @param endpoint  The CoAP endpoint.
* @return description string.
*/
const char *coap_endpoint_str(const coap_endpoint_t *endpoint);

coap_session_t *coap_session_get_by_peer(const coap_context_t *ctx,
  const coap_address_t *remote_addr, int ifindex);

 /**
  * @defgroup cc Rate Control
  * The transmission parameters for CoAP rate control ("Congestion
  * Control" in stream-oriented protocols) are defined in
  * https://tools.ietf.org/html/rfc7252#section-4.8
  * @{
  */

  /**
   * Number of seconds when to expect an ACK or a response to an
   * outstanding CON message.
   * RFC 7252, Section 4.8 Default value of ACK_TIMEOUT is 2
   *
   * Configurable using coap_session_set_ack_timeout()
   */
#define COAP_DEFAULT_ACK_TIMEOUT ((coap_fixed_point_t){2,0})

  /**
   * A factor that is used to randomize the wait time before a message
   * is retransmitted to prevent synchronization effects.
   * RFC 7252, Section 4.8 Default value of ACK_RANDOM_FACTOR is 1.5
   *
   * Configurable using coap_session_set_ack_random_factor()
   */
#define COAP_DEFAULT_ACK_RANDOM_FACTOR ((coap_fixed_point_t){1,500})

  /**
   * Number of message retransmissions before message sending is stopped
   * RFC 7252, Section 4.8 Default value of MAX_RETRANSMIT is 4
   *
   * Configurable using coap_session_set_max_retransmit()
   */
#define COAP_DEFAULT_MAX_RETRANSMIT  4

  /**
   * The number of simultaneous outstanding interactions that a client
   * maintains to a given server.
   * RFC 7252, Section 4.8 Default value of NSTART is 1
   */
#define COAP_DEFAULT_NSTART 1

  /**
   * The maximum number of seconds before sending back a response to a
   * multicast request.
   * RFC 7252, Section 4.8 DEFAULT_LEISURE is 5.
   */
#ifndef COAP_DEFAULT_LEISURE
#define COAP_DEFAULT_LEISURE (5U)
#endif /* COAP_DEFAULT_LEISURE */

  /**
   * The MAX_TRANSMIT_SPAN definition for the session (s).
   *
   * RFC 7252, Section 4.8.2 Calculation of MAX_TRAMSMIT_SPAN
   *  ACK_TIMEOUT * ((2 ** (MAX_RETRANSMIT)) - 1) * ACK_RANDOM_FACTOR
   */
#define COAP_MAX_TRANSMIT_SPAN(s) \
 ((s->ack_timeout.integer_part * 1000 + s->ack_timeout.fractional_part) * \
  ((1 << (s->max_retransmit)) -1) * \
  (s->ack_random_factor.integer_part * 1000 + \
   s->ack_random_factor.fractional_part) \
  / 1000000)

  /**
   * The MAX_TRANSMIT_WAIT definition for the session (s).
   *
   * RFC 7252, Section 4.8.2 Calculation of MAX_TRAMSMIT_WAIT
   *  ACK_TIMEOUT * ((2 ** (MAX_RETRANSMIT + 1)) - 1) * ACK_RANDOM_FACTOR
   */
#define COAP_MAX_TRANSMIT_WAIT(s) \
 ((s->ack_timeout.integer_part * 1000 + s->ack_timeout.fractional_part) * \
  ((1 << (s->max_retransmit + 1)) -1) * \
  (s->ack_random_factor.integer_part * 1000 + \
   s->ack_random_factor.fractional_part) \
  / 1000000)

  /**
   * The MAX_LATENCY definition.
   * RFC 7252, Section 4.8.2 MAX_LATENCY is 100.
   */
#define COAP_MAX_LATENCY 100

  /**
   * The PROCESSING_DELAY definition for the session (s).
   *
   * RFC 7252, Section 4.8.2 Calculation of PROCESSING_DELAY
   *  PROCESSING_DELAY set to ACK_TIMEOUT
   */
#define COAP_PROCESSING_DELAY(s) \
 ((s->ack_timeout.integer_part * 1000 + s->ack_timeout.fractional_part + 500) \
  / 1000)

  /**
   * The MAX_RTT definition for the session (s).
   *
   * RFC 7252, Section 4.8.2 Calculation of MAX_RTT
   *  (2 * MAX_LATENCY) + PROCESSING_DELAY
   */
#define COAP_MAX_RTT(s) \
 ((2 * COAP_MAX_LATENCY) + COAP_PROCESSING_DELAY(s))

  /**
   * The EXCHANGE_LIFETIME definition for the session (s).
   *
   * RFC 7252, Section 4.8.2 Calculation of EXCHANGE_LIFETIME
   *  MAX_TRANSMIT_SPAN + (2 * MAX_LATENCY) + PROCESSING_DELAY
   */
#define COAP_EXCHANGE_LIFETIME(s) \
 (COAP_MAX_TRANSMIT_SPAN(s) + (2 * COAP_MAX_LATENCY) + COAP_PROCESSING_DELAY(s))

  /**
   * The NON_LIFETIME definition for the session (s).
   *
   * RFC 7252, Section 4.8.2 Calculation of NON_LIFETIME
   *  MAX_TRANSMIT_SPAN + MAX_LATENCY
   */
#define COAP_NON_LIFETIME(s) \
 (COAP_MAX_TRANSMIT_SPAN(s) + COAP_MAX_LATENCY)

      /** @} */

/**
* Set the CoAP maximum retransmit count before failure
*
* Number of message retransmissions before message sending is stopped
*
* @param session The CoAP session.
* @param value The value to set to. The default is 4 and should not normally
*              get changed.
*/
void coap_session_set_max_retransmit(coap_session_t *session,
                                     unsigned int value);

/**
* Set the CoAP initial ack response timeout before the next re-transmit
*
* Number of seconds when to expect an ACK or a response to an
* outstanding CON message.
*
* @param session The CoAP session.
* @param value The value to set to. The default is 2 and should not normally
*              get changed.
*/
void coap_session_set_ack_timeout(coap_session_t *session,
                                  coap_fixed_point_t value);

/**
* Set the CoAP ack randomize factor
*
* A factor that is used to randomize the wait time before a message
* is retransmitted to prevent synchronization effects.
*
* @param session The CoAP session.
* @param value The value to set to. The default is 1.5 and should not normally
*              get changed.
*/
void coap_session_set_ack_random_factor(coap_session_t *session,
                                        coap_fixed_point_t value);

/**
* Get the CoAP maximum retransmit before failure
*
* Number of message retransmissions before message sending is stopped
*
* @param session The CoAP session.
*
* @return Current maximum retransmit value
*/
unsigned int coap_session_get_max_retransmit(const coap_session_t *session);

/**
* Get the CoAP initial ack response timeout before the next re-transmit
*
* Number of seconds when to expect an ACK or a response to an
* outstanding CON message.
*
* @param session The CoAP session.
*
* @return Current ack response timeout value
*/
coap_fixed_point_t coap_session_get_ack_timeout(const coap_session_t *session);

/**
* Get the CoAP ack randomize factor
*
* A factor that is used to randomize the wait time before a message
* is retransmitted to prevent synchronization effects.
*
* @param session The CoAP session.
*
* @return Current ack randomize value
*/
coap_fixed_point_t coap_session_get_ack_random_factor(
                                               const coap_session_t *session);

/**
 * Send a ping message for the session.
 * @param session The CoAP session.
 *
 * @return COAP_INVALID_MID if there is an error
 */
coap_mid_t coap_session_send_ping(coap_session_t *session);

#endif  /* COAP_SESSION_H */
