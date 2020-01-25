/*
 * async.h -- state management for asynchronous messages
 *
 * Copyright (C) 2010-2011 Olaf Bergmann <bergmann@tzi.org>
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

/**
 * @file async.h
 * @brief State management for asynchronous messages
 */

#ifndef COAP_ASYNC_H_
#define COAP_ASYNC_H_

#include "net.h"

#ifndef WITHOUT_ASYNC

/**
 * @defgroup coap_async Asynchronous Messaging
 * @{
 * Structure for managing asynchronous state of CoAP resources. A
 * coap_resource_t object holds a list of coap_async_state_t objects that can be
 * used to generate a separate response in case a result of an operation cannot
 * be delivered in time, or the resource has been explicitly subscribed to with
 * the option @c observe.
 */
typedef struct coap_async_state_t {
  unsigned char flags;  /**< holds the flags to control behaviour */

  /**
   * Holds the internal time when the object was registered with a
   * resource. This field will be updated whenever
   * coap_register_async() is called for a specific resource.
   */
  coap_tick_t created;

  /**
   * This field can be used to register opaque application data with the
   * asynchronous state object.
   */
  void *appdata;
  uint16_t message_id;       /**< id of last message seen */
  coap_session_t *session;         /**< transaction session */
  coap_tid_t id;                   /**< transaction id */
  struct coap_async_state_t *next; /**< internally used for linking */
  size_t tokenlen;                 /**< length of the token */
  uint8_t token[8];                /**< the token to use in a response */
} coap_async_state_t;

/* Definitions for Async Status Flags These flags can be used to control the
 * behaviour of asynchronous response generation.
 */
#define COAP_ASYNC_CONFIRM   0x01  /**< send confirmable response */
#define COAP_ASYNC_SEPARATE  0x02  /**< send separate response */
#define COAP_ASYNC_OBSERVED  0x04  /**< the resource is being observed */

/** release application data on destruction */
#define COAP_ASYNC_RELEASE_DATA  0x08

/**
 * Allocates a new coap_async_state_t object and fills its fields according to
 * the given @p request. The @p flags are used to control generation of empty
 * ACK responses to stop retransmissions and to release registered @p data when
 * the resource is deleted by coap_free_async(). This function returns a pointer
 * to the registered coap_async_t object or @c NULL on error. Note that this
 * function will return @c NULL in case that an object with the same identifier
 * is already registered.
 *
 * @param context  The context to use.
 * @param session  The session that is used for asynchronous transmissions.
 * @param request  The request that is handled asynchronously.
 * @param flags    Flags to control state management.
 * @param data     Opaque application data to register. Note that the
 *                 storage occupied by @p data is released on destruction
 *                 only if flag COAP_ASYNC_RELEASE_DATA is set.
 *
 * @return         A pointer to the registered coap_async_state_t object or @c
 *                 NULL in case of an error.
 */
coap_async_state_t *
coap_register_async(coap_context_t *context,
                    coap_session_t *session,
                    coap_pdu_t *request,
                    unsigned char flags,
                    void *data);

/**
 * Removes the state object identified by @p id from @p context. The removed
 * object is returned in @p s, if found. Otherwise, @p s is undefined. This
 * function returns @c 1 if the object was removed, @c 0 otherwise. Note that
 * the storage allocated for the stored object is not released by this
 * functions. You will have to call coap_free_async() to do so.
 *
 * @param context The context where the async object is registered.
 * @param session  The session that is used for asynchronous transmissions.
 * @param id      The identifier of the asynchronous transaction.
 * @param s       Will be set to the object identified by @p id after removal.
 *
 * @return        @c 1 if object was removed and @p s updated, or @c 0 if no
 *                object was found with the given id. @p s is valid only if the
 *                return value is @c 1.
 */
int coap_remove_async(coap_context_t *context,
                      coap_session_t *session,
                      coap_tid_t id,
                      coap_async_state_t **s);

/**
 * Releases the memory that was allocated by coap_async_state_init() for the
 * object @p s. The registered application data will be released automatically
 * if COAP_ASYNC_RELEASE_DATA is set.
 *
 * @param state The object to delete.
 */
void
coap_free_async(coap_async_state_t *state);

/**
 * Retrieves the object identified by @p id from the list of asynchronous
 * transactions that are registered with @p context. This function returns a
 * pointer to that object or @c NULL if not found.
 *
 * @param context The context where the asynchronous objects are registered
 *                with.
 * @param session  The session that is used for asynchronous transmissions.
 * @param id      The id of the object to retrieve.
 *
 * @return        A pointer to the object identified by @p id or @c NULL if
 *                not found.
 */
coap_async_state_t *coap_find_async(coap_context_t *context, coap_session_t *session, coap_tid_t id);

/**
 * Updates the time stamp of @p s.
 *
 * @param s The state object to update.
 */
COAP_STATIC_INLINE void
coap_touch_async(coap_async_state_t *s) { coap_ticks(&s->created); }

/** @} */

#endif /*  WITHOUT_ASYNC */

#endif /* COAP_ASYNC_H_ */
