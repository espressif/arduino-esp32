/*
 * coap_pdu_internal.h -- CoAP PDU structure
 *
 * Copyright (C) 2010-2021 Olaf Bergmann <bergmann@tzi.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

/**
 * @file coap_pdu_internal.h
 * @brief CoAP PDU internal information
 */

#ifndef COAP_COAP_PDU_INTERNAL_H_
#define COAP_COAP_PDU_INTERNAL_H_

#ifdef WITH_LWIP
#include <lwip/pbuf.h>
#endif

#include <stdint.h>

/**
 * @defgroup pdu_internal PDU (Internal)
 * CoAP PDU Structures, Enums and Functions that are not exposed to
 * applications
 * @{
 */

#define COAP_DEFAULT_VERSION      1 /* version of CoAP supported */

/* TCP Message format constants, do not modify */
#define COAP_MESSAGE_SIZE_OFFSET_TCP8 13
#define COAP_MESSAGE_SIZE_OFFSET_TCP16 269 /* 13 + 256 */
#define COAP_MESSAGE_SIZE_OFFSET_TCP32 65805 /* 269 + 65536 */

/* Derived message size limits */
#define COAP_MAX_MESSAGE_SIZE_TCP0 (COAP_MESSAGE_SIZE_OFFSET_TCP8-1) /* 12 */
#define COAP_MAX_MESSAGE_SIZE_TCP8 (COAP_MESSAGE_SIZE_OFFSET_TCP16-1) /* 268 */
#define COAP_MAX_MESSAGE_SIZE_TCP16 (COAP_MESSAGE_SIZE_OFFSET_TCP32-1) /* 65804 */
#define COAP_MAX_MESSAGE_SIZE_TCP32 (COAP_MESSAGE_SIZE_OFFSET_TCP32+0xFFFFFFFF)

#ifndef COAP_DEBUG_BUF_SIZE
#if defined(WITH_CONTIKI) || defined(WITH_LWIP)
#define COAP_DEBUG_BUF_SIZE 128
#else /* defined(WITH_CONTIKI) || defined(WITH_LWIP) */
/* 1024 derived from RFC7252 4.6.  Message Size max payload */
#define COAP_DEBUG_BUF_SIZE (8 + 1024 * 2)
#endif /* defined(WITH_CONTIKI) || defined(WITH_LWIP) */
#endif /* COAP_DEBUG_BUF_SIZE */

#ifndef COAP_DEFAULT_MAX_PDU_RX_SIZE
#if defined(WITH_CONTIKI) || defined(WITH_LWIP)
#define COAP_DEFAULT_MAX_PDU_RX_SIZE (COAP_MAX_MESSAGE_SIZE_TCP16+4UL)
#else
/* 8 MiB max-message-size plus some space for options */
#define COAP_DEFAULT_MAX_PDU_RX_SIZE (8UL*1024*1024+256)
#endif
#endif /* COAP_DEFAULT_MAX_PDU_RX_SIZE */

/**
 * Indicates that a response is suppressed. This will occur for error
 * responses if the request was received via IP multicast.
 */
#define COAP_DROPPED_RESPONSE -2

#define COAP_PDU_DELAYED -3

#define COAP_PAYLOAD_START 0xFF /* payload marker */

#define COAP_PDU_IS_EMPTY(pdu)     ((pdu)->code == 0)
#define COAP_PDU_IS_REQUEST(pdu)   (!COAP_PDU_IS_EMPTY(pdu) && (pdu)->code < 32)
#define COAP_PDU_IS_RESPONSE(pdu)  ((pdu)->code >= 64 && (pdu)->code < 224)
#define COAP_PDU_IS_SIGNALING(pdu) ((pdu)->code >= 224)

#define COAP_PDU_MAX_UDP_HEADER_SIZE 4
#define COAP_PDU_MAX_TCP_HEADER_SIZE 6

/**
 * structure for CoAP PDUs
 * token, if any, follows the fixed size header, then options until
 * payload marker (0xff), then the payload if stored inline.
 * Memory layout is:
 * <---header--->|<---token---><---options--->0xff<---payload--->
 * header is addressed with a negative offset to token, its maximum size is
 * max_hdr_size.
 * options starts at token + token_length
 * payload starts at data, its length is used_size - (data - token)
 */

struct coap_pdu_t {
  coap_pdu_type_t type;     /**< message type */
  coap_pdu_code_t code;     /**< request method (value 1--31) or response code
                                 (value 64-255) */
  coap_mid_t mid;           /**< message id, if any, in regular host byte
                                 order */
  uint8_t max_hdr_size;     /**< space reserved for protocol-specific header */
  uint8_t hdr_size;         /**< actual size used for protocol-specific
                                 header */
  uint8_t token_length;     /**< length of Token */
  uint16_t max_opt;         /**< highest option number in PDU */
  size_t alloc_size;        /**< allocated storage for token, options and
                                 payload */
  size_t used_size;         /**< used bytes of storage for token, options and
                                 payload */
  size_t max_size;          /**< maximum size for token, options and payload,
                                 or zero for variable size pdu */
  uint8_t *token;           /**< first byte of token, if any, or options */
  uint8_t *data;            /**< first byte of payload, if any */
#ifdef WITH_LWIP
  struct pbuf *pbuf;        /**< lwIP PBUF. The package data will always reside
                             *   inside the pbuf's payload, but this pointer
                             *   has to be kept because no exact offset can be
                             *   given. This field must not be accessed from
                             *   outside, because the pbuf's reference count
                             *   is checked to be 1 when the pbuf is assigned
                             *   to the pdu, and the pbuf stays exclusive to
                             *   this pdu. */
#endif
  const uint8_t *body_data; /**< Holds ptr to re-assembled data or NULL */
  size_t body_length;       /**< Holds body data length */
  size_t body_offset;       /**< Holds body data offset */
  size_t body_total;        /**< Holds body data total size */
  coap_lg_xmit_t *lg_xmit;  /**< Holds ptr to lg_xmit if sending a set of
                                 blocks */
};

/**
 * Dynamically grows the size of @p pdu to @p new_size. The new size
 * must not exceed the PDU's configure maximum size. On success, this
 * function returns 1, otherwise 0.
 *
 * @param pdu      The PDU to resize.
 * @param new_size The new size in bytes.
 * @return         1 if the operation succeeded, 0 otherwise.
 */
int coap_pdu_resize(coap_pdu_t *pdu, size_t new_size);

/**
 * Dynamically grows the size of @p pdu to @p new_size if needed. The new size
 * must not exceed the PDU's configured maximum size. On success, this
 * function returns 1, otherwise 0.
 *
 * @param pdu      The PDU to resize.
 * @param new_size The new size in bytes.
 * @return         1 if the operation succeeded, 0 otherwise.
 */
int coap_pdu_check_resize(coap_pdu_t *pdu, size_t new_size);

/**
* Interprets @p data to determine the number of bytes in the header.
* This function returns @c 0 on error or a number greater than zero on success.
*
* @param proto  Session's protocol
* @param data   The first byte of raw data to parse as CoAP PDU.
*
* @return       A value greater than zero on success or @c 0 on error.
*/
size_t coap_pdu_parse_header_size(coap_proto_t proto,
                                 const uint8_t *data);

/**
 * Parses @p data to extract the message size.
 * @p length must be at least coap_pdu_parse_header_size(proto, data).
 * This function returns @c 0 on error or a number greater than zero on success.
 *
 * @param proto  Session's protocol
 * @param data   The raw data to parse as CoAP PDU.
 * @param length The actual size of @p data.
 *
 * @return       A value greater than zero on success or @c 0 on error.
 */
size_t coap_pdu_parse_size(coap_proto_t proto,
                           const uint8_t *data,
                           size_t length);

/**
 * Decode the protocol specific header for the specified PDU.
 * @param pdu A newly received PDU.
 * @param proto The target wire protocol.
 * @return 1 for success or 0 on error.
 */

int coap_pdu_parse_header(coap_pdu_t *pdu, coap_proto_t proto);

/**
 * Verify consistency in the given CoAP PDU structure and locate the data.
 * This function returns @c 0 on error or a number greater than zero on
 * success.
 * This function only parses the token and options, up to the payload start
 * marker.
 *
 * @param pdu     The PDU structure to check.
 *
 * @return       1 on success or @c 0 on error.
 */
int coap_pdu_parse_opt(coap_pdu_t *pdu);

/**
* Parses @p data into the CoAP PDU structure given in @p result.
* The target pdu must be large enough to
* This function returns @c 0 on error or a number greater than zero on success.
*
* @param proto   Session's protocol
* @param data    The raw data to parse as CoAP PDU.
* @param length  The actual size of @p data.
* @param pdu     The PDU structure to fill. Note that the structure must
*                provide space to hold at least the token and options
*                part of the message.
*
* @return       1 on success or @c 0 on error.
*/
int coap_pdu_parse(coap_proto_t proto,
                   const uint8_t *data,
                   size_t length,
                   coap_pdu_t *pdu);

/**
 * Clears any contents from @p pdu and resets @c used_size,
 * and @c data pointers. @c max_size is set to @p size, any
 * other field is set to @c 0. Note that @p pdu must be a valid
 * pointer to a coap_pdu_t object created e.g. by coap_pdu_init().
 *
 * @param pdu   The PDU to clear.
 * @param size  The maximum size of the PDU.
 */
void coap_pdu_clear(coap_pdu_t *pdu, size_t size);

/**
 * Removes (first) option of given number from the @p pdu.
 *
 * @param pdu   The PDU to remove the option from.
 * @param number The number of the CoAP option to remove (first only removed).
 *
 * @return @c 1 if success else @c 0 if error.
 */
int coap_remove_option(coap_pdu_t *pdu, coap_option_num_t number);

/**
 * Inserts option of given number in the @p pdu with the appropriate data.
 * The option will be inserted in the appropriate place in the options in
 * the pdu.
 *
 * @param pdu    The PDU where the option is to be inserted.
 * @param number The number of the new option.
 * @param len    The length of the new option.
 * @param data   The data of the new option.
 *
 * @return The overall length of the option or @c 0 on failure.
 */
size_t coap_insert_option(coap_pdu_t *pdu, coap_option_num_t number,
                          size_t len, const uint8_t *data);

/**
 * Updates existing first option of given number in the @p pdu with the new
 * data.
 *
 * @param pdu    The PDU where the option is to be updated.
 * @param number The number of the option to update (first only updated).
 * @param len    The length of the updated option.
 * @param data   The data of the updated option.
 *
 * @return The overall length of the updated option or @c 0 on failure.
 */
size_t coap_update_option(coap_pdu_t *pdu,
                          coap_option_num_t number,
                          size_t len,
                          const uint8_t *data);

/**
 * Compose the protocol specific header for the specified PDU.
 *
 * @param pdu A newly composed PDU.
 * @param proto The target wire protocol.
 *
 * @return Number of header bytes prepended before pdu->token or 0 on error.
 */

size_t coap_pdu_encode_header(coap_pdu_t *pdu, coap_proto_t proto);

 /**
 * Updates token in @p pdu with length @p len and @p data.
 * This function returns @c 0 on error or a value greater than zero on success.
 *
 * @param pdu  The PDU where the token is to be updated.
 * @param len  The length of the new token.
 * @param data The token to add.
 *
 * @return     A value greater than zero on success, or @c 0 on error.
 */
int coap_update_token(coap_pdu_t *pdu,
                      size_t len,
                      const uint8_t *data);

/** @} */

#endif /* COAP_COAP_PDU_INTERNAL_H_ */
