/*
 * coap_resource_internal.h -- generic resource handling
 *
 * Copyright (C) 2010,2011,2014-2021 Olaf Bergmann <bergmann@tzi.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

/**
 * @file coap_resource_internal.h
 * @brief Generic resource internal handling
 */

#ifndef COAP_RESOURCE_INTERNAL_H_
#define COAP_RESOURCE_INTERNAL_H_

#include "uthash.h"

/**
 * @defgroup coap_resource_internal Resources (Internal)
 * Structures, Enums and Functions that are not exposed to applications
 * @{
 */

/**
* Abstraction of attribute associated with a resource.
*/
struct coap_attr_t {
  struct coap_attr_t *next; /**< Pointer to next in chain or NULL */
  coap_str_const_t *name;   /**< Name of the attribute */
  coap_str_const_t *value;  /**< Value of the attribute (can be NULL) */
  int flags;
};

/**
* Abstraction of resource that can be attached to coap_context_t.
* The key is uri_path.
*/
struct coap_resource_t {
  unsigned int dirty:1;          /**< set to 1 if resource has changed */
  unsigned int partiallydirty:1; /**< set to 1 if some subscribers have not yet
                                  *   been notified of the last change */
  unsigned int observable:1;     /**< can be observed */
  unsigned int cacheable:1;      /**< can be cached */
  unsigned int is_unknown:1;     /**< resource created for unknown handler */
  unsigned int is_proxy_uri:1;   /**< resource created for proxy URI handler */

  /**
   * Used to store handlers for the seven coap methods @c GET, @c POST, @c PUT,
   * @c DELETE, @c FETCH, @c PATCH and @c IPATCH.
   * coap_dispatch() will pass incoming requests to handle_request() and then
   * to the handler that corresponds to its request method or generate a 4.05
   * response if no handler is available.
   */
  coap_method_handler_t handler[7];

  UT_hash_handle hh;

  coap_attr_t *link_attr; /**< attributes to be included with the link format */
  coap_subscription_t *subscribers;  /**< list of observers for this resource */

  /**
   * Request URI Path for this resource. This field will point into static
   * or allocated memory which must remain there for the duration of the
   * resource.
   */
  coap_str_const_t *uri_path;  /**< the key used for hash lookup for this
                                    resource */
  int flags; /**< zero or more COAP_RESOURCE_FLAGS_* or'd together */

  /**
  * The next value for the Observe option. This field must be increased each
  * time the resource changes. Only the lower 24 bits are sent.
  */
  unsigned int observe;

  /**
   * Pointer back to the context that 'owns' this resource.
   */
  coap_context_t *context;

  /**
   * Count of valid names this host is known by (proxy support)
   */
  size_t proxy_name_count;

  /**
   * Array valid names this host is known by (proxy support)
   */
  coap_str_const_t ** proxy_name_list;

  /**
   * This pointer is under user control. It can be used to store context for
   * the coap handler.
   */
  void *user_data;

};

/**
 * Deletes all resources from given @p context and frees their storage.
 *
 * @param context The CoAP context with the resources to be deleted.
 */
void coap_delete_all_resources(coap_context_t *context);

#define RESOURCES_ADD(r, obj) \
  HASH_ADD(hh, (r), uri_path->s[0], (obj)->uri_path->length, (obj))

#define RESOURCES_DELETE(r, obj) \
  HASH_DELETE(hh, (r), (obj))

#define RESOURCES_ITER(r,tmp)  \
  coap_resource_t *tmp, *rtmp; \
  HASH_ITER(hh, (r), tmp, rtmp)

#define RESOURCES_FIND(r, k, res) {                     \
    HASH_FIND(hh, (r), (k)->s, (k)->length, (res)); \
  }

/**
 * Deletes an attribute.
 * Note: This is for internal use only, as it is not deleted from its chain.
 *
 * @param attr Pointer to a previously created attribute.
 *
 */
void coap_delete_attr(coap_attr_t *attr);

coap_print_status_t coap_print_wellknown(coap_context_t *,
                                         unsigned char *,
                                         size_t *, size_t,
                                         coap_opt_t *);


/** @} */

#endif /* COAP_RESOURCE_INTERNAL_H_ */
