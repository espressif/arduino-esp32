/*
 * coap_cache_internal.h -- Cache functions for libcoap
 *
 * Copyright (C) 2019--2020 Olaf Bergmann <bergmann@tzi.org> and others
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

/**
 * @file coap_cache_internal.h
 * @brief COAP cache internal information
 */

#ifndef COAP_CACHE_INTERNAL_H_
#define COAP_CACHE_INTERNAL_H_

#include "coap_io.h"

/**
 * @defgroup cache_internal Cache Support (Internal)
 * CoAP Cache Structures, Enums and Functions that are not exposed to
 * applications
 * @{
 */

/* Holds a digest in binary typically sha256 except for notls */
typedef struct coap_digest_t {
  uint8_t key[32];
} coap_digest_t;

struct coap_cache_key_t {
  uint8_t key[32];
};

struct coap_cache_entry_t {
  UT_hash_handle hh;
  coap_cache_key_t *cache_key;
  coap_session_t *session;
  coap_pdu_t *pdu;
  void* app_data;
  coap_tick_t expire_ticks;
  unsigned int idle_timeout;
  coap_cache_app_data_free_callback_t callback;
};

/**
 * Expire coap_cache_entry_t entries
 *
 * Internal function.
 *
 * @param context The context holding the coap-entries to exire
 */
void coap_expire_cache_entries(coap_context_t *context);

typedef void coap_digest_ctx_t;

/**
 * Initialize a coap_digest
 *
 * Internal function.
 *
 * @return          The digest context or @c NULL if failure.
 */
coap_digest_ctx_t *coap_digest_setup(void);

/**
 * Free off coap_digest_ctx_t. Always done by
 * coap_digest_final()
 *
 * Internal function.
 *
 * @param digest_ctx The coap_digest context.
 */
void coap_digest_free(coap_digest_ctx_t *digest_ctx);

/**
 * Update the coap_digest information with the next chunk of data
 *
 * Internal function.
 *
 * @param digest_ctx The coap_digest context.
 * @param data       Pointer to data.
 * @param data_len   Number of bytes.
 *
 * @return           @c 1 success, @c 0 failure.
 */
int coap_digest_update(coap_digest_ctx_t *digest_ctx,
                      const uint8_t *data,
                      size_t data_len
                      );

/**
 * Finalize the coap_digest information  into the provided
 * @p digest_buffer.
 *
 * Internal function.
 *
 * @param digest_ctx    The coap_digest context.
 * @param digest_buffer Pointer to digest buffer to update
 *
 * @return              @c 1 success, @c 0 failure.
 */
int coap_digest_final(coap_digest_ctx_t *digest_ctx,
                      coap_digest_t *digest_buffer);

/** @} */

#endif /* COAP_CACHE_INTERNAL_H_ */
