/* coap_cache.h -- Caching of CoAP requests
*
* Copyright (C) 2020 Olaf Bergmann <bergmann@tzi.org>
*
 * SPDX-License-Identifier: BSD-2-Clause
 *
* This file is part of the CoAP library libcoap. Please see
* README for terms of use.
*/

/**
 * @file coap_cache.h
 * @brief Provides a simple cache request storage for CoAP requests
 */

#ifndef COAP_CACHE_H_
#define COAP_CACHE_H_

#include "coap_forward_decls.h"

/**
 * @defgroup cache Cache Support
 * API functions for CoAP Caching
 * @{
 */

/**
 * Callback to free off the app data when the cache-entry is
 * being deleted / freed off.
 *
 * @param data  The app data to be freed off.
 */
typedef void (*coap_cache_app_data_free_callback_t)(void *data);

typedef enum coap_cache_session_based_t {
  COAP_CACHE_NOT_SESSION_BASED,
  COAP_CACHE_IS_SESSION_BASED
} coap_cache_session_based_t;

typedef enum coap_cache_record_pdu_t {
  COAP_CACHE_NOT_RECORD_PDU,
  COAP_CACHE_RECORD_PDU
} coap_cache_record_pdu_t;

/**
 * Calculates a cache-key for the given CoAP PDU. See
 * https://tools.ietf.org/html/rfc7252#section-5.6
 * for an explanation of CoAP cache keys.
 *
 * Specific CoAP options can be removed from the cache-key.  Examples of
 * this are the BLOCK1 and BLOCK2 options - which make no real sense including
 * them in a client or server environment, but should be included in a proxy
 * caching environment where things are cached on a per block basis.
 * This is done globally by calling the coap_cache_ignore_options()
 * function.
 *
 * NOTE: The returned cache-key needs to be freed off by the caller by
 * calling coap_cache_delete_key().
 *
 * @param session The session to add into cache-key if @p session_based
 *                is set.
 * @param pdu     The CoAP PDU for which a cache-key is to be
 *                calculated.
 * @param session_based COAP_CACHE_IS_SESSION_BASED if session based
 *                      cache-key, else COAP_CACHE_NOT_SESSION_BASED.
 *
 * @return        The returned cache-key or @c NULL if failure.
 */
coap_cache_key_t *coap_cache_derive_key(const coap_session_t *session,
                                        const coap_pdu_t *pdu,
                                   coap_cache_session_based_t session_based);

/**
 * Calculates a cache-key for the given CoAP PDU. See
 * https://tools.ietf.org/html/rfc7252#section-5.6
 * for an explanation of CoAP cache keys.
 *
 * Specific CoAP options can be removed from the cache-key.  Examples of
 * this are the BLOCK1 and BLOCK2 options - which make no real sense including
 * them in a client or server environment, but should be included in a proxy
 * caching environment where things are cached on a per block basis.
 * This is done individually by specifying @p cache_ignore_count and
 * @p cache_ignore_options .
 *
 * NOTE: The returned cache-key needs to be freed off by the caller by
 * calling coap_cache_delete_key().
 *
 * @param session The session to add into cache-key if @p session_based
 *                is set.
 * @param pdu     The CoAP PDU for which a cache-key is to be
 *                calculated.
 * @param session_based COAP_CACHE_IS_SESSION_BASED if session based
 *                      cache-key, else COAP_CACHE_NOT_SESSION_BASED.
 * @param ignore_options The array of options to ignore.
 * @param ignore_count   The number of options to ignore.
 *
 * @return        The returned cache-key or @c NULL if failure.
 */
coap_cache_key_t *coap_cache_derive_key_w_ignore(const coap_session_t *session,
                                      const coap_pdu_t *pdu,
                                      coap_cache_session_based_t session_based,
                                      const uint16_t *ignore_options,
                                      size_t ignore_count);

/**
 * Delete the cache-key.
 *
 * @param cache_key The cache-key to delete.
 */
void coap_delete_cache_key(coap_cache_key_t *cache_key);

/**
 * Define the CoAP options that are not to be included when calculating
 * the cache-key. Options that are defined as Non-Cache and the Observe
 * option are always ignored.
 *
 * @param context   The context to save the ignored options information in.
 * @param options   The array of options to ignore.
 * @param count     The number of options to ignore.  Use 0 to reset the
 *                  options matching.
 *
 * @return          @return @c 1 if successful, else @c 0.
 */
int coap_cache_ignore_options(coap_context_t *context,
                              const uint16_t *options, size_t count);

/**
 * Create a new cache-entry hash keyed by cache-key derived from the PDU.
 *
 * If @p session_based is set, then this cache-entry will get deleted when
 * the session is freed off.
 * If @p record_pdu is set, then the copied PDU will get freed off when
 * this cache-entry is deleted.
 *
 * The cache-entry is maintained on a context hash list.
 *
 * @param session   The session to use to derive the context from.
 * @param pdu       The pdu to use to generate the cache-key.
 * @param record_pdu COAP_CACHE_RECORD_PDU if to take a copy of the PDU for
 *                   later use, else COAP_CACHE_NOT_RECORD_PDU.
 * @param session_based COAP_CACHE_IS_SESSION_BASED if to associate this
 *                      cache-entry with the the session (which is embedded
 *                      in the cache-entry), else COAP_CACHE_NOT_SESSION_BASED.
 * @param idle_time Idle time in seconds before cache-entry is expired.
 *                  If set to 0, it does not expire (but will get
 *                  deleted if the session is deleted and it is session_based).
 *
 * @return          The returned cache-key or @c NULL if failure.
 */
coap_cache_entry_t *coap_new_cache_entry(coap_session_t *session,
                                 const coap_pdu_t *pdu,
                                 coap_cache_record_pdu_t record_pdu,
                                 coap_cache_session_based_t session_based,
                                 unsigned int idle_time);

/**
 * Remove a cache-entry from the hash list and free off all the appropriate
 * contents apart from app_data.
 *
 * @param context     The context to use.
 * @param cache_entry The cache-entry to remove.
 */
void coap_delete_cache_entry(coap_context_t *context,
                             coap_cache_entry_t *cache_entry);

/**
 * Searches for a cache-entry identified by @p cache_key. This
 * function returns the corresponding cache-entry or @c NULL
 * if not found.
 *
 * @param context    The context to use.
 * @param cache_key  The cache-key to get the hashed coap-entry.
 *
 * @return The cache-entry for @p cache_key or @c NULL if not found.
 */
coap_cache_entry_t *coap_cache_get_by_key(coap_context_t *context,
                                          const coap_cache_key_t *cache_key);

/**
 * Searches for a cache-entry corresponding to @p pdu. This
 * function returns the corresponding cache-entry or @c NULL if not
 * found.
 *
 * @param session    The session to use.
 * @param pdu        The CoAP request to search for.
 * @param session_based COAP_CACHE_IS_SESSION_BASED if session based
 *                     cache-key to be used, else COAP_CACHE_NOT_SESSION_BASED.
 *
 * @return The cache-entry for @p request or @c NULL if not found.
 */
coap_cache_entry_t *coap_cache_get_by_pdu(coap_session_t *session,
                                          const coap_pdu_t *pdu,
                                   coap_cache_session_based_t session_based);

/**
 * Returns the PDU information stored in the @p coap_cache entry.
 *
 * @param cache_entry The CoAP cache entry.
 *
 * @return The PDU information stored in the cache_entry or NULL
 *         if the PDU was not initially copied.
 */
const coap_pdu_t *coap_cache_get_pdu(const coap_cache_entry_t *cache_entry);

/**
 * Stores @p data with the given cache entry. This function
 * overwrites any value that has previously been stored with @p
 * cache_entry.
 *
 * @param cache_entry The CoAP cache entry.
 * @param data The data pointer to store with wih the cache entry. Note that
 *             this data must be valid during the lifetime of @p cache_entry.
 * @param callback The callback to call to free off this data when the
 *                 cache-entry is deleted, or @c NULL if not required.
 */
void coap_cache_set_app_data(coap_cache_entry_t *cache_entry, void *data,
                             coap_cache_app_data_free_callback_t callback);

/**
 * Returns any application-specific data that has been stored with @p
 * cache_entry using the function coap_cache_set_app_data(). This function will
 * return @c NULL if no data has been stored.
 *
 * @param cache_entry The CoAP cache entry.
 *
 * @return The data pointer previously stored or @c NULL if no data stored.
 */
void *coap_cache_get_app_data(const coap_cache_entry_t *cache_entry);

/** @} */

#endif  /* COAP_CACHE_H */
