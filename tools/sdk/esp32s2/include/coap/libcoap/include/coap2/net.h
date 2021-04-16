/*
 * net.h -- CoAP network interface
 *
 * Copyright (C) 2010-2015 Olaf Bergmann <bergmann@tzi.org>
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

#ifndef COAP_NET_H_
#define COAP_NET_H_

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <sys/time.h>
#endif
#include <time.h>

#ifdef WITH_LWIP
#include <lwip/ip_addr.h>
#endif

#include "coap_io.h"
#include "coap_dtls.h"
#include "coap_event.h"
#include "coap_time.h"
#include "option.h"
#include "pdu.h"
#include "prng.h"
#include "coap_session.h"

struct coap_queue_t;

/**
 * Queue entry
 */
typedef struct coap_queue_t {
  struct coap_queue_t *next;
  coap_tick_t t;                /**< when to send PDU for the next time */
  unsigned char retransmit_cnt; /**< retransmission counter, will be removed
                                 *    when zero */
  unsigned int timeout;         /**< the randomized timeout value */
  coap_session_t *session;      /**< the CoAP session */
  coap_tid_t id;                /**< CoAP transaction id */
  coap_pdu_t *pdu;              /**< the CoAP PDU to send */
} coap_queue_t;

/**
 * Adds @p node to given @p queue, ordered by variable t in @p node.
 *
 * @param queue Queue to add to.
 * @param node Node entry to add to Queue.
 *
 * @return @c 1 added to queue, @c 0 failure.
 */
int coap_insert_node(coap_queue_t **queue, coap_queue_t *node);

/**
 * Destroys specified @p node.
 *
 * @param node Node entry to remove.
 *
 * @return @c 1 node deleted from queue, @c 0 failure.
 */
int coap_delete_node(coap_queue_t *node);

/**
 * Removes all items from given @p queue and frees the allocated storage.
 *
 * @param queue The queue to delete.
 */
void coap_delete_all(coap_queue_t *queue);

/**
 * Creates a new node suitable for adding to the CoAP sendqueue.
 *
 * @return New node entry, or @c NULL if failure.
 */
coap_queue_t *coap_new_node(void);

struct coap_resource_t;
struct coap_context_t;
#ifndef WITHOUT_ASYNC
struct coap_async_state_t;
#endif

/**
 * Response handler that is used as call-back in coap_context_t.
 *
 * @param context CoAP session.
 * @param session CoAP session.
 * @param sent The PDU that was transmitted.
 * @param received The PDU that was received.
 * @param id CoAP transaction ID.
 */
typedef void (*coap_response_handler_t)(struct coap_context_t *context,
                                        coap_session_t *session,
                                        coap_pdu_t *sent,
                                        coap_pdu_t *received,
                                        const coap_tid_t id);

/**
 * Negative Acknowedge handler that is used as call-back in coap_context_t.
 *
 * @param context CoAP session.
 * @param session CoAP session.
 * @param sent The PDU that was transmitted.
 * @param reason The reason for the NACK.
 * @param id CoAP transaction ID.
 */
typedef void (*coap_nack_handler_t)(struct coap_context_t *context,
                                    coap_session_t *session,
                                    coap_pdu_t *sent,
                                    coap_nack_reason_t reason,
                                    const coap_tid_t id);

/**
 * Recieved Ping handler that is used as call-back in coap_context_t.
 *
 * @param context CoAP session.
 * @param session CoAP session.
 * @param received The PDU that was received.
 * @param id CoAP transaction ID.
 */
typedef void (*coap_ping_handler_t)(struct coap_context_t *context,
                                    coap_session_t *session,
                                    coap_pdu_t *received,
                                    const coap_tid_t id);

/**
 * Recieved Pong handler that is used as call-back in coap_context_t.
 *
 * @param context CoAP session.
 * @param session CoAP session.
 * @param received The PDU that was received.
 * @param id CoAP transaction ID.
 */
typedef void (*coap_pong_handler_t)(struct coap_context_t *context,
                                    coap_session_t *session,
                                    coap_pdu_t *received,
                                    const coap_tid_t id);

/**
 * The CoAP stack's global state is stored in a coap_context_t object.
 */
typedef struct coap_context_t {
  coap_opt_filter_t known_options;
  struct coap_resource_t *resources; /**< hash table or list of known
                                          resources */
  struct coap_resource_t *unknown_resource; /**< can be used for handling
                                                 unknown resources */

#ifndef WITHOUT_ASYNC
  /**
   * list of asynchronous transactions */
  struct coap_async_state_t *async_state;
#endif /* WITHOUT_ASYNC */

  /**
   * The time stamp in the first element of the sendqeue is relative
   * to sendqueue_basetime. */
  coap_tick_t sendqueue_basetime;
  coap_queue_t *sendqueue;
  coap_endpoint_t *endpoint;      /**< the endpoints used for listening  */
  coap_session_t *sessions;       /**< client sessions */

#ifdef WITH_CONTIKI
  struct uip_udp_conn *conn;      /**< uIP connection object */
  struct etimer retransmit_timer; /**< fires when the next packet must be sent */
  struct etimer notify_timer;     /**< used to check resources periodically */
#endif /* WITH_CONTIKI */

#ifdef WITH_LWIP
  uint8_t timer_configured;       /**< Set to 1 when a retransmission is
                                   *   scheduled using lwIP timers for this
                                   *   context, otherwise 0. */
#endif /* WITH_LWIP */

  coap_response_handler_t response_handler;
  coap_nack_handler_t nack_handler;
  coap_ping_handler_t ping_handler;
  coap_pong_handler_t pong_handler;

  /**
   * Callback function that is used to signal events to the
   * application.  This field is set by coap_set_event_handler().
   */
  coap_event_handler_t handle_event;

  ssize_t (*network_send)(coap_socket_t *sock, const coap_session_t *session, const uint8_t *data, size_t datalen);

  ssize_t (*network_read)(coap_socket_t *sock, struct coap_packet_t *packet);

  size_t(*get_client_psk)(const coap_session_t *session, const uint8_t *hint, size_t hint_len, uint8_t *identity, size_t *identity_len, size_t max_identity_len, uint8_t *psk, size_t max_psk_len);
  size_t(*get_server_psk)(const coap_session_t *session, const uint8_t *identity, size_t identity_len, uint8_t *psk, size_t max_psk_len);
  size_t(*get_server_hint)(const coap_session_t *session, uint8_t *hint, size_t max_hint_len);

  void *dtls_context;
  uint8_t *psk_hint;
  size_t psk_hint_len;
  uint8_t *psk_key;
  size_t psk_key_len;

  unsigned int session_timeout;    /**< Number of seconds of inactivity after which an unused session will be closed. 0 means use default. */
  unsigned int max_idle_sessions;  /**< Maximum number of simultaneous unused sessions per endpoint. 0 means no maximum. */
  unsigned int max_handshake_sessions; /**< Maximum number of simultaneous negotating sessions per endpoint. 0 means use default. */
  unsigned int ping_timeout;           /**< Minimum inactivity time before sending a ping message. 0 means disabled. */
  unsigned int csm_timeout;           /**< Timeout for waiting for a CSM from the remote side. 0 means disabled. */

  void *app;                       /**< application-specific data */
} coap_context_t;

/**
 * Registers a new message handler that is called whenever a response was
 * received that matches an ongoing transaction.
 *
 * @param context The context to register the handler for.
 * @param handler The response handler to register.
 */
COAP_STATIC_INLINE void
coap_register_response_handler(coap_context_t *context,
                               coap_response_handler_t handler) {
  context->response_handler = handler;
}

/**
 * Registers a new message handler that is called whenever a confirmable
 * message (request or response) is dropped after all retries have been
 * exhausted, or a rst message was received, or a network or TLS level
 * event was received that indicates delivering the message is not possible.
 *
 * @param context The context to register the handler for.
 * @param handler The nack handler to register.
 */
COAP_STATIC_INLINE void
coap_register_nack_handler(coap_context_t *context,
                           coap_nack_handler_t handler) {
  context->nack_handler = handler;
}

/**
 * Registers a new message handler that is called whenever a CoAP Ping
 * message is received.
 *
 * @param context The context to register the handler for.
 * @param handler The ping handler to register.
 */
COAP_STATIC_INLINE void
coap_register_ping_handler(coap_context_t *context,
                           coap_ping_handler_t handler) {
  context->ping_handler = handler;
}

/**
 * Registers a new message handler that is called whenever a CoAP Pong
 * message is received.
 *
 * @param context The context to register the handler for.
 * @param handler The pong handler to register.
 */
COAP_STATIC_INLINE void
coap_register_pong_handler(coap_context_t *context,
                           coap_pong_handler_t handler) {
  context->pong_handler = handler;
}

/**
 * Registers the option type @p type with the given context object @p ctx.
 *
 * @param ctx  The context to use.
 * @param type The option type to register.
 */
COAP_STATIC_INLINE void
coap_register_option(coap_context_t *ctx, uint16_t type) {
  coap_option_setb(ctx->known_options, type);
}

/**
 * Set sendqueue_basetime in the given context object @p ctx to @p now. This
 * function returns the number of elements in the queue head that have timed
 * out.
 */
unsigned int coap_adjust_basetime(coap_context_t *ctx, coap_tick_t now);

/**
 * Returns the next pdu to send without removing from sendqeue.
 */
coap_queue_t *coap_peek_next( coap_context_t *context );

/**
 * Returns the next pdu to send and removes it from the sendqeue.
 */
coap_queue_t *coap_pop_next( coap_context_t *context );

/**
 * Creates a new coap_context_t object that will hold the CoAP stack status.
 */
coap_context_t *coap_new_context(const coap_address_t *listen_addr);

/**
 * Set the context's default PSK hint and/or key for a server.
 *
 * @param context The current coap_context_t object.
 * @param hint    The default PSK server hint sent to a client. If @p NULL, PSK
 *                authentication is disabled. Empty string is a valid hint.
 * @param key     The default PSK key. If @p NULL, PSK authentication will fail.
 * @param key_len The default PSK key's length. If @p 0, PSK authentication will
 *                fail.
 *
 * @return @c 1 if successful, else @c 0.
 */
int coap_context_set_psk( coap_context_t *context, const char *hint,
                           const uint8_t *key, size_t key_len );

/**
 * Set the context's default PKI information for a server.
 *
 * @param context        The current coap_context_t object.
 * @param setup_data     If @p NULL, PKI authentication will fail. Certificate
 *                       information required.
 *
 * @return @c 1 if successful, else @c 0.
 */
int
coap_context_set_pki(coap_context_t *context,
                     coap_dtls_pki_t *setup_data);

/**
 * Set the context's default Root CA information for a client or server.
 *
 * @param context        The current coap_context_t object.
 * @param ca_file        If not @p NULL, is the full path name of a PEM encoded
 *                       file containing all the Root CAs to be used.
 * @param ca_dir         If not @p NULL, points to a directory containing PEM
 *                       encoded files containing all the Root CAs to be used.
 *
 * @return @c 1 if successful, else @c 0.
 */
int
coap_context_set_pki_root_cas(coap_context_t *context,
                              const char *ca_file,
                              const char *ca_dir);

/**
 * Set the context keepalive timer for sessions.
 * A keepalive message will be sent after if a session has been inactive,
 * i.e. no packet sent or received, for the given number of seconds.
 * For reliable protocols, a PING message will be sent. If a PONG has not
 * been received before the next PING is due to be sent, the session will
 * considered as disconnected.
 *
 * @param context        The coap_context_t object.
 * @param seconds                 Number of seconds for the inactivity timer, or zero
 *                       to disable CoAP-level keepalive messages.
 *
 * @return 1 if successful, else 0
 */
void coap_context_set_keepalive(coap_context_t *context, unsigned int seconds);

/**
 * Returns a new message id and updates @p session->tx_mid accordingly. The
 * message id is returned in network byte order to make it easier to read in
 * tracing tools.
 *
 * @param session The current coap_session_t object.
 *
 * @return        Incremented message id in network byte order.
 */
COAP_STATIC_INLINE uint16_t
coap_new_message_id(coap_session_t *session) {
  return ++session->tx_mid;
}

/**
 * CoAP stack context must be released with coap_free_context(). This function
 * clears all entries from the receive queue and send queue and deletes the
 * resources that have been registered with @p context, and frees the attached
 * endpoints.
 *
 * @param context The current coap_context_t object to free off.
 */
void coap_free_context(coap_context_t *context);

/**
 * Stores @p data with the given CoAP context. This function
 * overwrites any value that has previously been stored with @p
 * context.
 *
 * @param context The CoAP context.
 * @param data The data to store with wih the context. Note that this data
 *             must be valid during the lifetime of @p context.
 */
void coap_set_app_data(coap_context_t *context, void *data);

/**
 * Returns any application-specific data that has been stored with @p
 * context using the function coap_set_app_data(). This function will
 * return @c NULL if no data has been stored.
 *
 * @param context The CoAP context.
 *
 * @return The data previously stored or @c NULL if not data stored.
 */
void *coap_get_app_data(const coap_context_t *context);

/**
 * Creates a new ACK PDU with specified error @p code. The options specified by
 * the filter expression @p opts will be copied from the original request
 * contained in @p request. Unless @c SHORT_ERROR_RESPONSE was defined at build
 * time, the textual reason phrase for @p code will be added as payload, with
 * Content-Type @c 0.
 * This function returns a pointer to the new response message, or @c NULL on
 * error. The storage allocated for the new message must be relased with
 * coap_free().
 *
 * @param request Specification of the received (confirmable) request.
 * @param code    The error code to set.
 * @param opts    An option filter that specifies which options to copy from
 *                the original request in @p node.
 *
 * @return        A pointer to the new message or @c NULL on error.
 */
coap_pdu_t *coap_new_error_response(coap_pdu_t *request,
                                    unsigned char code,
                                    coap_opt_filter_t opts);

/**
 * Sends an error response with code @p code for request @p request to @p dst.
 * @p opts will be passed to coap_new_error_response() to copy marked options
 * from the request. This function returns the transaction id if the message was
 * sent, or @c COAP_INVALID_TID otherwise.
 *
 * @param session         The CoAP session.
 * @param request         The original request to respond to.
 * @param code            The response code.
 * @param opts            A filter that specifies the options to copy from the
 *                        @p request.
 *
 * @return                The transaction id if the message was sent, or @c
 *                        COAP_INVALID_TID otherwise.
 */
coap_tid_t coap_send_error(coap_session_t *session,
                           coap_pdu_t *request,
                           unsigned char code,
                           coap_opt_filter_t opts);

/**
 * Helper funktion to create and send a message with @p type (usually ACK or
 * RST). This function returns @c COAP_INVALID_TID when the message was not
 * sent, a valid transaction id otherwise.
 *
 * @param session         The CoAP session.
 * @param request         The request that should be responded to.
 * @param type            Which type to set.
 * @return                transaction id on success or @c COAP_INVALID_TID
 *                        otherwise.
 */
coap_tid_t
coap_send_message_type(coap_session_t *session, coap_pdu_t *request, unsigned char type);

/**
 * Sends an ACK message with code @c 0 for the specified @p request to @p dst.
 * This function returns the corresponding transaction id if the message was
 * sent or @c COAP_INVALID_TID on error.
 *
 * @param session         The CoAP session.
 * @param request         The request to be acknowledged.
 *
 * @return                The transaction id if ACK was sent or @c
 *                        COAP_INVALID_TID on error.
 */
coap_tid_t coap_send_ack(coap_session_t *session, coap_pdu_t *request);

/**
 * Sends an RST message with code @c 0 for the specified @p request to @p dst.
 * This function returns the corresponding transaction id if the message was
 * sent or @c COAP_INVALID_TID on error.
 *
 * @param session         The CoAP session.
 * @param request         The request to be reset.
 *
 * @return                The transaction id if RST was sent or @c
 *                        COAP_INVALID_TID on error.
 */
COAP_STATIC_INLINE coap_tid_t
coap_send_rst(coap_session_t *session, coap_pdu_t *request) {
  return coap_send_message_type(session, request, COAP_MESSAGE_RST);
}

/**
* Sends a CoAP message to given peer. The memory that is
* allocated by pdu will be released by coap_send().
* The caller must not use the pdu after calling coap_send().
*
* @param session         The CoAP session.
* @param pdu             The CoAP PDU to send.
*
* @return                The message id of the sent message or @c
*                        COAP_INVALID_TID on error.
*/
coap_tid_t coap_send( coap_session_t *session, coap_pdu_t *pdu );

/**
 * Handles retransmissions of confirmable messages
 *
 * @param context      The CoAP context.
 * @param node         The node to retransmit.
 *
 * @return             The message id of the sent message or @c
 *                     COAP_INVALID_TID on error.
 */
coap_tid_t coap_retransmit(coap_context_t *context, coap_queue_t *node);

/**
* For applications with their own message loop, send all pending retransmits and
* return the list of sockets with events to wait for and the next timeout
* The application should call coap_read, then coap_write again when any condition below is true:
* - data is available on any of the sockets with the COAP_SOCKET_WANT_READ
* - an incoming connection is pending in the listen queue and the COAP_SOCKET_WANT_ACCEPT flag is set
* - at least some data can be written without blocking on any of the sockets with the COAP_SOCKET_WANT_WRITE flag set
* - a connection event occured (success or failure) and the COAP_SOCKET_WANT_CONNECT flag is set
* - the timeout has expired
* Before calling coap_read or coap_write again, the application should position COAP_SOCKET_CAN_READ and COAP_SOCKET_CAN_WRITE flags as applicable.
*
* @param ctx The CoAP context
* @param sockets array of socket descriptors, filled on output
* @param max_sockets size of socket array.
* @param num_sockets pointer to the number of valid entries in the socket arrays on output
* @param now Current time.
*
* @return timeout as maxmimum number of milliseconds that the application should wait for network events or 0 if the application should wait forever.
*/

unsigned int
coap_write(coap_context_t *ctx,
  coap_socket_t *sockets[],
  unsigned int max_sockets,
  unsigned int *num_sockets,
  coap_tick_t now
);

/**
 * For applications with their own message loop, reads all data from the network.
 *
 * @param ctx The CoAP context
 * @param now Current time
 */
void coap_read(coap_context_t *ctx, coap_tick_t now);

/**
 * The main message processing loop.
 *
 * @param ctx The CoAP context
 * @param timeout_ms Minimum number of milliseconds to wait for new messages before returning. If zero the call will block until at least one packet is sent or received.
 *
 * @return number of milliseconds spent or @c -1 if there was an error
 */

int coap_run_once( coap_context_t *ctx, unsigned int timeout_ms );

/**
 * Parses and interprets a CoAP datagram with context @p ctx. This function
 * returns @c 0 if the datagram was handled, or a value less than zero on
 * error.
 *
 * @param ctx    The current CoAP context.
 * @param session The current CoAP session.
 * @param data The received packet'd data.
 * @param data_len The received packet'd data length.
 *
 * @return       @c 0 if message was handled successfully, or less than zero on
 *               error.
 */
int coap_handle_dgram(coap_context_t *ctx, coap_session_t *session, uint8_t *data, size_t data_len);

/**
 * Invokes the event handler of @p context for the given @p event and
 * @p data.
 *
 * @param context The CoAP context whose event handler is to be called.
 * @param event   The event to deliver.
 * @param session The session related to @p event.
 * @return The result from the associated event handler or 0 if none was
 * registered.
 */
int coap_handle_event(coap_context_t *context,
                      coap_event_t event,
                      coap_session_t *session);
/**
 * This function removes the element with given @p id from the list given list.
 * If @p id was found, @p node is updated to point to the removed element. Note
 * that the storage allocated by @p node is @b not released. The caller must do
 * this manually using coap_delete_node(). This function returns @c 1 if the
 * element with id @p id was found, @c 0 otherwise. For a return value of @c 0,
 * the contents of @p node is undefined.
 *
 * @param queue The queue to search for @p id.
 * @param session The session to look for.
 * @param id    The transaction id to look for.
 * @param node  If found, @p node is updated to point to the removed node. You
 *              must release the storage pointed to by @p node manually.
 *
 * @return      @c 1 if @p id was found, @c 0 otherwise.
 */
int coap_remove_from_queue(coap_queue_t **queue,
                           coap_session_t *session,
                           coap_tid_t id,
                           coap_queue_t **node);

coap_tid_t
coap_wait_ack( coap_context_t *context, coap_session_t *session,
               coap_queue_t *node);

/**
 * Retrieves transaction from the queue.
 *
 * @param queue The transaction queue to be searched.
 * @param session The session to find.
 * @param id    The transaction id to find.
 *
 * @return      A pointer to the transaction object or @c NULL if not found.
 */
coap_queue_t *coap_find_transaction(coap_queue_t *queue, coap_session_t *session, coap_tid_t id);

/**
 * Cancels all outstanding messages for session @p session that have the specified
 * token.
 *
 * @param context      The context in use.
 * @param session      Session of the messages to remove.
 * @param token        Message token.
 * @param token_length Actual length of @p token.
 */
void coap_cancel_all_messages(coap_context_t *context,
                              coap_session_t *session,
                              const uint8_t *token,
                              size_t token_length);

/**
* Cancels all outstanding messages for session @p session.
*
* @param context      The context in use.
* @param session      Session of the messages to remove.
* @param reason       The reasion for the session cancellation
*/
void
coap_cancel_session_messages(coap_context_t *context,
                             coap_session_t *session,
                             coap_nack_reason_t reason);

/**
 * Dispatches the PDUs from the receive queue in given context.
 */
void coap_dispatch(coap_context_t *context, coap_session_t *session,
                   coap_pdu_t *pdu);

/**
 * Returns 1 if there are no messages to send or to dispatch in the context's
 * queues. */
int coap_can_exit(coap_context_t *context);

/**
 * Returns the current value of an internal tick counter. The counter counts \c
 * COAP_TICKS_PER_SECOND ticks every second.
 */
void coap_ticks(coap_tick_t *);

/**
 * Verifies that @p pdu contains no unknown critical options. Options must be
 * registered at @p ctx, using the function coap_register_option(). A basic set
 * of options is registered automatically by coap_new_context(). This function
 * returns @c 1 if @p pdu is ok, @c 0 otherwise. The given filter object @p
 * unknown will be updated with the unknown options. As only @c COAP_MAX_OPT
 * options can be signalled this way, remaining options must be examined
 * manually.
 *
 * @code
  coap_opt_filter_t f = COAP_OPT_NONE;
  coap_opt_iterator_t opt_iter;

  if (coap_option_check_critical(ctx, pdu, f) == 0) {
    coap_option_iterator_init(pdu, &opt_iter, f);

    while (coap_option_next(&opt_iter)) {
      if (opt_iter.type & 0x01) {
        ... handle unknown critical option in opt_iter ...
      }
    }
  }
   @endcode
 *
 * @param ctx      The context where all known options are registered.
 * @param pdu      The PDU to check.
 * @param unknown  The output filter that will be updated to indicate the
 *                 unknown critical options found in @p pdu.
 *
 * @return         @c 1 if everything was ok, @c 0 otherwise.
 */
int coap_option_check_critical(coap_context_t *ctx,
                               coap_pdu_t *pdu,
                               coap_opt_filter_t unknown);

/**
 * Creates a new response for given @p request with the contents of @c
 * .well-known/core. The result is NULL on error or a newly allocated PDU that
 * must be either sent with coap_sent() or released by coap_delete_pdu().
 *
 * @param context The current coap context to use.
 * @param session The CoAP session.
 * @param request The request for @c .well-known/core .
 *
 * @return        A new 2.05 response for @c .well-known/core or NULL on error.
 */
coap_pdu_t *coap_wellknown_response(coap_context_t *context,
                                    coap_session_t *session,
                                    coap_pdu_t *request);

/**
 * Calculates the initial timeout based on the session CoAP transmission
 * parameters 'ack_timeout', 'ack_random_factor', and COAP_TICKS_PER_SECOND.
 * The calculation requires 'ack_timeout' and 'ack_random_factor' to be in
 * Qx.FRAC_BITS fixed point notation, whereas the passed parameter @p r
 * is interpreted as the fractional part of a Q0.MAX_BITS random value.
 *
 * @param session session timeout is associated with
 * @param r  random value as fractional part of a Q0.MAX_BITS fixed point
 *           value
 * @return   COAP_TICKS_PER_SECOND * 'ack_timeout' *
 *           (1 + ('ack_random_factor' - 1) * r)
 */
unsigned int coap_calc_timeout(coap_session_t *session, unsigned char r);

/**
 * Function interface for joining a multicast group for listening
 *
 * @param ctx   The current context
 * @param groupname The name of the group that is to be joined for listening
 *
 * @return       0 on success, -1 on error
 */
int
coap_join_mcast_group(coap_context_t *ctx, const char *groupname);

#endif /* COAP_NET_H_ */
