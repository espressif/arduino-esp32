/*
 * block.h -- block transfer
 *
 * Copyright (C) 2010-2012,2014-2015 Olaf Bergmann <bergmann@tzi.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

#ifndef COAP_BLOCK_H_
#define COAP_BLOCK_H_

#include "encode.h"
#include "option.h"
#include "pdu.h"

/**
 * @defgroup block Block Transfer
 * API functions for handling PDUs using CoAP BLOCK options
 * @{
 */

#ifndef COAP_MAX_BLOCK_SZX
/**
 * The largest value for the SZX component in a Block option.
 */
#define COAP_MAX_BLOCK_SZX      6
#endif /* COAP_MAX_BLOCK_SZX */

/**
 * Structure of Block options.
 */
typedef struct {
  unsigned int num;       /**< block number */
  unsigned int m:1;       /**< 1 if more blocks follow, 0 otherwise */
  unsigned int szx:3;     /**< block size */
} coap_block_t;

#define COAP_BLOCK_USE_LIBCOAP  0x01 /* Use libcoap to do block requests */
#define COAP_BLOCK_SINGLE_BODY  0x02 /* Deliver the data as a single body */

/**
 * Returns the value of the least significant byte of a Block option @p opt.
 * For zero-length options (i.e. num == m == szx == 0), COAP_OPT_BLOCK_LAST
 * returns @c NULL.
 */
#define COAP_OPT_BLOCK_LAST(opt) \
  (coap_opt_length(opt) ? (coap_opt_value(opt) + (coap_opt_length(opt)-1)) : 0)

/** Returns the value of the More-bit of a Block option @p opt. */
#define COAP_OPT_BLOCK_MORE(opt) \
  (coap_opt_length(opt) ? (*COAP_OPT_BLOCK_LAST(opt) & 0x08) : 0)

/** Returns the value of the SZX-field of a Block option @p opt. */
#define COAP_OPT_BLOCK_SZX(opt)  \
  (coap_opt_length(opt) ? (*COAP_OPT_BLOCK_LAST(opt) & 0x07) : 0)

/**
 * Returns the value of field @c num in the given block option @p block_opt.
 */
unsigned int coap_opt_block_num(const coap_opt_t *block_opt);

/**
 * Checks if more than @p num blocks are required to deliver @p data_len
 * bytes of data for a block size of 1 << (@p szx + 4).
 */
COAP_STATIC_INLINE int
coap_more_blocks(size_t data_len, unsigned int num, uint16_t szx) {
  return ((num+1) << (szx + 4)) < data_len;
}

#if 0
/** Sets the More-bit in @p block_opt */
COAP_STATIC_INLINE void
coap_opt_block_set_m(coap_opt_t *block_opt, int m) {
  if (m)
    *(coap_opt_value(block_opt) + (coap_opt_length(block_opt) - 1)) |= 0x08;
  else
    *(coap_opt_value(block_opt) + (coap_opt_length(block_opt) - 1)) &= ~0x08;
}
#endif

/**
 * Initializes @p block from @p pdu. @p number must be either COAP_OPTION_BLOCK1
 * or COAP_OPTION_BLOCK2. When option @p number was found in @p pdu, @p block is
 * initialized with values from this option and the function returns the value
 * @c 1. Otherwise, @c 0 is returned.
 *
 * @param pdu    The pdu to search for option @p number.
 * @param number The option number to search for (must be COAP_OPTION_BLOCK1 or
 *               COAP_OPTION_BLOCK2).
 * @param block  The block structure to initilize.
 *
 * @return       @c 1 on success, @c 0 otherwise.
 */
int coap_get_block(const coap_pdu_t *pdu, coap_option_num_t number,
                   coap_block_t *block);

/**
 * Writes a block option of type @p number to message @p pdu. If the requested
 * block size is too large to fit in @p pdu, it is reduced accordingly. An
 * exception is made for the final block when less space is required. The actual
 * length of the resource is specified in @p data_length.
 *
 * This function may change *block to reflect the values written to @p pdu. As
 * the function takes into consideration the remaining space @p pdu, no more
 * options should be added after coap_write_block_opt() has returned.
 *
 * @param block       The block structure to use. On return, this object is
 *                    updated according to the values that have been written to
 *                    @p pdu.
 * @param number      COAP_OPTION_BLOCK1 or COAP_OPTION_BLOCK2.
 * @param pdu         The message where the block option should be written.
 * @param data_length The length of the actual data that will be added the @p
 *                    pdu by calling coap_add_block().
 *
 * @return            @c 1 on success, or a negative value on error.
 */
int coap_write_block_opt(coap_block_t *block,
                         coap_option_num_t number,
                         coap_pdu_t *pdu,
                         size_t data_length);

/**
 * Adds the @p block_num block of size 1 << (@p block_szx + 4) from source @p
 * data to @p pdu.
 *
 * @param pdu       The message to add the block.
 * @param len       The length of @p data.
 * @param data      The source data to fill the block with.
 * @param block_num The actual block number.
 * @param block_szx Encoded size of block @p block_number.
 *
 * @return          @c 1 on success, @c 0 otherwise.
 */
int coap_add_block(coap_pdu_t *pdu,
                   size_t len,
                   const uint8_t *data,
                   unsigned int block_num,
                   unsigned char block_szx);

/**
 * Re-assemble payloads into a body
 *
 * @param body_data The pointer to the data for the body holding the
 *                  representation so far or NULL if the first time.
 * @param length    The length of @p data.
 * @param data      The payload data to update the body with.
 * @param offset    The offset of the @p data into the body.
 * @param total     The estimated total size of the body.
 *
 * @return          The current representation of the body or @c NULL if error.
 *                  If NULL, @p body_data will have been de-allocated.
 */
coap_binary_t *
coap_block_build_body(coap_binary_t *body_data, size_t length,
                      const uint8_t *data, size_t offset, size_t total);

/**
 * Adds the appropriate part of @p data to the @p response pdu.  If blocks are
 * required, then the appropriate block will be added to the PDU and sent.
 * Adds a ETAG option that is the hash of the entire data if the data is to be
 * split into blocks
 * Used by a request handler.
 *
 * Note: The application will get called for every packet of a large body to
 * process. Consider using coap_add_data_response_large() instead.
 *
 * @param request    The requesting pdu.
 * @param response   The response pdu.
 * @param media_type The format of the data.
 * @param maxage     The maxmimum life of the data. If @c -1, then there
 *                   is no maxage.
 * @param length     The total length of the data.
 * @param data       The entire data block to transmit.
 *
 */
void
coap_add_data_blocked_response(const coap_pdu_t *request,
                               coap_pdu_t *response,
                               uint16_t media_type,
                               int maxage,
                               size_t length,
                               const uint8_t* data);

/**
 * Callback handler for de-allocating the data based on @p app_ptr provided to
 * coap_add_data_large_*() functions following transmission of the supplied
 * data.
 *
 * @param session The session that this data is associated with
 * @param app_ptr The application provided pointer provided to the
 *                coap_add_data_large_* functions.
 */
typedef void (*coap_release_large_data_t)(coap_session_t *session,
                                          void *app_ptr);

/**
 * Associates given data with the @p pdu that is passed as second parameter.
 *
 * If all the data can be transmitted in a single PDU, this is functionally
 * the same as coap_add_data() except @p release_func (if not NULL) will get
 * invoked after data transmission.
 *
 * Used for a client request.
 *
 * If the data spans multiple PDUs, then the data will get transmitted using
 * BLOCK1 option with the addition of the SIZE1 option.
 * The underlying library will handle the transmission of the individual blocks.
 * Once the body of data has been transmitted (or a failure occurred), then
 * @p release_func (if not NULL) will get called so the application can
 * de-allocate the @p data based on @p app_data. It is the responsibility of
 * the application not to change the contents of @p data until the data
 * transfer has completed.
 *
 * There is no need for the application to include the BLOCK1 option in the
 * @p pdu.
 *
 * coap_add_data_large_request() (or the alternative coap_add_data_large_*()
 * functions) must be called only once per PDU and must be the last PDU update
 * before the PDU is transmitted. The (potentially) initial data will get
 * transmitted when coap_send() is invoked.
 *
 * Note: COAP_BLOCK_USE_LIBCOAP must be set by coap_context_set_block_mode()
 * for libcoap to work correctly when using this function.
 *
 * @param session  The session to associate the data with.
 * @param pdu      The PDU to associate the data with.
 * @param length   The length of data to transmit.
 * @param data     The data to transmit.
 * @param release_func The function to call to de-allocate @p data or @c NULL
 *                 if the function is not required.
 * @param app_ptr  A Pointer that the application can provide for when
 *                 release_func() is called.
 *
 * @return @c 1 if addition is successful, else @c 0.
 */
int coap_add_data_large_request(coap_session_t *session,
                                coap_pdu_t *pdu,
                                size_t length,
                                const uint8_t *data,
                                coap_release_large_data_t release_func,
                                void *app_ptr);

/**
 * Associates given data with the @p response pdu that is passed as fourth
 * parameter.
 *
 * If all the data can be transmitted in a single PDU, this is functionally
 * the same as coap_add_data() except @p release_func (if not NULL) will get
 * invoked after data transmission. The MEDIA_TYPE, MAXAGE and ETAG options may
 * be added in as appropriate.
 *
 * Used by a server request handler to create the response.
 *
 * If the data spans multiple PDUs, then the data will get transmitted using
 * BLOCK2 (response) option with the addition of the SIZE2 and ETAG
 * options. The underlying library will handle the transmission of the
 * individual blocks. Once the body of data has been transmitted (or a
 * failure occurred), then @p release_func (if not NULL) will get called so the
 * application can de-allocate the @p data based on @p app_data. It is the
 * responsibility of the application not to change the contents of @p data
 * until the data transfer has completed.
 *
 * There is no need for the application to include the BLOCK2 option in the
 * @p pdu.
 *
 * coap_add_data_large_response() (or the alternative coap_add_data_large*()
 * functions) must be called only once per PDU and must be the last PDU update
 * before returning from the request handler function.
 *
 * Note: COAP_BLOCK_USE_LIBCOAP must be set by coap_context_set_block_mode()
 * for libcoap to work correctly when using this function.
 *
 * @param resource   The resource the data is associated with.
 * @param session    The coap session.
 * @param request    The requesting pdu.
 * @param response   The response pdu.
 * @param query      The query taken from the (original) requesting pdu.
 * @param media_type The format of the data.
 * @param maxage     The maxmimum life of the data. If @c -1, then there
 *                   is no maxage.
 * @param etag       ETag to use if not 0.
 * @param length     The total length of the data.
 * @param data       The entire data block to transmit.
 * @param release_func The function to call to de-allocate @p data or NULL if
 *                   the function is not required.
 * @param app_ptr    A Pointer that the application can provide for when
 *                   release_func() is called.
 *
 * @return @c 1 if addition is successful, else @c 0.
 */
int
coap_add_data_large_response(coap_resource_t *resource,
                             coap_session_t *session,
                             const coap_pdu_t *request,
                             coap_pdu_t *response,
                             const coap_string_t *query,
                             uint16_t media_type,
                             int maxage,
                             uint64_t etag,
                             size_t length,
                             const uint8_t *data,
                             coap_release_large_data_t release_func,
                             void *app_ptr);

/**
 * Set the context level CoAP block handling bits for handling RFC7959.
 * These bits flow down to a session when a session is created and if the peer
 * does not support something, an appropriate bit may get disabled in the
 * session block_mode.
 * The session block_mode then flows down into coap_crcv_t or coap_srcv_t where
 * again an appropriate bit may get disabled.
 *
 * Note: This function must be called before the session is set up.
 *
 * Note: COAP_BLOCK_USE_LIBCOAP must be set if libcoap is to do all the
 * block tracking and requesting, otherwise the application will have to do
 * all of this work (the default if coap_context_set_block_mode() is not
 * called).
 *
 * @param context        The coap_context_t object.
 * @param block_mode     Zero or more COAP_BLOCK_ or'd options
 */
void coap_context_set_block_mode(coap_context_t *context,
                                  uint8_t block_mode);

/**
 * Cancel an observe that is being tracked by the client large receive logic.
 * (coap_context_set_block_mode() has to be called)
 * This will trigger the sending of an observe cancel pdu to the server.
 *
 * @param session  The session that is being used for the observe.
 * @param token    The original token used to initiate the observation.
 * @param message_type The COAP_MESSAGE_ type (NON or CON) to send the observe
 *                 cancel pdu as.
 *
 * @return @c 1 if observe cancel transmission initiation is successful,
 *         else @c 0.
 */
int coap_cancel_observe(coap_session_t *session, coap_binary_t *token,
                    coap_pdu_type_t message_type);

/**@}*/

#endif /* COAP_BLOCK_H_ */
