/*
 * block.h -- block transfer
 *
 * Copyright (C) 2010-2012,2014-2015 Olaf Bergmann <bergmann@tzi.org>
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

#ifndef _COAP_BLOCK_H_
#define _COAP_BLOCK_H_

#include "encode.h"
#include "option.h"
#include "pdu.h"

/**
 * @defgroup block Block Transfer
 * @{
 */

#ifndef COAP_MAX_BLOCK_SZX
/**
 * The largest value for the SZX component in a Block option. Note that
 * 1 << (COAP_MAX_BLOCK_SZX + 4) should not exceed COAP_MAX_PDU_SIZE.
 */
#define COAP_MAX_BLOCK_SZX      4
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
  (COAP_OPT_LENGTH(opt) ? (COAP_OPT_VALUE(opt) + (COAP_OPT_LENGTH(opt)-1)) : 0)

/** Returns the value of the More-bit of a Block option @p opt. */
#define COAP_OPT_BLOCK_MORE(opt) \
  (COAP_OPT_LENGTH(opt) ? (*COAP_OPT_BLOCK_LAST(opt) & 0x08) : 0)

/** Returns the value of the SZX-field of a Block option @p opt. */
#define COAP_OPT_BLOCK_SZX(opt)  \
  (COAP_OPT_LENGTH(opt) ? (*COAP_OPT_BLOCK_LAST(opt) & 0x07) : 0)

/**
 * Returns the value of field @c num in the given block option @p block_opt.
 */
unsigned int coap_opt_block_num(const coap_opt_t *block_opt);

/**
 * Checks if more than @p num blocks are required to deliver @p data_len
 * bytes of data for a block size of 1 << (@p szx + 4).
 */
static inline int
coap_more_blocks(size_t data_len, unsigned int num, unsigned short szx) {
  return ((num+1) << (szx + 4)) < data_len;
}

/** Sets the More-bit in @p block_opt */
static inline void
coap_opt_block_set_m(coap_opt_t *block_opt, int m) {
  if (m)
    *(COAP_OPT_VALUE(block_opt) + (COAP_OPT_LENGTH(block_opt) - 1)) |= 0x08;
  else
    *(COAP_OPT_VALUE(block_opt) + (COAP_OPT_LENGTH(block_opt) - 1)) &= ~0x08;
}

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
int coap_get_block(coap_pdu_t *pdu, unsigned short type, coap_block_t *block);

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
                         unsigned short type,
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
                   const unsigned char *data,
                   unsigned int block_num,
                   unsigned char block_szx);
/**@}*/

#endif /* _COAP_BLOCK_H_ */
