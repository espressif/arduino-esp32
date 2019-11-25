/*
 * net.h -- CoAP network interface
 *
 * Copyright (C) 2010-2015 Olaf Bergmann <bergmann@tzi.org>
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

#ifndef _COAP_NET_H_
#define _COAP_NET_H_

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#ifdef WITH_LWIP
#include <lwip/ip_addr.h>
#endif

#include "coap_io.h"
#include "coap_time.h"
#include "option.h"
#include "pdu.h"
#include "prng.h"

struct coap_queue_t;

typedef struct coap_queue_t {
  struct coap_queue_t *next;
  coap_tick_t t;                /**< when to send PDU for the next time */
  unsigned char retransmit_cnt; /**< retransmission counter, will be removed
                                 *    when zero */
  unsigned int timeout;         /**< the randomized timeout value */
  coap_endpoint_t local_if;     /**< the local address interface */
  coap_address_t remote;        /**< remote address */
  coap_tid_t id;                /**< unique transaction id */
  coap_pdu_t *pdu;              /**< the CoAP PDU to send */
} coap_queue_t;

/** Adds node to given queue, ordered by node->t. */
int coap_insert_node(coap_queue_t **queue, coap_queue_t *node);

/** Destroys specified node. */
int coap_delete_node(coap_queue_t *node);

/** Removes all items from given queue and frees the allocated storage. */
void coap_delete_all(coap_queue_t *queue);

/** Creates a new node suitable for adding to the CoAP sendqueue. */
coap_queue_t *coap_new_node(void);

struct coap_resource_t;
struct coap_context_t;
#ifndef WITHOUT_ASYNC
struct coap_async_state_t;
#endif

/** Message handler that is used as call-back in coap_context_t */
typedef void (*coap_response_handler_t)(struct coap_context_t  *,
                                        const coap_endpoint_t *local_interface,
                                        const coap_address_t *remote,
                                        coap_pdu_t *sent,
                                        coap_pdu_t *received,
                                        const coap_tid_t id);

#define COAP_MID_CACHE_SIZE 3
typedef struct {
  unsigned char flags[COAP_MID_CACHE_SIZE];
  coap_key_t item[COAP_MID_CACHE_SIZE];
} coap_mid_cache_t;

/** The CoAP stack's global state is stored in a coap_context_t object */
typedef struct coap_context_t {
  coap_opt_filter_t known_options;
  struct coap_resource_t *resources; /**< hash table or list of known resources */

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
  coap_endpoint_t *endpoint;      /**< the endpoint used for listening  */

#ifdef WITH_POSIX
  int sockfd;                     /**< send/receive socket */
#endif /* WITH_POSIX */

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

  /**
   * The last message id that was used is stored in this field. The initial
   * value is set by coap_new_context() and is usually a random value. A new
   * message id can be created with coap_new_message_id().
   */
  unsigned short message_id;

  /**
   * The next value to be used for Observe. This field is global for all
   * resources and will be updated when notifications are created.
   */
  unsigned int observe;

  coap_response_handler_t response_handler;

  ssize_t (*network_send)(struct coap_context_t *context,
                          const coap_endpoint_t *local_interface,
                          const coap_address_t *dst,
                          unsigned char *data, size_t datalen);

  ssize_t (*network_read)(coap_endpoint_t *ep, coap_packet_t **packet);

} coap_context_t;

/**
 * Registers a new message handler that is called whenever a response was
 * received that matches an ongoing transaction.
 *
 * @param context The context to register the handler for.
 * @param handler The response handler to register.
 */
static inline void
coap_register_response_handler(coap_context_t *context,
                               coap_response_handler_t handler) {
  context->response_handler = handler;
}

/**
 * Registers the option type @p type with the given context object @p ctx.
 *
 * @param ctx  The context to use.
 * @param type The option type to register.
 */
inline static void
coap_register_option(coap_context_t *ctx, unsigned char type) {
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
 * Returns a new message id and updates @p context->message_id accordingly. The
 * message id is returned in network byte order to make it easier to read in
 * tracing tools.
 *
 * @param context The current coap_context_t object.
 *
 * @return        Incremented message id in network byte order.
 */
static inline unsigned short
coap_new_message_id(coap_context_t *context) {
  context->message_id++;
#ifndef WITH_CONTIKI
  return htons(context->message_id);
#else /* WITH_CONTIKI */
  return uip_htons(context->message_id);
#endif
}

/**
 * CoAP stack context must be released with coap_free_context(). This function
 * clears all entries from the receive queue and send queue and deletes the
 * resources that have been registered with @p context, and frees the attached
 * endpoints.
 */
void coap_free_context(coap_context_t *context);


/**
 * Sends a confirmed CoAP message to given destination. The memory that is
 * allocated by pdu will not be released by coap_send_confirmed(). The caller
 * must release the memory.
 *
 * @param context         The CoAP context to use.
 * @param local_interface The local network interface where the outbound
 *                        packet is sent.
 * @param dst             The address to send to.
 * @param pdu             The CoAP PDU to send.
 *
 * @return                The message id of the sent message or @c
 *                        COAP_INVALID_TID on error.
 */
coap_tid_t coap_send_confirmed(coap_context_t *context,
                               const coap_endpoint_t *local_interface,
                               const coap_address_t *dst,
                               coap_pdu_t *pdu);

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
 * Sends a non-confirmed CoAP message to given destination. The memory that is
 * allocated by pdu will not be released by coap_send().
 * The caller must release the memory.
 *
 * @param context         The CoAP context to use.
 * @param local_interface The local network interface where the outbound packet
 *                        is sent.
 * @param dst             The address to send to.
 * @param pdu             The CoAP PDU to send.
 *
 * @return                The message id of the sent message or @c
 *                        COAP_INVALID_TID on error.
 */
coap_tid_t coap_send(coap_context_t *context,
                     const coap_endpoint_t *local_interface,
                     const coap_address_t *dst,
                     coap_pdu_t *pdu);

/**
 * Sends an error response with code @p code for request @p request to @p dst.
 * @p opts will be passed to coap_new_error_response() to copy marked options
 * from the request. This function returns the transaction id if the message was
 * sent, or @c COAP_INVALID_TID otherwise.
 *
 * @param context         The context to use.
 * @param request         The original request to respond to.
 * @param local_interface The local network interface where the outbound packet
 *                        is sent.
 * @param dst             The remote peer that sent the request.
 * @param code            The response code.
 * @param opts            A filter that specifies the options to copy from the
 *                        @p request.
 *
 * @return                The transaction id if the message was sent, or @c
 *                        COAP_INVALID_TID otherwise.
 */
coap_tid_t coap_send_error(coap_context_t *context,
                           coap_pdu_t *request,
                           const coap_endpoint_t *local_interface,
                           const coap_address_t *dst,
                           unsigned char code,
                           coap_opt_filter_t opts);

/**
 * Helper funktion to create and send a message with @p type (usually ACK or
 * RST). This function returns @c COAP_INVALID_TID when the message was not
 * sent, a valid transaction id otherwise.
 *
 * @param  context        The CoAP context.
 * @param local_interface The local network interface where the outbound packet
 *                        is sent.
 * @param dst             Where to send the context.
 * @param request         The request that should be responded to.
 * @param type            Which type to set.
 * @return                transaction id on success or @c COAP_INVALID_TID
 *                        otherwise.
 */
coap_tid_t
coap_send_message_type(coap_context_t *context,
                       const coap_endpoint_t *local_interface,
                       const coap_address_t *dst,
                       coap_pdu_t *request,
                       unsigned char type);

/**
 * Sends an ACK message with code @c 0 for the specified @p request to @p dst.
 * This function returns the corresponding transaction id if the message was
 * sent or @c COAP_INVALID_TID on error.
 *
 * @param context         The context to use.
 * @param local_interface The local network interface where the outbound packet
 *                        is sent.
 * @param dst             The destination address.
 * @param request         The request to be acknowledged.
 *
 * @return                The transaction id if ACK was sent or @c
 *                        COAP_INVALID_TID on error.
 */
coap_tid_t coap_send_ack(coap_context_t *context,
                         const coap_endpoint_t *local_interface,
                         const coap_address_t *dst,
                         coap_pdu_t *request);

/**
 * Sends an RST message with code @c 0 for the specified @p request to @p dst.
 * This function returns the corresponding transaction id if the message was
 * sent or @c COAP_INVALID_TID on error.
 *
 * @param context         The context to use.
 * @param local_interface The local network interface where the outbound packet
 *                        is sent.
 * @param dst             The destination address.
 * @param request         The request to be reset.
 *
 * @return                The transaction id if RST was sent or @c
 *                        COAP_INVALID_TID on error.
 */
static inline coap_tid_t
coap_send_rst(coap_context_t *context,
              const coap_endpoint_t *local_interface,
              const coap_address_t *dst,
              coap_pdu_t *request) {
  return coap_send_message_type(context,
                                local_interface,
                                dst, request,
                                COAP_MESSAGE_RST);
}

/**
 * Handles retransmissions of confirmable messages
 */
coap_tid_t coap_retransmit(coap_context_t *context, coap_queue_t *node);

/**
 * Reads data from the network and tries to parse as CoAP PDU. On success, 0 is
 * returned and a new node with the parsed PDU is added to the receive queue in
 * the specified context object.
 */
int coap_read(coap_context_t *context);

/**
 * Parses and interprets a CoAP message with context @p ctx. This function
 * returns @c 0 if the message was handled, or a value less than zero on
 * error.
 *
 * @param ctx    The current CoAP context.
 * @param packet The received packet.
 *
 * @return       @c 0 if message was handled successfully, or less than zero on
 *               error.
 */
int coap_handle_message(coap_context_t *ctx,
                        coap_packet_t *packet);

/**
 * Calculates a unique transaction id from given arguments @p peer and @p pdu.
 * The id is returned in @p id.
 *
 * @param peer The remote party who sent @p pdu.
 * @param pdu  The message that initiated the transaction.
 * @param id   Set to the new id.
 */
void coap_transaction_id(const coap_address_t *peer,
                         const coap_pdu_t *pdu,
                         coap_tid_t *id);

/**
 * This function removes the element with given @p id from the list given list.
 * If @p id was found, @p node is updated to point to the removed element. Note
 * that the storage allocated by @p node is @b not released. The caller must do
 * this manually using coap_delete_node(). This function returns @c 1 if the
 * element with id @p id was found, @c 0 otherwise. For a return value of @c 0,
 * the contents of @p node is undefined.
 *
 * @param queue The queue to search for @p id.
 * @param id    The node id to look for.
 * @param node  If found, @p node is updated to point to the removed node. You
 *              must release the storage pointed to by @p node manually.
 *
 * @return      @c 1 if @p id was found, @c 0 otherwise.
 */
int coap_remove_from_queue(coap_queue_t **queue,
                           coap_tid_t id,
                           coap_queue_t **node);

/**
 * Removes the transaction identified by @p id from given @p queue. This is a
 * convenience function for coap_remove_from_queue() with automatic deletion of
 * the removed node.
 *
 * @param queue The queue to search for @p id.
 * @param id    The transaction id.
 *
 * @return      @c 1 if node was found, removed and destroyed, @c 0 otherwise.
 */
inline static int
coap_remove_transaction(coap_queue_t **queue, coap_tid_t id) {
  coap_queue_t *node;
  if (!coap_remove_from_queue(queue, id, &node))
    return 0;

  coap_delete_node(node);
  return 1;
}

/**
 * Retrieves transaction from the queue.
 *
 * @param queue The transaction queue to be searched.
 * @param id    Unique key of the transaction to find.
 *
 * @return      A pointer to the transaction object or NULL if not found.
 */
coap_queue_t *coap_find_transaction(coap_queue_t *queue, coap_tid_t id);

/**
 * Cancels all outstanding messages for peer @p dst that have the specified
 * token.
 *
 * @param context      The context in use.
 * @param dst          Destination address of the messages to remove.
 * @param token        Message token.
 * @param token_length Actual length of @p token.
 */
void coap_cancel_all_messages(coap_context_t *context,
                              const coap_address_t *dst,
                              const unsigned char *token,
                              size_t token_length);

/**
 * Dispatches the PDUs from the receive queue in given context.
 */
void coap_dispatch(coap_context_t *context, coap_queue_t *rcvd);

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
 * @endcode
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
 * must be released by coap_delete_pdu().
 *
 * @param context The current coap context to use.
 * @param request The request for @c .well-known/core .
 *
 * @return        A new 2.05 response for @c .well-known/core or NULL on error.
 */
coap_pdu_t *coap_wellknown_response(coap_context_t *context,
                                    coap_pdu_t *request);

#endif /* _COAP_NET_H_ */
