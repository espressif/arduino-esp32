/*
 * block.h -- block transfer
 *
 * Copyright (C) 2010-2012,2014-2015 Olaf Bergmann <bergmann@tzi.org>
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

#ifndef COAP_BLOCK_H_
#define COAP_BLOCK_H_

#include "encode.h"
#include "option.h"
#include "pdu.h"

struct coap_resource_t;
struct coap_session_t;

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
 * Initializes @p block from @p pdu. @p type must be either COAP_OPTION_BLOCK1
 * or COAP_OPTION_BLOCK2. When option @p type was found in @p pdu, @p block is
 * initialized with values from this option and the function returns the value
 * @c 1. Otherwise, @c 0 is returned.
 *
 * @param pdu   The pdu to search for option @p type.
 * @param type  The option to search for (must be COAP_OPTION_BLOCK1 or
 *              COAP_OPTION_BLOCK2).
 * @param block The block structure to initilize.
 *
 * @return      @c 1 on success, @c 0 otherwise.
 */
int coap_get_block(coap_pdu_t *pdu, uint16_t type, coap_block_t *block);

/**
 * Writes a block option of type @p type to message @p pdu. If the requested
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
 * @param type        COAP_OPTION_BLOCK1 or COAP_OPTION_BLOCK2.
 * @param pdu         The message where the block option should be written.
 * @param data_length The length of the actual data that will be added the @p
 *                    pdu by calling coap_add_block().
 *
 * @return            @c 1 on success, or a negative value on error.
 */
int coap_write_block_opt(coap_block_t *block,
                         uint16_t type,
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
                   unsigned int len,
                   const uint8_t *data,
                   unsigned int block_num,
                   unsigned char block_szx);

/**
 * Adds the appropriate part of @p data to the @p response pdu.  If blocks are
 * required, then the appropriate block will be added to the PDU and sent.
 * Adds a ETAG option that is the hash of the entire data if the data is to be
 * split into blocks
 * Used by a GET request handler.
 *
 * @param resource   The resource the data is associated with.
 * @param session    The coap session.
 * @param request    The requesting pdu.
 * @param response   The response pdu.
 * @param token      The token taken from the (original) requesting pdu.
 * @param media_type The format of the data.
 * @param maxage     The maxmimum life of the data. If @c -1, then there
 *                   is no maxage.
 * @param length     The total length of the data.
 * @param data       The entire data block to transmit.
 *
 */
void
coap_add_data_blocked_response(struct coap_resource_t *resource,
                               struct coap_session_t *session,
                               coap_pdu_t *request,
                               coap_pdu_t *response,
                               const coap_binary_t *token,
                               uint16_t media_type,
                               int maxage,
                               size_t length,
                               const uint8_t* data);

/**@}*/

#endif /* COAP_BLOCK_H_ */
