/*
 * net.h -- CoAP network interface
 *
 * Copyright (C) 2010-2021 Olaf Bergmann <bergmann@tzi.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

#ifndef COAP_NET_H_
#define COAP_NET_H_

#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <sys/select.h>
#include <sys/time.h>
#endif
#include <time.h>

#ifdef WITH_LWIP
#include <lwip/ip_addr.h>
#endif

#include "coap_io.h"
#include "coap_dtls.h"
#include "coap_event.h"
#include "pdu.h"
#include "coap_session.h"

/**
 * @defgroup context Context Handling
 * API functions for handling PDUs using CoAP Contexts
 * @{
 */

typedef enum coap_response_t {
  COAP_RESPONSE_FAIL, /**< Response not liked - send CoAP RST packet */
  COAP_RESPONSE_OK    /**< Response is fine */
} coap_response_t;

/**
 * Response handler that is used as callback in coap_context_t.
 *
 * @param session CoAP session.
 * @param sent The PDU that was transmitted.
 * @param received The PDU that was received.
 * @param mid CoAP transaction ID.

 * @return @c COAP_RESPONSE_OK if successful, else @c COAP_RESPONSE_FAIL which
 *         triggers sending a RST packet.
 */
typedef coap_response_t (*coap_response_handler_t)(coap_session_t *session,
                                                   const coap_pdu_t *sent,
                                                   const coap_pdu_t *received,
                                                   const coap_mid_t mid);

/**
 * Negative Acknowedge handler that is used as callback in coap_context_t.
 *
 * @param session CoAP session.
 * @param sent The PDU that was transmitted.
 * @param reason The reason for the NACK.
 * @param mid CoAP message ID.
 */
typedef void (*coap_nack_handler_t)(coap_session_t *session,
                                    const coap_pdu_t *sent,
                                    const coap_nack_reason_t reason,
                                    const coap_mid_t mid);

/**
 * Received Ping handler that is used as callback in coap_context_t.
 *
 * @param session CoAP session.
 * @param received The PDU that was received.
 * @param mid CoAP message ID.
 */
typedef void (*coap_ping_handler_t)(coap_session_t *session,
                                    const coap_pdu_t *received,
                                    const coap_mid_t mid);

/**
 * Received Pong handler that is used as callback in coap_context_t.
 *
 * @param session CoAP session.
 * @param received The PDU that was received.
 * @param mid CoAP message ID.
 */
typedef void (*coap_pong_handler_t)(coap_session_t *session,
                                    const coap_pdu_t *received,
                                    const coap_mid_t mid);

/**
 * Registers a new message handler that is called whenever a response is
 * received.
 *
 * @param context The context to register the handler for.
 * @param handler The response handler to register.
 */
void
coap_register_response_handler(coap_context_t *context,
                               coap_response_handler_t handler);

/**
 * Registers a new message handler that is called whenever a confirmable
 * message (request or response) is dropped after all retries have been
 * exhausted, or a rst message was received, or a network or TLS level
 * event was received that indicates delivering the message is not possible.
 *
 * @param context The context to register the handler for.
 * @param handler The nack handler to register.
 */
void
coap_register_nack_handler(coap_context_t *context,
                           coap_nack_handler_t handler);

/**
 * Registers a new message handler that is called whenever a CoAP Ping
 * message is received.
 *
 * @param context The context to register the handler for.
 * @param handler The ping handler to register.
 */
void
coap_register_ping_handler(coap_context_t *context,
                           coap_ping_handler_t handler);

/**
 * Registers a new message handler that is called whenever a CoAP Pong
 * message is received.
 *
 * @param context The context to register the handler for.
 * @param handler The pong handler to register.
 */
void
coap_register_pong_handler(coap_context_t *context,
                           coap_pong_handler_t handler);

/**
 * Registers the option type @p type with the given context object @p ctx.
 *
 * @param ctx  The context to use.
 * @param type The option type to register.
 */
void
coap_register_option(coap_context_t *ctx, uint16_t type);

/**
 * Creates a new coap_context_t object that will hold the CoAP stack status.
 */
coap_context_t *coap_new_context(const coap_address_t *listen_addr);

/**
 * Set the context's default PSK hint and/or key for a server.
 *
 * @param context The current coap_context_t object.
 * @param hint    The default PSK server hint sent to a client. If NULL, PSK
 *                authentication is disabled. Empty string is a valid hint.
 * @param key     The default PSK key. If NULL, PSK authentication will fail.
 * @param key_len The default PSK key's length. If @p 0, PSK authentication will
 *                fail.
 *
 * @return @c 1 if successful, else @c 0.
 */
int coap_context_set_psk( coap_context_t *context, const char *hint,
                           const uint8_t *key, size_t key_len );

/**
 * Set the context's default PSK hint and/or key for a server.
 *
 * @param context    The current coap_context_t object.
 * @param setup_data If NULL, PSK authentication will fail. PSK
 *                   information required.
 *
 * @return @c 1 if successful, else @c 0.
 */
int coap_context_set_psk2(coap_context_t *context,
                          coap_dtls_spsk_t *setup_data);

/**
 * Set the context's default PKI information for a server.
 *
 * @param context        The current coap_context_t object.
 * @param setup_data     If NULL, PKI authentication will fail. Certificate
 *                       information required.
 *
 * @return @c 1 if successful, else @c 0.
 */
int
coap_context_set_pki(coap_context_t *context,
                     const coap_dtls_pki_t *setup_data);

/**
 * Set the context's default Root CA information for a client or server.
 *
 * @param context        The current coap_context_t object.
 * @param ca_file        If not NULL, is the full path name of a PEM encoded
 *                       file containing all the Root CAs to be used.
 * @param ca_dir         If not NULL, points to a directory containing PEM
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
 * For unreliable protocols, a CoAP Empty message will be sent. If a
 * CoAP RST is not received, the CoAP Empty messages will get resent based
 * on the Confirmable retry parameters until there is a failure timeout,
 * at which point the session will be considered as disconnected.
 * For reliable protocols, a CoAP PING message will be sent. If a CoAP PONG
 * has not been received before the next PING is due to be sent, the session
 * will be considered as disconnected.
 *
 * @param context        The coap_context_t object.
 * @param seconds        Number of seconds for the inactivity timer, or zero
 *                       to disable CoAP-level keepalive messages.
 */
void coap_context_set_keepalive(coap_context_t *context, unsigned int seconds);

/**
 * Get the libcoap internal file descriptor for using in an application's
 * select() or returned as an event in an application's epoll_wait() call.
 *
 * @param context        The coap_context_t object.
 *
 * @return The libcoap file descriptor or @c -1 if epoll is not available.
 */
int coap_context_get_coap_fd(const coap_context_t *context);

/**
 * Set the maximum idle sessions count. The number of server sessions that
 * are currently not in use. If this number is exceeded, the least recently
 * used server session is completely removed.
 * 0 (the default) means that the number is not monitored.
 *
 * @param context           The coap_context_t object.
 * @param max_idle_sessions The maximum idle session count.
 */
void
coap_context_set_max_idle_sessions(coap_context_t *context,
                                   unsigned int max_idle_sessions);

/**
 * Get the maximum idle sessions count.
 *
 * @param context The coap_context_t object.
 *
 * @return The count of max idle sessions.
 */
unsigned int
coap_context_get_max_idle_sessions(const coap_context_t *context);

/**
 * Set the session timeout value. The number of seconds of inactivity after
 * which an unused server session will be closed.
 * 0 means use default (300 secs).
 *
 * @param context         The coap_context_t object.
 * @param session_timeout The session timeout value.
 */
void
coap_context_set_session_timeout(coap_context_t *context,
                                   unsigned int session_timeout);

/**
 * Get the session timeout value
 *
 * @param context The coap_context_t object.
 *
 * @return The session timeout value.
 */
unsigned int
coap_context_get_session_timeout(const coap_context_t *context);

/**
 * Set the CSM timeout value. The number of seconds to wait for a (TCP) CSM
 * negotiation response from the peer.
 * 0 (the default) means use wait forever.
 *
 * @param context    The coap_context_t object.
 * @param csm_tmeout The CSM timeout value.
 */
void
coap_context_set_csm_timeout(coap_context_t *context,
                             unsigned int csm_tmeout);

/**
 * Get the CSM timeout value
 *
 * @param context The coap_context_t object.
 *
 * @return The CSM timeout value.
 */
unsigned int
coap_context_get_csm_timeout(const coap_context_t *context);

/**
 * Set the maximum number of sessions in (D)TLS handshake value. If this number
 * is exceeded, the least recently used server session in handshake is
 * completely removed.
 * 0 (the default) means that the number is not monitored.
 *
 * @param context         The coap_context_t object.
 * @param max_handshake_sessions The maximum number of sessions in handshake.
 */
void
coap_context_set_max_handshake_sessions(coap_context_t *context,
                                        unsigned int max_handshake_sessions);

/**
 * Get the session timeout value
 *
 * @param context The coap_context_t object.
 *
 * @return The maximim number of sessions in (D)TLS handshake value.
 */
unsigned int
coap_context_get_max_handshake_sessions(const coap_context_t *context);

/**
 * Returns a new message id and updates @p session->tx_mid accordingly. The
 * message id is returned in network byte order to make it easier to read in
 * tracing tools.
 *
 * @param session The current coap_session_t object.
 *
 * @return        Incremented message id in network byte order.
 */
uint16_t coap_new_message_id(coap_session_t *session);

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
 * error. The storage allocated for the new message must be released with
 * coap_free().
 *
 * @param request Specification of the received (confirmable) request.
 * @param code    The error code to set.
 * @param opts    An option filter that specifies which options to copy from
 *                the original request in @p node.
 *
 * @return        A pointer to the new message or @c NULL on error.
 */
coap_pdu_t *coap_new_error_response(const coap_pdu_t *request,
                                    coap_pdu_code_t code,
                                    coap_opt_filter_t *opts);

/**
 * Sends an error response with code @p code for request @p request to @p dst.
 * @p opts will be passed to coap_new_error_response() to copy marked options
 * from the request. This function returns the message id if the message was
 * sent, or @c COAP_INVALID_MID otherwise.
 *
 * @param session         The CoAP session.
 * @param request         The original request to respond to.
 * @param code            The response code.
 * @param opts            A filter that specifies the options to copy from the
 *                        @p request.
 *
 * @return                The message id if the message was sent, or @c
 *                        COAP_INVALID_MID otherwise.
 */
coap_mid_t coap_send_error(coap_session_t *session,
                           const coap_pdu_t *request,
                           coap_pdu_code_t code,
                           coap_opt_filter_t *opts);

/**
 * Helper function to create and send a message with @p type (usually ACK or
 * RST). This function returns @c COAP_INVALID_MID when the message was not
 * sent, a valid transaction id otherwise.
 *
 * @param session         The CoAP session.
 * @param request         The request that should be responded to.
 * @param type            Which type to set.
 * @return                message id on success or @c COAP_INVALID_MID
 *                        otherwise.
 */
coap_mid_t
coap_send_message_type(coap_session_t *session, const coap_pdu_t *request,
                       coap_pdu_type_t type);

/**
 * Sends an ACK message with code @c 0 for the specified @p request to @p dst.
 * This function returns the corresponding message id if the message was
 * sent or @c COAP_INVALID_MID on error.
 *
 * @param session         The CoAP session.
 * @param request         The request to be acknowledged.
 *
 * @return                The message id if ACK was sent or @c
 *                        COAP_INVALID_MID on error.
 */
coap_mid_t coap_send_ack(coap_session_t *session, const coap_pdu_t *request);

/**
 * Sends an RST message with code @c 0 for the specified @p request to @p dst.
 * This function returns the corresponding message id if the message was
 * sent or @c COAP_INVALID_MID on error.
 *
 * @param session         The CoAP session.
 * @param request         The request to be reset.
 *
 * @return                The message id if RST was sent or @c
 *                        COAP_INVALID_MID on error.
 */
COAP_STATIC_INLINE coap_mid_t
coap_send_rst(coap_session_t *session, const coap_pdu_t *request) {
  return coap_send_message_type(session, request, COAP_MESSAGE_RST);
}

/**
* Sends a CoAP message to given peer. The memory that is
* allocated for the pdu will be released by coap_send().
* The caller must not use the pdu after calling coap_send().
*
* @param session         The CoAP session.
* @param pdu             The CoAP PDU to send.
*
* @return                The message id of the sent message or @c
*                        COAP_INVALID_MID on error.
*/
coap_mid_t coap_send( coap_session_t *session, coap_pdu_t *pdu );

#define coap_send_large(session, pdu) coap_send(session, pdu)

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
 * Returns 1 if there are no messages to send or to dispatch in the context's
 * queues. */
int coap_can_exit(coap_context_t *context);

/**
 * Returns the current value of an internal tick counter. The counter counts \c
 * COAP_TICKS_PER_SECOND ticks every second.
 */
void coap_ticks(coap_tick_t *);

/**
 * Function interface for joining a multicast group for listening for the
 * currently defined endpoints that are UDP.
 *
 * @param ctx       The current context.
 * @param groupname The name of the group that is to be joined for listening.
 * @param ifname    Network interface to join the group on, or NULL if first
 *                  appropriate interface is to be chosen by the O/S.
 *
 * @return       0 on success, -1 on error
 */
int
coap_join_mcast_group_intf(coap_context_t *ctx, const char *groupname,
                           const char *ifname);

#define coap_join_mcast_group(ctx, groupname) \
            (coap_join_mcast_group_intf(ctx, groupname, NULL))

/**
 * Function interface for defining the hop count (ttl) for sending
 * multicast traffic
 *
 * @param session The current contexsion.
 * @param hops    The number of hops (ttl) to use before the multicast
 *                packet expires.
 *
 * @return       1 on success, 0 on error
 */
int
coap_mcast_set_hops(coap_session_t *session, size_t hops);

/**@}*/

/**
 * @defgroup app_io Application I/O Handling
 * API functions for Application Input / Output
 * @{
 */

#define COAP_IO_WAIT    0
#define COAP_IO_NO_WAIT ((uint32_t)-1)

/**
 * The main I/O processing function.  All pending network I/O is completed,
 * and then optionally waits for the next input packet.
 *
 * This internally calls coap_io_prepare_io(), then select() for the appropriate
 * sockets, updates COAP_SOCKET_CAN_xxx where appropriate and then calls
 * coap_io_do_io() before returning with the time spent in the function.
 *
 * Alternatively, if libcoap is compiled with epoll support, this internally
 * calls coap_io_prepare_epoll(), then epoll_wait() for waiting for any file
 * descriptors that have (internally) been set up with epoll_ctl() and
 * finally coap_io_do_epoll() before returning with the time spent in the
 * function.
 *
 * @param ctx The CoAP context
 * @param timeout_ms Minimum number of milliseconds to wait for new packets
 *                   before returning after doing any processing.
 *                   If COAP_IO_WAIT, the call will block until the next
 *                   internal action (e.g. packet retransmit) if any, or block
 *                   until the next packet is received whichever is the sooner
 *                   and do the necessary processing.
 *                   If COAP_IO_NO_WAIT, the function will return immediately
 *                   after processing without waiting for any new input
 *                   packets to arrive.
 *
 * @return Number of milliseconds spent in function or @c -1 if there was
 *         an error
 */
int coap_io_process(coap_context_t *ctx, uint32_t timeout_ms);

#ifndef RIOT_VERSION
/**
 * The main message processing loop with additional fds for internal select.
 *
 * @param ctx The CoAP context
 * @param timeout_ms Minimum number of milliseconds to wait for new packets
 *                   before returning after doing any processing.
 *                   If COAP_IO_WAIT, the call will block until the next
 *                   internal action (e.g. packet retransmit) if any, or block
 *                   until the next packet is received whichever is the sooner
 *                   and do the necessary processing.
 *                   If COAP_IO_NO_WAIT, the function will return immediately
 *                   after processing without waiting for any new input
 *                   packets to arrive.
 * @param nfds      The maximum FD set in readfds, writefds or exceptfds
 *                  plus one,
 * @param readfds   Read FDs to additionally check for in internal select()
 *                  or NULL if not required.
 * @param writefds  Write FDs to additionally check for in internal select()
 *                  or NULL if not required.
 * @param exceptfds Except FDs to additionally check for in internal select()
 *                  or NULL if not required.
 *
 *
 * @return Number of milliseconds spent in coap_io_process_with_fds, or @c -1
 *         if there was an error.  If defined, readfds, writefds, exceptfds
 *         are updated as returned by the internal select() call.
 */
int coap_io_process_with_fds(coap_context_t *ctx, uint32_t timeout_ms,
                             int nfds, fd_set *readfds, fd_set *writefds,
                             fd_set *exceptfds);
#endif /* !RIOT_VERSION */

/**@}*/

/**
 * @defgroup app_io_internal Application I/O Handling (Internal)
 * Internal API functions for Application Input / Output
 * @{
 */

/**
* Iterates through all the coap_socket_t structures embedded in endpoints or
* sessions associated with the @p ctx to determine which are wanting any
* read, write, accept or connect I/O (COAP_SOCKET_WANT_xxx is set). If set,
* the coap_socket_t is added to the @p sockets.
*
* Any now timed out delayed packet is transmitted, along with any packets
* associated with requested observable response.
*
* In addition, it returns when the next expected I/O is expected to take place
* (e.g. a packet retransmit).
*
* Prior to calling coap_io_do_io(), the @p sockets must be tested to see
* if any of the COAP_SOCKET_WANT_xxx have the appropriate information and if
* so, COAP_SOCKET_CAN_xxx is set. This typically will be done after using a
* select() call.
*
* Note: If epoll support is compiled into libcoap, coap_io_prepare_epoll() must
* be used instead of coap_io_prepare_io().
*
* Internal function.
*
* @param ctx The CoAP context
* @param sockets Array of socket descriptors, filled on output
* @param max_sockets Size of socket array.
* @param num_sockets Pointer to the number of valid entries in the socket
*                    arrays on output.
* @param now Current time.
*
* @return timeout Maxmimum number of milliseconds that can be used by a
*                 select() to wait for network events or 0 if wait should be
*                 forever.
*/
unsigned int
coap_io_prepare_io(coap_context_t *ctx,
  coap_socket_t *sockets[],
  unsigned int max_sockets,
  unsigned int *num_sockets,
  coap_tick_t now
);

/**
 * Processes any outstanding read, write, accept or connect I/O as indicated
 * in the coap_socket_t structures (COAP_SOCKET_CAN_xxx set) embedded in
 * endpoints or sessions associated with @p ctx.
 *
 * Note: If epoll support is compiled into libcoap, coap_io_do_epoll() must
 * be used instead of coap_io_do_io().
 *
 * Internal function.
 *
 * @param ctx The CoAP context
 * @param now Current time
 */
void coap_io_do_io(coap_context_t *ctx, coap_tick_t now);

/**
 * Any now timed out delayed packet is transmitted, along with any packets
 * associated with requested observable response.
 *
 * In addition, it returns when the next expected I/O is expected to take place
 * (e.g. a packet retransmit).
 *
 * Note: If epoll support is compiled into libcoap, coap_io_prepare_epoll() must
 * be used instead of coap_io_prepare_io().
 *
 * Internal function.
 *
 * @param ctx The CoAP context
 * @param now Current time.
 *
 * @return timeout Maxmimum number of milliseconds that can be used by a
 *                 epoll_wait() to wait for network events or 0 if wait should be
 *                 forever.
 */
unsigned int
coap_io_prepare_epoll(coap_context_t *ctx, coap_tick_t now);

struct epoll_event;

/**
 * Process all the epoll events
 *
 * Note: If epoll support is compiled into libcoap, coap_io_do_epoll() must
 * be used instead of coap_io_do_io().
 *
 * Internal function
 *
 * @param ctx    The current CoAP context.
 * @param events The list of events returned from an epoll_wait() call.
 * @param nevents The number of events.
 *
 */
void coap_io_do_epoll(coap_context_t *ctx, struct epoll_event* events,
                      size_t nevents);

/**@}*/

/**
 * @deprecated Use coap_io_process() instead.
 *
 * This function just calls coap_io_process().
 *
 * @param ctx The CoAP context
 * @param timeout_ms Minimum number of milliseconds to wait for new packets
 *                   before returning after doing any processing.
 *                   If COAP_IO_WAIT, the call will block until the next
 *                   internal action (e.g. packet retransmit) if any, or block
 *                   until the next packet is received whichever is the sooner
 *                   and do the necessary processing.
 *                   If COAP_IO_NO_WAIT, the function will return immediately
 *                   after processing without waiting for any new input
 *                   packets to arrive.
 *
 * @return Number of milliseconds spent in function or @c -1 if there was
 *         an error
 */
COAP_STATIC_INLINE COAP_DEPRECATED int
coap_run_once(coap_context_t *ctx, uint32_t timeout_ms)
{
  return coap_io_process(ctx, timeout_ms);
}

/**
* @deprecated Use coap_io_prepare_io() instead.
*
* This function just calls coap_io_prepare_io().
*
* Internal function.
*
* @param ctx The CoAP context
* @param sockets Array of socket descriptors, filled on output
* @param max_sockets Size of socket array.
* @param num_sockets Pointer to the number of valid entries in the socket
*                    arrays on output.
* @param now Current time.
*
* @return timeout Maxmimum number of milliseconds that can be used by a
*                 select() to wait for network events or 0 if wait should be
*                 forever.
*/
COAP_STATIC_INLINE COAP_DEPRECATED unsigned int
coap_write(coap_context_t *ctx,
  coap_socket_t *sockets[],
  unsigned int max_sockets,
  unsigned int *num_sockets,
  coap_tick_t now
) {
  return coap_io_prepare_io(ctx, sockets, max_sockets, num_sockets, now);
}

/**
 * @deprecated Use coap_io_do_io() instead.
 *
 * This function just calls coap_io_do_io().
 *
 * Internal function.
 *
 * @param ctx The CoAP context
 * @param now Current time
 */
COAP_STATIC_INLINE COAP_DEPRECATED void
coap_read(coap_context_t *ctx, coap_tick_t now
) {
  coap_io_do_io(ctx, now);
}

/* Old definitions which may be hanging around in old code - be helpful! */
#define COAP_RUN_NONBLOCK COAP_RUN_NONBLOCK_deprecated_use_COAP_IO_NO_WAIT
#define COAP_RUN_BLOCK COAP_RUN_BLOCK_deprecated_use_COAP_IO_WAIT

#endif /* COAP_NET_H_ */
