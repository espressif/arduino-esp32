/*
 * resource.h -- generic resource handling
 *
 * Copyright (C) 2010,2011,2014-2021 Olaf Bergmann <bergmann@tzi.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

/**
 * @file resource.h
 * @brief Generic resource handling
 */

#ifndef COAP_RESOURCE_H_
#define COAP_RESOURCE_H_

#ifndef COAP_RESOURCE_CHECK_TIME
/** The interval in seconds to check if resources have changed. */
#define COAP_RESOURCE_CHECK_TIME 2
#endif /* COAP_RESOURCE_CHECK_TIME */

#include "async.h"
#include "block.h"
#include "str.h"
#include "pdu.h"
#include "net.h"
#include "subscribe.h"

/**
 * @defgroup coap_resource Resource Configuraton
 * API functions for setting up resources
 * @{
 */

/**
 * Definition of message handler function
 */
typedef void (*coap_method_handler_t)
  (coap_resource_t *,
   coap_session_t *,
   const coap_pdu_t * /* request */,
   const coap_string_t * /* query string */,
   coap_pdu_t * /* response */);

#define COAP_ATTR_FLAGS_RELEASE_NAME  0x1
#define COAP_ATTR_FLAGS_RELEASE_VALUE 0x2

/** The URI passed to coap_resource_init() is free'd by coap_delete_resource(). */
#define COAP_RESOURCE_FLAGS_RELEASE_URI 0x1

/**
 * Notifications will be sent non-confirmable by default. RFC 7641 Section 4.5
 * https://tools.ietf.org/html/rfc7641#section-4.5
 * Libcoap will always send every fifth packet as confirmable.
 */
#define COAP_RESOURCE_FLAGS_NOTIFY_NON  0x0

/**
 * Notifications will be sent confirmable. RFC 7641 Section 4.5
 * https://tools.ietf.org/html/rfc7641#section-4.5
 */
#define COAP_RESOURCE_FLAGS_NOTIFY_CON  0x2

/**
 * Notifications will always be sent non-confirmable. This is in
 * violation of RFC 7641 Section 4.5
 * https://tools.ietf.org/html/rfc7641#section-4.5
 * but required by the DOTS signal channel protocol which needs to operate in
 * lossy DDoS attack environments.
 * https://tools.ietf.org/html/rfc8782#section-4.4.2.1
 */
#define COAP_RESOURCE_FLAGS_NOTIFY_NON_ALWAYS  0x4

/**
 * Creates a new resource object and initializes the link field to the string
 * @p uri_path. This function returns the new coap_resource_t object.
 *
 * If the string is going to be freed off by coap_delete_resource() when
 * COAP_RESOURCE_FLAGS_RELEASE_URI is set in @p flags, then either the 's'
 * variable of coap_str_const_t has to point to constant text, or point to data
 * within the allocated coap_str_const_t parameter.
 *
 * @param uri_path The string URI path of the new resource. The leading '/' is
 *                 not normally required - e.g. just "full/path/for/resource".
 * @param flags    Flags for memory management (in particular release of
 *                 memory). Possible values:@n
 *
 *                 COAP_RESOURCE_FLAGS_RELEASE_URI
 *                  If this flag is set, the URI passed to
 *                  coap_resource_init() is free'd by
 *                  coap_delete_resource()@n
 *
 *                 COAP_RESOURCE_FLAGS_NOTIFY_CON
 *                  If this flag is set, coap-observe notifications
 *                  will be sent confirmable by default.@n
 *
 *                 COAP_RESOURCE_FLAGS_NOTIFY_NON (default)
 *                  If this flag is set, coap-observe notifications
 *                  will be sent non-confirmable by default.@n
 *
 *                  If flags is set to 0 then the
 *                  COAP_RESOURCE_FLAGS_NOTIFY_NON is considered.
 *
 * @return         A pointer to the new object or @c NULL on error.
 */
coap_resource_t *coap_resource_init(coap_str_const_t *uri_path,
                                    int flags);


/**
 * Creates a new resource object for the unknown resource handler with support
 * for PUT.
 *
 * In the same way that additional handlers can be added to the resource
 * created by coap_resource_init() by using coap_register_handler(), POST,
 * GET, DELETE etc. handlers can be added to this resource. It is the
 * responsibility of the application to manage the unknown resources by either
 * creating new resources with coap_resource_init() (which should have a
 * DELETE handler specified for the resource removal) or by maintaining an
 * active resource list.
 *
 * Note: There can only be one unknown resource handler per context - attaching
 *       a new one overrides the previous definition.
 *
 * Note: It is not possible to observe the unknown resource with a GET request
 *       - a separate resource needs to be reated by the PUT (or POST) handler,
 *       and make that resource observable.
 *
 * This function returns the new coap_resource_t object.
 *
 * @param put_handler The PUT handler to register with @p resource for
 *                    unknown Uri-Path.
 *
 * @return       A pointer to the new object or @c NULL on error.
 */
coap_resource_t *coap_resource_unknown_init(coap_method_handler_t put_handler);

/**
 * Creates a new resource object for handling proxy URIs.
 * This function returns the new coap_resource_t object.
 *
 * Note: There can only be one proxy resource handler per context - attaching
 *       a new one overrides the previous definition.
 *
 * @param handler The PUT/POST/GET etc. handler that handles all request types.
 * @param host_name_count The number of provided host_name_list entries. A
 *                        minimum of 1 must be provided.
 * @param host_name_list Array of depth host_name_count names that this proxy
 *                       is known by.
 *
 * @return         A pointer to the new object or @c NULL on error.
 */
coap_resource_t *coap_resource_proxy_uri_init(coap_method_handler_t handler,
                      size_t host_name_count, const char *host_name_list[]);

/**
 * Returns the resource identified by the unique string @p uri_path. If no
 * resource was found, this function returns @c NULL.
 *
 * @param context  The context to look for this resource.
 * @param uri_path  The unique string uri of the resource.
 *
 * @return         A pointer to the resource or @c NULL if not found.
 */
coap_resource_t *coap_get_resource_from_uri_path(coap_context_t *context,
                                                coap_str_const_t *uri_path);

/**
 * Get the uri_path from a @p resource.
 *
 * @param resource The CoAP resource to check.
 *
 * @return         The uri_path if it exists or @c NULL otherwise.
 */
coap_str_const_t* coap_resource_get_uri_path(coap_resource_t *resource);

/**
 * Sets the notification message type of resource @p resource to given
 * @p mode

 * @param resource The resource to update.
 * @param mode     Must be one of @c COAP_RESOURCE_FLAGS_NOTIFY_NON
 *                 or @c COAP_RESOURCE_FLAGS_NOTIFY_CON.
 */
void coap_resource_set_mode(coap_resource_t *resource, int mode);

/**
 * Sets the user_data. The user_data is exclusively used by the library-user
 * and can be used as user defined context in the handler functions.
 *
 * @param resource Resource to attach the data to
 * @param data     Data to attach to the user_data field. This pointer is
 *                 only used for storage, the data remains under user control
 */
void coap_resource_set_userdata(coap_resource_t *resource, void *data);

/**
 * Gets the user_data. The user_data is exclusively used by the library-user
 * and can be used as context in the handler functions.
 *
 * @param resource Resource to retrieve the user_data from
 *
 * @return        The user_data pointer
 */
void *coap_resource_get_userdata(coap_resource_t *resource);

/**
 * Definition of release resource user_data callback function
 */
typedef void (*coap_resource_release_userdata_handler_t)(void *user_data);

/**
 * Defines the context wide callback to use to when the resource is deleted
 * to release the data held in the resource's user_data.
 *
 * @param context  The context to associate the release callback with
 * @param callback The callback to invoke when the resource is deleted or NULL
 *
 */
void coap_resource_release_userdata_handler(coap_context_t *context,
                          coap_resource_release_userdata_handler_t callback);

/**
 * Registers the given @p resource for @p context. The resource must have been
 * created by coap_resource_init() or coap_resource_unknown_init(), the
 * storage allocated for the resource will be released by coap_delete_resource().
 *
 * @param context  The context to use.
 * @param resource The resource to store.
 */
void coap_add_resource(coap_context_t *context, coap_resource_t *resource);

/**
 * Deletes a resource identified by @p resource. The storage allocated for that
 * resource is freed, and removed from the context.
 *
 * @param context  The context where the resources are stored.
 * @param resource The resource to delete.
 *
 * @return         @c 1 if the resource was found (and destroyed),
 *                 @c 0 otherwise.
 */
int coap_delete_resource(coap_context_t *context, coap_resource_t *resource);

/**
 * Registers the specified @p handler as message handler for the request type @p
 * method
 *
 * @param resource The resource for which the handler shall be registered.
 * @param method   The CoAP request method to handle.
 * @param handler  The handler to register with @p resource.
 */
void coap_register_handler(coap_resource_t *resource,
                           coap_request_t method,
                           coap_method_handler_t handler);

/**
 * Registers a new attribute with the given @p resource. As the
 * attribute's coap_str_const_ fields will point to @p name and @p value the
 * caller must ensure that these pointers are valid during the
 * attribute's lifetime.

 * If the @p name and/or @p value string is going to be freed off at attribute
 * removal time by the setting of COAP_ATTR_FLAGS_RELEASE_NAME or
 * COAP_ATTR_FLAGS_RELEASE_VALUE in @p flags, then either the 's'
 * variable of coap_str_const_t has to point to constant text, or point to data
 * within the allocated coap_str_const_t parameter.
 *
 * @param resource The resource to register the attribute with.
 * @param name     The attribute's name as a string.
 * @param value    The attribute's value as a string or @c NULL if none.
 * @param flags    Flags for memory management (in particular release of
 *                 memory). Possible values:@n
 *
 *                 COAP_ATTR_FLAGS_RELEASE_NAME
 *                  If this flag is set, the name passed to
 *                  coap_add_attr_release() is free'd
 *                  when the attribute is deleted@n
 *
 *                 COAP_ATTR_FLAGS_RELEASE_VALUE
 *                  If this flag is set, the value passed to
 *                  coap_add_attr_release() is free'd
 *                  when the attribute is deleted@n
 *
 * @return         A pointer to the new attribute or @c NULL on error.
 */
coap_attr_t *coap_add_attr(coap_resource_t *resource,
                           coap_str_const_t *name,
                           coap_str_const_t *value,
                           int flags);

/**
 * Returns @p resource's coap_attr_t object with given @p name if found, @c NULL
 * otherwise.
 *
 * @param resource The resource to search for attribute @p name.
 * @param name     Name of the requested attribute as a string.
 * @return         The first attribute with specified @p name or @c NULL if none
 *                 was found.
 */
coap_attr_t *coap_find_attr(coap_resource_t *resource,
                            coap_str_const_t *name);

/**
 * Returns @p attribute's value.
 *
 * @param attribute Pointer to attribute.
 *
 * @return Attribute's value or @c NULL.
 */
coap_str_const_t *coap_attr_get_value(coap_attr_t *attribute);

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

/** @} */

/**
 * Returns the resource identified by the unique string @p uri_path. If no
 * resource was found, this function returns @c NULL.
 *
 * @param context  The context to look for this resource.
 * @param uri_path  The unique string uri of the resource.
 *
 * @return         A pointer to the resource or @c NULL if not found.
 */
coap_resource_t *coap_get_resource_from_uri_path(coap_context_t *context,
                                                coap_str_const_t *uri_path);

/**
 * @deprecated use coap_resource_notify_observers() instead.
 */
COAP_DEPRECATED int
coap_resource_set_dirty(coap_resource_t *r,
                        const coap_string_t *query);

#endif /* COAP_RESOURCE_H_ */
