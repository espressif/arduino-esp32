/*
 * resource.h -- generic resource handling
 *
 * Copyright (C) 2010,2011,2014,2015 Olaf Bergmann <bergmann@tzi.org>
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

/**
 * @file resource.h
 * @brief Generic resource handling
 */

#ifndef _COAP_RESOURCE_H_
#define _COAP_RESOURCE_H_

# include <assert.h>

#ifndef COAP_RESOURCE_CHECK_TIME
/** The interval in seconds to check if resources have changed. */
#define COAP_RESOURCE_CHECK_TIME 2
#endif /* COAP_RESOURCE_CHECK_TIME */

#ifdef COAP_RESOURCES_NOHASH
#  include "utlist.h"
#else
#  include "uthash.h"
#endif

#include "hashkey.h"
#include "async.h"
#include "str.h"
#include "pdu.h"
#include "net.h"
#include "subscribe.h"

/**
 * Definition of message handler function (@sa coap_resource_t).
 */
typedef void (*coap_method_handler_t)
  (coap_context_t  *,
   struct coap_resource_t *,
   const coap_endpoint_t *,
   coap_address_t *,
   coap_pdu_t *,
   str * /* token */,
   coap_pdu_t * /* response */);

#define COAP_ATTR_FLAGS_RELEASE_NAME  0x1
#define COAP_ATTR_FLAGS_RELEASE_VALUE 0x2

typedef struct coap_attr_t {
  struct coap_attr_t *next;
  str name;
  str value;
  int flags;
} coap_attr_t;

/** The URI passed to coap_resource_init() is free'd by coap_delete_resource(). */
#define COAP_RESOURCE_FLAGS_RELEASE_URI 0x1

/**
 * Notifications will be sent non-confirmable by default. RFC 7641 Section 4.5
 * https://tools.ietf.org/html/rfc7641#section-4.5
 */
#define COAP_RESOURCE_FLAGS_NOTIFY_NON  0x0

/**
 * Notifications will be sent confirmable by default. RFC 7641 Section 4.5
 * https://tools.ietf.org/html/rfc7641#section-4.5
 */
#define COAP_RESOURCE_FLAGS_NOTIFY_CON  0x2

typedef struct coap_resource_t {
  unsigned int dirty:1;          /**< set to 1 if resource has changed */
  unsigned int partiallydirty:1; /**< set to 1 if some subscribers have not yet
                                  *   been notified of the last change */
  unsigned int observable:1;     /**< can be observed */
  unsigned int cacheable:1;      /**< can be cached */

  /**
   * Used to store handlers for the four coap methods @c GET, @c POST, @c PUT,
   * and @c DELETE. coap_dispatch() will pass incoming requests to the handler
   * that corresponds to its request method or generate a 4.05 response if no
   * handler is available.
   */
  coap_method_handler_t handler[4];

  coap_key_t key;                /**< the actual key bytes for this resource */

#ifdef COAP_RESOURCES_NOHASH
  struct coap_resource_t *next;
#else
  UT_hash_handle hh;
#endif

  coap_attr_t *link_attr; /**< attributes to be included with the link format */
  coap_subscription_t *subscribers;  /**< list of observers for this resource */

  /**
   * Request URI for this resource. This field will point into the static
   * memory.
   */
  str uri;
  int flags;

} coap_resource_t;

/**
 * Creates a new resource object and initializes the link field to the string
 * of length @p len. This function returns the new coap_resource_t object.
 *
 * @param uri    The URI path of the new resource.
 * @param len    The length of @p uri.
 * @param flags  Flags for memory management (in particular release of memory).
 *
 * @return       A pointer to the new object or @c NULL on error.
 */
coap_resource_t *coap_resource_init(const unsigned char *uri,
                                    size_t len, int flags);


/**
 * Sets the notification message type of resource @p r to given
 * @p mode which must be one of @c COAP_RESOURCE_FLAGS_NOTIFY_NON
 * or @c COAP_RESOURCE_FLAGS_NOTIFY_CON.
 */
static inline void
coap_resource_set_mode(coap_resource_t *r, int mode) {
  r->flags = (r->flags & !COAP_RESOURCE_FLAGS_NOTIFY_CON) | mode;
}

/**
 * Registers the given @p resource for @p context. The resource must have been
 * created by coap_resource_init(), the storage allocated for the resource will
 * be released by coap_delete_resource().
 *
 * @param context  The context to use.
 * @param resource The resource to store.
 */
void coap_add_resource(coap_context_t *context, coap_resource_t *resource);

/**
 * Deletes a resource identified by @p key. The storage allocated for that
 * resource is freed.
 *
 * @param context  The context where the resources are stored.
 * @param key      The unique key for the resource to delete.
 *
 * @return         @c 1 if the resource was found (and destroyed),
 *                 @c 0 otherwise.
 */
int coap_delete_resource(coap_context_t *context, coap_key_t key);

/**
 * Deletes all resources from given @p context and frees their storage.
 *
 * @param context The CoAP context with the resources to be deleted.
 */
void coap_delete_all_resources(coap_context_t *context);

/**
 * Registers a new attribute with the given @p resource. As the
 * attributes str fields will point to @p name and @p val the
 * caller must ensure that these pointers are valid during the
 * attribute's lifetime.
 *
 * @param resource The resource to register the attribute with.
 * @param name     The attribute's name.
 * @param nlen     Length of @p name.
 * @param val      The attribute's value or @c NULL if none.
 * @param vlen     Length of @p val if specified.
 * @param flags    Flags for memory management (in particular release of
 *                 memory).
 *
 * @return         A pointer to the new attribute or @c NULL on error.
 */
coap_attr_t *coap_add_attr(coap_resource_t *resource,
                           const unsigned char *name,
                           size_t nlen,
                           const unsigned char *val,
                           size_t vlen,
                           int flags);

/**
 * Returns @p resource's coap_attr_t object with given @p name if found, @c NULL
 * otherwise.
 *
 * @param resource The resource to search for attribute @p name.
 * @param name     Name of the requested attribute.
 * @param nlen     Actual length of @p name.
 * @return         The first attribute with specified @p name or @c NULL if none
 *                 was found.
 */
coap_attr_t *coap_find_attr(coap_resource_t *resource,
                            const unsigned char *name,
                            size_t nlen);

/**
 * Deletes an attribute.
 *
 * @param attr Pointer to a previously created attribute.
 *
 */
void coap_delete_attr(coap_attr_t *attr);

/**
 * Status word to encode the result of conditional print or copy operations such
 * as coap_print_link(). The lower 28 bits of coap_print_status_t are used to
 * encode the number of characters that has actually been printed, bits 28 to 31
 * encode the status.  When COAP_PRINT_STATUS_ERROR is set, an error occurred
 * during output. In this case, the other bits are undefined.
 * COAP_PRINT_STATUS_TRUNC indicates that the output is truncated, i.e. the
 * printing would have exceeded the current buffer.
 */
typedef unsigned int coap_print_status_t;

#define COAP_PRINT_STATUS_MASK  0xF0000000u
#define COAP_PRINT_OUTPUT_LENGTH(v) ((v) & ~COAP_PRINT_STATUS_MASK)
#define COAP_PRINT_STATUS_ERROR 0x80000000u
#define COAP_PRINT_STATUS_TRUNC 0x40000000u

/**
 * Writes a description of this resource in link-format to given text buffer. @p
 * len must be initialized to the maximum length of @p buf and will be set to
 * the number of characters actually written if successful. This function
 * returns @c 1 on success or @c 0 on error.
 *
 * @param resource The resource to describe.
 * @param buf      The output buffer to write the description to.
 * @param len      Must be initialized to the length of @p buf and
 *                 will be set to the length of the printed link description.
 * @param offset   The offset within the resource description where to
 *                 start writing into @p buf. This is useful for dealing
 *                 with the Block2 option. @p offset is updated during
 *                 output as it is consumed.
 *
 * @return If COAP_PRINT_STATUS_ERROR is set, an error occured. Otherwise,
 *         the lower 28 bits will indicate the number of characters that
 *         have actually been output into @p buffer. The flag
 *         COAP_PRINT_STATUS_TRUNC indicates that the output has been
 *         truncated.
 */
coap_print_status_t coap_print_link(const coap_resource_t *resource,
                                    unsigned char *buf,
                                    size_t *len,
                                    size_t *offset);

/**
 * Registers the specified @p handler as message handler for the request type @p
 * method
 *
 * @param resource The resource for which the handler shall be registered.
 * @param method   The CoAP request method to handle.
 * @param handler  The handler to register with @p resource.
 */
static inline void
coap_register_handler(coap_resource_t *resource,
                      unsigned char method,
                      coap_method_handler_t handler) {
  assert(resource);
  assert(method > 0 && (size_t)(method-1) < sizeof(resource->handler)/sizeof(coap_method_handler_t));
  resource->handler[method-1] = handler;
}

/**
 * Returns the resource identified by the unique string @p key. If no resource
 * was found, this function returns @c NULL.
 *
 * @param context  The context to look for this resource.
 * @param key      The unique key of the resource.
 *
 * @return         A pointer to the resource or @c NULL if not found.
 */
coap_resource_t *coap_get_resource_from_key(coap_context_t *context,
                                            coap_key_t key);

/**
 * Calculates the hash key for the resource requested by the Uri-Options of @p
 * request. This function calls coap_hash() for every path segment.
 *
 * @param request The requesting pdu.
 * @param key     The resulting hash is stored in @p key.
 */
void coap_hash_request_uri(const coap_pdu_t *request, coap_key_t key);

/**
 * @addtogroup observe
 */

/**
 * Adds the specified peer as observer for @p resource. The subscription is
 * identified by the given @p token. This function returns the registered
 * subscription information if the @p observer has been added, or @c NULL on
 * error.
 *
 * @param resource        The observed resource.
 * @param local_interface The local network interface where the observer is
 *                        attached to.
 * @param observer        The remote peer that wants to received status updates.
 * @param token           The token that identifies this subscription.
 * @return                A pointer to the added/updated subscription
 *                        information or @c NULL on error.
 */
coap_subscription_t *coap_add_observer(coap_resource_t *resource,
                                       const coap_endpoint_t *local_interface,
                                       const coap_address_t *observer,
                                       const str *token);

/**
 * Returns a subscription object for given @p peer.
 *
 * @param resource The observed resource.
 * @param peer     The address to search for.
 * @param token    The token that identifies this subscription or @c NULL for
 *                 any token.
 * @return         A valid subscription if exists or @c NULL otherwise.
 */
coap_subscription_t *coap_find_observer(coap_resource_t *resource,
                                        const coap_address_t *peer,
                                        const str *token);

/**
 * Marks an observer as alive.
 *
 * @param context  The CoAP context to use.
 * @param observer The transport address of the observer.
 * @param token    The corresponding token that has been used for the
 *                 subscription.
 */
void coap_touch_observer(coap_context_t *context,
                         const coap_address_t *observer,
                         const str *token);

/**
 * Removes any subscription for @p observer from @p resource and releases the
 * allocated storage. The result is @c 1 if an observation relationship with @p
 * observer and @p token existed, @c 0 otherwise.
 *
 * @param resource The observed resource.
 * @param observer The observer's address.
 * @param token    The token that identifies this subscription or @c NULL for
 *                 any token.
 * @return         @c 1 if the observer has been deleted, @c 0 otherwise.
 */
int coap_delete_observer(coap_resource_t *resource,
                         const coap_address_t *observer,
                         const str *token);

/**
 * Checks for all known resources, if they are dirty and notifies subscribed
 * observers.
 */
void coap_check_notify(coap_context_t *context);

#ifdef COAP_RESOURCES_NOHASH

#define RESOURCES_ADD(r, obj) \
  LL_PREPEND((r), (obj))

#define RESOURCES_DELETE(r, obj) \
  LL_DELETE((r), (obj))

#define RESOURCES_ITER(r,tmp) \
  coap_resource_t *tmp;       \
  LL_FOREACH((r), tmp)

#define RESOURCES_FIND(r, k, res) {                         \
    coap_resource_t *tmp;                                   \
    (res) = tmp = NULL;                                     \
    LL_FOREACH((r), tmp) {                                  \
      if (memcmp((k), tmp->key, sizeof(coap_key_t)) == 0) { \
        (res) = tmp;                                        \
        break;                                              \
      }                                                     \
    }                                                       \
  }
#else /* COAP_RESOURCES_NOHASH */

#define RESOURCES_ADD(r, obj) \
  HASH_ADD(hh, (r), key, sizeof(coap_key_t), (obj))

#define RESOURCES_DELETE(r, obj) \
  HASH_DELETE(hh, (r), (obj))

#define RESOURCES_ITER(r,tmp)  \
  coap_resource_t *tmp, *rtmp; \
  HASH_ITER(hh, (r), tmp, rtmp)

#define RESOURCES_FIND(r, k, res) {                     \
    HASH_FIND(hh, (r), (k), sizeof(coap_key_t), (res)); \
  }

#endif /* COAP_RESOURCES_NOHASH */

/** @} */

coap_print_status_t coap_print_wellknown(coap_context_t *,
                                         unsigned char *,
                                         size_t *, size_t,
                                         coap_opt_t *);

void coap_handle_failed_notify(coap_context_t *,
                               const coap_address_t *,
                               const str *);

#endif /* _COAP_RESOURCE_H_ */
