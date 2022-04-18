/*
 * coap_context_internal.h -- Structures, Enums & Functions that are not
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
 * @file coap_net_internal.h
 * @brief COAP net internal information
 */

#ifndef COAP_NET_INTERNAL_H_
#define COAP_NET_INTERNAL_H_

/**
 * @defgroup context_internal Context Handling (Internal)
 * CoAP Context Structures, Enums and Functions that are not exposed to
 * applications
 * @{
 */

/**
 * Queue entry
 */
struct coap_queue_t {
  struct coap_queue_t *next;
  coap_tick_t t;                /**< when to send PDU for the next time */
  unsigned char retransmit_cnt; /**< retransmission counter, will be removed
                                 *    when zero */
  unsigned int timeout;         /**< the randomized timeout value */
  coap_session_t *session;      /**< the CoAP session */
  coap_mid_t id;                /**< CoAP message id */
  coap_pdu_t *pdu;              /**< the CoAP PDU to send */
};

/**
 * The CoAP stack's global state is stored in a coap_context_t object.
 */
struct coap_context_t {
  coap_opt_filter_t known_options;
  coap_resource_t *resources; /**< hash table or list of known
                                   resources */
  coap_resource_t *unknown_resource; /**< can be used for handling
                                          unknown resources */
  coap_resource_t *proxy_uri_resource; /**< can be used for handling
                                            proxy URI resources */
  coap_resource_release_userdata_handler_t release_userdata;
                                        /**< function to  release user_data
                                             when resource is deleted */

#ifndef WITHOUT_ASYNC
  /**
   * list of asynchronous message ids */
  coap_async_t *async_state;
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
  struct etimer retransmit_timer; /**< fires when the next packet must be
                                       sent */
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

  ssize_t (*network_send)(coap_socket_t *sock, const coap_session_t *session,
                          const uint8_t *data, size_t datalen);

  ssize_t (*network_read)(coap_socket_t *sock, coap_packet_t *packet);

  size_t(*get_client_psk)(const coap_session_t *session, const uint8_t *hint,
                          size_t hint_len, uint8_t *identity,
                          size_t *identity_len, size_t max_identity_len,
                          uint8_t *psk, size_t max_psk_len);
  size_t(*get_server_psk)(const coap_session_t *session,
                          const uint8_t *identity, size_t identity_len,
                          uint8_t *psk, size_t max_psk_len);
  size_t(*get_server_hint)(const coap_session_t *session, uint8_t *hint,
                          size_t max_hint_len);

  void *dtls_context;

  coap_dtls_spsk_t spsk_setup_data;  /**< Contains the initial PSK server setup
                                          data */

  unsigned int session_timeout;    /**< Number of seconds of inactivity after
                                        which an unused session will be closed.
                                        0 means use default. */
  unsigned int max_idle_sessions;  /**< Maximum number of simultaneous unused
                                        sessions per endpoint. 0 means no
                                        maximum. */
  unsigned int max_handshake_sessions; /**< Maximum number of simultaneous
                                            negotating sessions per endpoint. 0
                                            means use default. */
  unsigned int ping_timeout;           /**< Minimum inactivity time before
                                            sending a ping message. 0 means
                                            disabled. */
  unsigned int csm_timeout;           /**< Timeout for waiting for a CSM from
                                           the remote side. 0 means disabled. */
  uint8_t observe_pending;         /**< Observe response pending */
  uint8_t block_mode;              /**< Zero or more COAP_BLOCK_ or'd options */
  uint64_t etag;                   /**< Next ETag to use */

  coap_cache_entry_t *cache;       /**< CoAP cache-entry cache */
  uint16_t *cache_ignore_options;  /**< CoAP options to ignore when creating a
                                        cache-key */
  size_t cache_ignore_count;       /**< The number of CoAP options to ignore
                                        when creating a cache-key */
  void *app;                       /**< application-specific data */
#ifdef COAP_EPOLL_SUPPORT
  int epfd;                        /**< External FD for epoll */
  int eptimerfd;                   /**< Internal FD for timeout */
  coap_tick_t next_timeout;        /**< When the next timeout is to occur */
#endif /* COAP_EPOLL_SUPPORT */
};

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
 * Internal function.
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
 * Handles retransmissions of confirmable messages
 *
 * @param context      The CoAP context.
 * @param node         The node to retransmit.
 *
 * @return             The message id of the sent message or @c
 *                     COAP_INVALID_MID on error.
 */
coap_mid_t coap_retransmit(coap_context_t *context, coap_queue_t *node);

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
 * This function removes the element with given @p id from the list given list.
 * If @p id was found, @p node is updated to point to the removed element. Note
 * that the storage allocated by @p node is @b not released. The caller must do
 * this manually using coap_delete_node(). This function returns @c 1 if the
 * element with id @p id was found, @c 0 otherwise. For a return value of @c 0,
 * the contents of @p node is undefined.
 *
 * @param queue The queue to search for @p id.
 * @param session The session to look for.
 * @param id    The message id to look for.
 * @param node  If found, @p node is updated to point to the removed node. You
 *              must release the storage pointed to by @p node manually.
 *
 * @return      @c 1 if @p id was found, @c 0 otherwise.
 */
int coap_remove_from_queue(coap_queue_t **queue,
                           coap_session_t *session,
                           coap_mid_t id,
                           coap_queue_t **node);

coap_mid_t
coap_wait_ack( coap_context_t *context, coap_session_t *session,
               coap_queue_t *node);

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
                               coap_opt_filter_t *unknown);

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
 * Sends a CoAP message to given peer. The memory that is
 * allocated for the pdu will be released by coap_send_internal().
 * The caller must not use the pdu after calling coap_send_internal().
 *
 * If the response body is split into multiple payloads using blocks, libcoap
 * will handle asking for the subsequent blocks and any necessary recovery
 * needed.
 *
 * @param session   The CoAP session.
 * @param pdu       The CoAP PDU to send.
 *
 * @return          The message id of the sent message or @c
 *                  COAP_INVALID_MID on error.
 */
coap_mid_t coap_send_internal(coap_session_t *session, coap_pdu_t *pdu);

/** @} */

#endif /* COAP_NET_INTERNAL_H_ */

