/* coap_session.h -- Session management for libcoap
*
* Copyright (C) 2017 Jean-Claue Michelou <jcm@spinetix.com>
*
* This file is part of the CoAP library libcoap. Please see
* README for terms of use.
*/

#ifndef COAP_SESSION_H_
#define COAP_SESSION_H_


#include "coap_io.h"
#include "coap_time.h"
#include "pdu.h"

struct coap_endpoint_t;
struct coap_context_t;
struct coap_queue_t;

/**
* Abstraction of a fixed point number that can be used where necessary instead
* of a float.  1,000 fractional bits equals one integer
*/
typedef struct coap_fixed_point_t {
  uint16_t integer_part;    /**< Integer part of fixed point variable */
  uint16_t fractional_part; /**< Fractional part of fixed point variable
                                1/1000 (3 points) precision */
} coap_fixed_point_t;

#define COAP_DEFAULT_SESSION_TIMEOUT 300
#define COAP_PARTIAL_SESSION_TIMEOUT_TICKS (30 * COAP_TICKS_PER_SECOND)
#define COAP_DEFAULT_MAX_HANDSHAKE_SESSIONS 100

#define COAP_PROTO_NOT_RELIABLE(p) ((p)==COAP_PROTO_UDP || (p)==COAP_PROTO_DTLS)
#define COAP_PROTO_RELIABLE(p) ((p)==COAP_PROTO_TCP || (p)==COAP_PROTO_TLS)

typedef uint8_t coap_session_type_t;
/**
 * coap_session_type_t values
 */
#define COAP_SESSION_TYPE_CLIENT 1  /**< client-side */
#define COAP_SESSION_TYPE_SERVER 2  /**< server-side */
#define COAP_SESSION_TYPE_HELLO  3  /**< server-side ephemeral session for responding to a client hello */

typedef uint8_t coap_session_state_t;
/**
 * coap_session_state_t values
 */
#define COAP_SESSION_STATE_NONE                0
#define COAP_SESSION_STATE_CONNECTING        1
#define COAP_SESSION_STATE_HANDSHAKE        2
#define COAP_SESSION_STATE_CSM                3
#define COAP_SESSION_STATE_ESTABLISHED        4

typedef struct coap_session_t {
  struct coap_session_t *next;
  coap_proto_t proto;               /**< protocol used */
  coap_session_type_t type;         /**< client or server side socket */
  coap_session_state_t state;       /**< current state of relationaship with peer */
  unsigned ref;                     /**< reference count from queues */
  unsigned tls_overhead;            /**< overhead of TLS layer */
  unsigned mtu;                     /**< path or CSM mtu */
  coap_address_t local_if;          /**< optional local interface address */
  coap_address_t remote_addr;       /**< remote address and port */
  coap_address_t local_addr;        /**< local address and port */
  int ifindex;                      /**< interface index */
  coap_socket_t sock;               /**< socket object for the session, if any */
  struct coap_endpoint_t *endpoint; /**< session's endpoint */
  struct coap_context_t *context;   /**< session's context */
  void *tls;                        /**< security parameters */
  uint16_t tx_mid;                  /**< the last message id that was used in this session */
  uint8_t con_active;               /**< Active CON request sent */
  struct coap_queue_t *delayqueue;  /**< list of delayed messages waiting to be sent */
  size_t partial_write;             /**< if > 0 indicates number of bytes already written from the pdu at the head of sendqueue */
  uint8_t read_header[8];           /**< storage space for header of incoming message header */
  size_t partial_read;              /**< if > 0 indicates number of bytes already read for an incoming message */
  coap_pdu_t *partial_pdu;          /**< incomplete incoming pdu */
  coap_tick_t last_rx_tx;
  coap_tick_t last_tx_rst;
  coap_tick_t last_ping;
  coap_tick_t last_pong;
  coap_tick_t csm_tx;
  uint8_t *psk_identity;
  size_t psk_identity_len;
  uint8_t *psk_key;
  size_t psk_key_len;
  void *app;                        /**< application-specific data */
  unsigned int max_retransmit;      /**< maximum re-transmit count (default 4) */
  coap_fixed_point_t ack_timeout;   /**< timeout waiting for ack (default 2 secs) */
  coap_fixed_point_t ack_random_factor; /**< ack random factor backoff (default 1.5) */
  unsigned int dtls_timeout_count;      /**< dtls setup retry counter */
  int dtls_event;                       /**< Tracking any (D)TLS events on this sesison */
} coap_session_t;

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
* Stores @p data with the given session. This function overwrites any value
* that has previously been stored with @p session.
*/
void coap_session_set_app_data(coap_session_t *session, void *data);

/**
* Returns any application-specific data that has been stored with @p
* session using the function coap_session_set_app_data(). This function will
* return @c NULL if no data has been stored.
*/
void *coap_session_get_app_data(const coap_session_t *session);

/**
* Notify session that it has failed.
*
* @param session The CoAP session.
* @param reason The reason why the session was disconnected.
*/
void coap_session_disconnected(coap_session_t *session, coap_nack_reason_t reason);

/**
* Notify session transport has just connected and CSM exchange can now start.
*
* @param session The CoAP session.
*/
void coap_session_send_csm(coap_session_t *session);

/**
* Notify session that it has just connected or reconnected.
*
* @param session The CoAP session.
*/
void coap_session_connected(coap_session_t *session);

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
  struct coap_context_t *ctx,
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
  struct coap_context_t *ctx,
  const coap_address_t *local_if,
  const coap_address_t *server,
  coap_proto_t proto,
  const char *identity,
  const uint8_t *key,
  unsigned key_len
);

struct coap_dtls_pki_t;

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
  struct coap_context_t *ctx,
  const coap_address_t *local_if,
  const coap_address_t *server,
  coap_proto_t proto,
  struct coap_dtls_pki_t *setup_data
);

/**
* Creates a new server session for the specified endpoint.
* @param ctx The CoAP context.
* @param ep An endpoint where an incoming connection request is pending.
*
* @return A new CoAP session or NULL if failed. Call coap_session_release to free.
*/
coap_session_t *coap_new_server_session(
  struct coap_context_t *ctx,
  struct coap_endpoint_t *ep
);

/**
* Function interface for datagram data transmission. This function returns
* the number of bytes that have been transmitted, or a value less than zero
* on error.
*
* @param session          Session to send data on.
* @param data             The data to send.
* @param datalen          The actual length of @p data.
*
* @return                 The number of bytes written on success, or a value
*                         less than zero on error.
*/
ssize_t coap_session_send(coap_session_t *session,
  const uint8_t *data, size_t datalen);

/**
* Function interface for stream data transmission. This function returns
* the number of bytes that have been transmitted, or a value less than zero
* on error. The number of bytes written may be less than datalen because of
* congestion control.
*
* @param session          Session to send data on.
* @param data             The data to send.
* @param datalen          The actual length of @p data.
*
* @return                 The number of bytes written on success, or a value
*                         less than zero on error.
*/
ssize_t coap_session_write(coap_session_t *session,
  const uint8_t *data, size_t datalen);

/**
* Send a pdu according to the session's protocol. This function returns
* the number of bytes that have been transmitted, or a value less than zero
* on error.
*
* @param session          Session to send pdu on.
* @param pdu              The pdu to send.
*
* @return                 The number of bytes written on success, or a value
*                         less than zero on error.
*/
ssize_t coap_session_send_pdu(coap_session_t *session, coap_pdu_t *pdu);


/**
 * @ingroup logging
 * Get session description.
 *
 * @param session  The CoAP session.
 * @return description string.
 */
const char *coap_session_str(const coap_session_t *session);

ssize_t
coap_session_delay_pdu(coap_session_t *session, coap_pdu_t *pdu,
                       struct coap_queue_t *node);
/**
* Abstraction of virtual endpoint that can be attached to coap_context_t. The
* tuple (handle, addr) must uniquely identify this endpoint.
*/
typedef struct coap_endpoint_t {
  struct coap_endpoint_t *next;
  struct coap_context_t *context; /**< endpoint's context */
  coap_proto_t proto;             /**< protocol used on this interface */
  uint16_t default_mtu;           /**< default mtu for this interface */
  coap_socket_t sock;             /**< socket object for the interface, if any */
  coap_address_t bind_addr;       /**< local interface address */
  coap_session_t *sessions;       /**< list of active sessions */
  coap_session_t hello;           /**< special session of DTLS hello messages */
} coap_endpoint_t;

/**
* Create a new endpoint for communicating with peers.
*
* @param context        The coap context that will own the new endpoint
* @param listen_addr    Address the endpoint will listen for incoming requests on or originate outgoing requests from. Use NULL to specify that no incoming request will be accepted and use a random endpoint.
* @param proto          Protocol used on this endpoint
*/

coap_endpoint_t *coap_new_endpoint(struct coap_context_t *context, const coap_address_t *listen_addr, coap_proto_t proto);

/**
* Set the endpoint's default MTU. This is the maximum message size that can be
* sent, excluding IP and UDP overhead.
*
* @param endpoint The CoAP endpoint.
* @param mtu maximum message size
*/
void coap_endpoint_set_default_mtu(coap_endpoint_t *endpoint, unsigned mtu);

void coap_free_endpoint(coap_endpoint_t *ep);


/**
 * @ingroup logging
* Get endpoint description.
*
* @param endpoint  The CoAP endpoint.
* @return description string.
*/
const char *coap_endpoint_str(const coap_endpoint_t *endpoint);

/**
* Lookup the server session for the packet received on an endpoint, or create
* a new one.
*
* @param endpoint Active endpoint the packet was received on.
* @param packet Received packet.
* @param now The current time in ticks.
* @return The CoAP session.
*/
coap_session_t *coap_endpoint_get_session(coap_endpoint_t *endpoint,
  const struct coap_packet_t *packet, coap_tick_t now);

/**
 * Create a new DTLS session for the @p endpoint.
 *
 * @ingroup dtls_internal
 *
 * @param endpoint  Endpoint to add DTLS session to
 * @param packet    Received packet information to base session on.
 * @param now       The current time in ticks.
 *
 * @return Created CoAP session or @c NULL if error.
 */
coap_session_t *coap_endpoint_new_dtls_session(coap_endpoint_t *endpoint,
  const struct coap_packet_t *packet, coap_tick_t now);

coap_session_t *coap_session_get_by_peer(struct coap_context_t *ctx,
  const struct coap_address_t *remote_addr, int ifindex);

void coap_session_free(coap_session_t *session);
void coap_session_mfree(coap_session_t *session);

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
   */
#define COAP_DEFAULT_ACK_TIMEOUT ((coap_fixed_point_t){2,0})

   /**
    * A factor that is used to randomize the wait time before a message
    * is retransmitted to prevent synchronization effects.
    * RFC 7252, Section 4.8 Default value of ACK_RANDOM_FACTOR is 1.5
    */
#define COAP_DEFAULT_ACK_RANDOM_FACTOR ((coap_fixed_point_t){1,500})

    /**
     * Number of message retransmissions before message sending is stopped
     * RFC 7252, Section 4.8 Default value of MAX_RETRANSMIT is 4
     */
#define COAP_DEFAULT_MAX_RETRANSMIT  4

     /**
      * The number of simultaneous outstanding interactions that a client
      * maintains to a given server.
      * RFC 7252, Section 4.8 Default value of NSTART is 1
      */
#define COAP_DEFAULT_NSTART 1

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
unsigned int coap_session_get_max_transmit(coap_session_t *session);

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
coap_fixed_point_t coap_session_get_ack_timeout(coap_session_t *session);

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
coap_fixed_point_t coap_session_get_ack_random_factor(coap_session_t *session);

/**
 * Send a ping message for the session.
 * @param session The CoAP session.
 *
 * @return COAP_INVALID_TID if there is an error
 */
coap_tid_t coap_session_send_ping(coap_session_t *session);

#endif  /* COAP_SESSION_H */
