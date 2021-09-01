/*
 * coap_forward_decls.h -- Forward declarations of structures that are
 * opaque to application programming that use libcoap.
 *
 * Copyright (C) 2019-2021 Jon Shallow <supjps-libcoap@jpshallow.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

/**
 * @file coap_forward_decls.h
 * @brief COAP forward definitions
 */

#ifndef COAP_FORWARD_DECLS_H_
#define COAP_FORWARD_DECLS_H_

/*
 * Define the forward declations for the structures (even non-opaque)
 * so that applications (using coap.h) as well as libcoap builds
 * can reference them (and makes .h file dependencies a lot simpler).
 */
struct coap_address_t;
struct coap_bin_const_t;
struct coap_dtls_pki_t;
struct coap_str_const_t;
struct coap_string_t;

/*
 * typedef all the opaque structures that are defined in coap_*_internal.h
 */

/* ************* coap_async_internal.h ***************** */

/**
 * Async Entry information.
 */
typedef struct coap_async_t coap_async_t;

/* ************* coap_block_internal.h ***************** */

/*
 * Block handling information.
 */
typedef struct coap_lg_xmit_t coap_lg_xmit_t;
typedef struct coap_lg_crcv_t coap_lg_crcv_t;
typedef struct coap_lg_srcv_t coap_lg_srcv_t;

/* ************* coap_cache_internal.h ***************** */

/*
 * Cache Entry information.
 */
typedef struct coap_cache_entry_t coap_cache_entry_t;
typedef struct coap_cache_key_t coap_cache_key_t;

/* ************* coap_io_internal.h ***************** */

/**
 * coap_socket_t and coap_packet_t information.
 */
typedef struct coap_packet_t coap_packet_t;
typedef struct coap_socket_t coap_socket_t;

/* ************* coap_net_internal.h ***************** */

/*
 * Net information.
 */
typedef struct coap_context_t coap_context_t;
typedef struct coap_queue_t coap_queue_t;

/* ************* coap_pdu_internal.h ***************** */

/**
 * PDU information.
 */
typedef struct coap_pdu_t coap_pdu_t;

/* ************* coap_resource_internal.h ***************** */

/*
 * Resource information.
 */
typedef struct coap_attr_t coap_attr_t;
typedef struct coap_resource_t coap_resource_t;

/* ************* coap_session_internal.h ***************** */

/*
 * Session information.
 */
typedef struct coap_addr_hash_t coap_addr_hash_t;
typedef struct coap_endpoint_t coap_endpoint_t;
typedef struct coap_session_t coap_session_t;

/* ************* coap_subscribe_internal.h ***************** */

/*
 * Observe subscriber information.
 */
typedef struct coap_subscription_t coap_subscription_t;

#endif /* COAP_FORWARD_DECLS_H_ */
