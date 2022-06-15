/*
 * pdu.h -- CoAP message structure
 *
 * Copyright (C) 2010-2014 Olaf Bergmann <bergmann@tzi.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

/**
 * @file pdu.h
 * @brief Pre-defined constants that reflect defaults for CoAP
 */

#ifndef COAP_PDU_H_
#define COAP_PDU_H_

#include "uri.h"
#include "option.h"

#ifdef WITH_LWIP
#include <lwip/pbuf.h>
#endif

#include <stdint.h>

/**
 * @defgroup pdu PDU
 * API functions for PDUs
 * @{
 */

#define COAP_DEFAULT_PORT      5683 /* CoAP default UDP/TCP port */
#define COAPS_DEFAULT_PORT     5684 /* CoAP default UDP/TCP port for secure transmission */
#define COAP_DEFAULT_MAX_AGE     60 /* default maximum object lifetime in seconds */
#ifndef COAP_DEFAULT_MTU
#define COAP_DEFAULT_MTU       1152
#endif /* COAP_DEFAULT_MTU */

#ifndef COAP_DEFAULT_HOP_LIMIT
#define COAP_DEFAULT_HOP_LIMIT       16
#endif /* COAP_DEFAULT_HOP_LIMIT */

#define COAP_DEFAULT_SCHEME  "coap" /* the default scheme for CoAP URIs */

/** well-known resources URI */
#define COAP_DEFAULT_URI_WELLKNOWN ".well-known/core"

/* CoAP message types */

/**
 * CoAP PDU message type definitions
 */
typedef enum coap_pdu_type_t {
  COAP_MESSAGE_CON,  /* 0 confirmable message (requires ACK/RST) */
  COAP_MESSAGE_NON,  /* 1 non-confirmable message (one-shot message) */
  COAP_MESSAGE_ACK,  /* 2 used to acknowledge confirmable messages */
  COAP_MESSAGE_RST   /* 3 indicates error in received messages */
} coap_pdu_type_t;

/**
 * CoAP PDU Request methods
 */
typedef enum coap_request_t {
  COAP_REQUEST_GET = 1,
  COAP_REQUEST_POST,      /* 2 */
  COAP_REQUEST_PUT,       /* 3 */
  COAP_REQUEST_DELETE,    /* 4 */
  COAP_REQUEST_FETCH,     /* 5 RFC 8132 */
  COAP_REQUEST_PATCH,     /* 6 RFC 8132 */
  COAP_REQUEST_IPATCH,    /* 7 RFC 8132 */
} coap_request_t;

/*
 * CoAP option numbers (be sure to update coap_option_check_critical() and
 * coap_add_option() when adding options
 */

/*
 * The C, U, and N flags indicate the properties
 * Critical, Unsafe, and NoCacheKey, respectively.
 * If U is set, then N has no meaning as per
 * https://tools.ietf.org/html/rfc7252#section-5.10
 * and is set to a -.
 *
 * Separately, R is for the options that can be repeated
 *
 * The least significant byte of the option is set as followed
 * as per https://tools.ietf.org/html/rfc7252#section-5.4.6
 *
 *   0   1   2   3   4   5   6   7
 * --+---+---+---+---+---+---+---+
 *           | NoCacheKey| U | C |
 * --+---+---+---+---+---+---+---+
 *
 * https://tools.ietf.org/html/rfc8613#section-4 goes on to define E, I and U
 * properties Encrypted and Integrity Protected, Integrity Protected Only, and
 * Unprotected respectively.  Integrity Protected Only is not currently used.
 *
 * An Option is tagged with CUNREIU with any of the letters replaced with _ if
 * not set, or - for N if U is set (see above) for aiding understanding of the
 * Option.
 */

#define COAP_OPTION_IF_MATCH        1 /* C__RE__, opaque,    0-8 B, RFC7252 */
#define COAP_OPTION_URI_HOST        3 /* CU-___U, String,  1-255 B, RFC7252 */
#define COAP_OPTION_ETAG            4 /* ___RE__, opaque,    1-8 B, RFC7252 */
#define COAP_OPTION_IF_NONE_MATCH   5 /* C___E__, empty,       0 B, RFC7252 */
#define COAP_OPTION_OBSERVE         6 /* _U-_E_U, empty/uint,0/0-3 B, RFC7641 */
#define COAP_OPTION_URI_PORT        7 /* CU-___U, uint,      0-2 B, RFC7252 */
#define COAP_OPTION_LOCATION_PATH   8 /* ___RE__, String,  0-255 B, RFC7252 */
#define COAP_OPTION_OSCORE          9 /* C_____U, *,       0-255 B, RFC8613 */
#define COAP_OPTION_URI_PATH       11 /* CU-RE__, String,  0-255 B, RFC7252 */
#define COAP_OPTION_CONTENT_FORMAT 12 /* ____E__, uint,      0-2 B, RFC7252 */
#define COAP_OPTION_CONTENT_TYPE COAP_OPTION_CONTENT_FORMAT
/* COAP_OPTION_MAXAGE default 60 seconds if not set */
#define COAP_OPTION_MAXAGE         14 /* _U-_E_U, uint,      0-4 B, RFC7252 */
#define COAP_OPTION_URI_QUERY      15 /* CU-RE__, String,  1-255 B, RFC7252 */
#define COAP_OPTION_HOP_LIMIT      16 /* ______U, uint,        1 B, RFC8768 */
#define COAP_OPTION_ACCEPT         17 /* C___E__, uint,      0-2 B, RFC7252 */
#define COAP_OPTION_LOCATION_QUERY 20 /* ___RE__, String,  0-255 B, RFC7252 */
#define COAP_OPTION_BLOCK2         23 /* CU-_E_U, uint,      0-3 B, RFC7959 */
#define COAP_OPTION_BLOCK1         27 /* CU-_E_U, uint,      0-3 B, RFC7959 */
#define COAP_OPTION_SIZE2          28 /* __N_E_U, uint,      0-4 B, RFC7959 */
#define COAP_OPTION_PROXY_URI      35 /* CU-___U, String, 1-1034 B, RFC7252 */
#define COAP_OPTION_PROXY_SCHEME   39 /* CU-___U, String,  1-255 B, RFC7252 */
#define COAP_OPTION_SIZE1          60 /* __N_E_U, uint,      0-4 B, RFC7252 */
#define COAP_OPTION_NORESPONSE    258 /* _U-_E_U, uint,      0-1 B, RFC7967 */

#define COAP_MAX_OPT            65535 /**< the highest option number we know */

/* CoAP result codes (HTTP-Code / 100 * 40 + HTTP-Code % 100) */

/* As of draft-ietf-core-coap-04, response codes are encoded to base
 * 32, i.e.  the three upper bits determine the response class while
 * the remaining five fine-grained information specific to that class.
 */
#define COAP_RESPONSE_CODE(N) (((N)/100 << 5) | (N)%100)

/* Determines the class of response code C */
#define COAP_RESPONSE_CLASS(C) (((C) >> 5) & 0xFF)

#ifndef SHORT_ERROR_RESPONSE
/**
 * Returns a human-readable response phrase for the specified CoAP response @p
 * code. This function returns @c NULL if not found.
 *
 * @param code The response code for which the literal phrase should be
 *             retrieved.
 *
 * @return     A zero-terminated string describing the error, or @c NULL if not
 *             found.
 */
const char *coap_response_phrase(unsigned char code);

#define COAP_ERROR_PHRASE_LENGTH   32 /**< maximum length of error phrase */

#else
#define coap_response_phrase(x) ((char *)NULL)

#define COAP_ERROR_PHRASE_LENGTH    0 /**< maximum length of error phrase */
#endif /* SHORT_ERROR_RESPONSE */

#define COAP_SIGNALING_CODE(N) (((N)/100 << 5) | (N)%100)

typedef enum coap_pdu_signaling_proto_t {
  COAP_SIGNALING_CSM =     COAP_SIGNALING_CODE(701),
  COAP_SIGNALING_PING =    COAP_SIGNALING_CODE(702),
  COAP_SIGNALING_PONG =    COAP_SIGNALING_CODE(703),
  COAP_SIGNALING_RELEASE = COAP_SIGNALING_CODE(704),
  COAP_SIGNALING_ABORT =   COAP_SIGNALING_CODE(705),
} coap_pdu_signaling_proto_t;

/* Applies to COAP_SIGNALING_CSM */
#define COAP_SIGNALING_OPTION_MAX_MESSAGE_SIZE 2
#define COAP_SIGNALING_OPTION_BLOCK_WISE_TRANSFER 4
/* Applies to COAP_SIGNALING_PING / COAP_SIGNALING_PONG */
#define COAP_SIGNALING_OPTION_CUSTODY 2
/* Applies to COAP_SIGNALING_RELEASE */
#define COAP_SIGNALING_OPTION_ALTERNATIVE_ADDRESS 2
#define COAP_SIGNALING_OPTION_HOLD_OFF 4
/* Applies to COAP_SIGNALING_ABORT */
#define COAP_SIGNALING_OPTION_BAD_CSM_OPTION 2

/* CoAP media type encoding */

#define COAP_MEDIATYPE_TEXT_PLAIN                 0 /* text/plain (UTF-8) */
#define COAP_MEDIATYPE_APPLICATION_LINK_FORMAT   40 /* application/link-format */
#define COAP_MEDIATYPE_APPLICATION_XML           41 /* application/xml */
#define COAP_MEDIATYPE_APPLICATION_OCTET_STREAM  42 /* application/octet-stream */
#define COAP_MEDIATYPE_APPLICATION_RDF_XML       43 /* application/rdf+xml */
#define COAP_MEDIATYPE_APPLICATION_EXI           47 /* application/exi  */
#define COAP_MEDIATYPE_APPLICATION_JSON          50 /* application/json  */
#define COAP_MEDIATYPE_APPLICATION_CBOR          60 /* application/cbor  */
#define COAP_MEDIATYPE_APPLICATION_CWT           61 /* application/cwt, RFC 8392  */

/* Content formats from RFC 8152 */
#define COAP_MEDIATYPE_APPLICATION_COSE_SIGN     98 /* application/cose; cose-type="cose-sign"     */
#define COAP_MEDIATYPE_APPLICATION_COSE_SIGN1    18 /* application/cose; cose-type="cose-sign1"    */
#define COAP_MEDIATYPE_APPLICATION_COSE_ENCRYPT  96 /* application/cose; cose-type="cose-encrypt"  */
#define COAP_MEDIATYPE_APPLICATION_COSE_ENCRYPT0 16 /* application/cose; cose-type="cose-encrypt0" */
#define COAP_MEDIATYPE_APPLICATION_COSE_MAC      97 /* application/cose; cose-type="cose-mac"      */
#define COAP_MEDIATYPE_APPLICATION_COSE_MAC0     17 /* application/cose; cose-type="cose-mac0"     */

#define COAP_MEDIATYPE_APPLICATION_COSE_KEY     101 /* application/cose-key  */
#define COAP_MEDIATYPE_APPLICATION_COSE_KEY_SET 102 /* application/cose-key-set  */

/* Content formats from RFC 8428 */
#define COAP_MEDIATYPE_APPLICATION_SENML_JSON   110 /* application/senml+json  */
#define COAP_MEDIATYPE_APPLICATION_SENSML_JSON  111 /* application/sensml+json */
#define COAP_MEDIATYPE_APPLICATION_SENML_CBOR   112 /* application/senml+cbor  */
#define COAP_MEDIATYPE_APPLICATION_SENSML_CBOR  113 /* application/sensml+cbor */
#define COAP_MEDIATYPE_APPLICATION_SENML_EXI    114 /* application/senml-exi   */
#define COAP_MEDIATYPE_APPLICATION_SENSML_EXI   115 /* application/sensml-exi  */
#define COAP_MEDIATYPE_APPLICATION_SENML_XML    310 /* application/senml+xml   */
#define COAP_MEDIATYPE_APPLICATION_SENSML_XML   311 /* application/sensml+xml  */

/* Content formats from RFC 8782 */
#define COAP_MEDIATYPE_APPLICATION_DOTS_CBOR    271 /* application/dots+cbor */

/* Note that identifiers for registered media types are in the range 0-65535. We
 * use an unallocated type here and hope for the best. */
#define COAP_MEDIATYPE_ANY                         0xff /* any media type */

/**
 * coap_mid_t is used to store the CoAP Message ID of a CoAP PDU.
 * Valid message ids are 0 to 2^16.  Negative values are error codes.
 */
typedef int coap_mid_t;

/** Indicates an invalid message id. */
#define COAP_INVALID_MID -1

/**
 * Indicates an invalid message id.
 * @deprecated Use COAP_INVALID_MID instead.
 */
#define COAP_INVALID_TID COAP_INVALID_MID

/**
 * @deprecated Use coap_optlist_t instead.
 *
 * Structures for more convenient handling of options. (To be used with ordered
 * coap_list_t.) The option's data will be added to the end of the coap_option
 * structure (see macro COAP_OPTION_DATA).
 */
COAP_DEPRECATED typedef struct {
  uint16_t key;           /* the option key (no delta coding) */
  unsigned int length;
} coap_option;

#define COAP_OPTION_KEY(option) (option).key
#define COAP_OPTION_LENGTH(option) (option).length
#define COAP_OPTION_DATA(option) ((unsigned char *)&(option) + sizeof(coap_option))

#ifdef WITH_LWIP
/**
 * Creates a CoAP PDU from an lwIP @p pbuf, whose reference is passed on to this
 * function.
 *
 * The pbuf is checked for being contiguous, and for having only one reference.
 * The reference is stored in the PDU and will be freed when the PDU is freed.
 *
 * (For now, these are fatal errors; in future, a new pbuf might be allocated,
 * the data copied and the passed pbuf freed).
 *
 * This behaves like coap_pdu_init(0, 0, 0, pbuf->tot_len), and afterwards
 * copying the contents of the pbuf to the pdu.
 *
 * @return A pointer to the new PDU object or @c NULL on error.
 */
coap_pdu_t * coap_pdu_from_pbuf(struct pbuf *pbuf);
#endif

/**
* CoAP protocol types
*/
typedef enum coap_proto_t {
  COAP_PROTO_NONE = 0,
  COAP_PROTO_UDP,
  COAP_PROTO_DTLS,
  COAP_PROTO_TCP,
  COAP_PROTO_TLS,
} coap_proto_t;

/**
 * Set of codes available for a PDU.
 */
typedef enum coap_pdu_code_t {
  COAP_EMPTY_CODE          = 0,

  COAP_REQUEST_CODE_GET    = COAP_REQUEST_GET,
  COAP_REQUEST_CODE_POST   = COAP_REQUEST_POST,
  COAP_REQUEST_CODE_PUT    = COAP_REQUEST_PUT,
  COAP_REQUEST_CODE_DELETE = COAP_REQUEST_DELETE,
  COAP_REQUEST_CODE_FETCH  = COAP_REQUEST_FETCH,
  COAP_REQUEST_CODE_PATCH  = COAP_REQUEST_PATCH,
  COAP_REQUEST_CODE_IPATCH = COAP_REQUEST_IPATCH,

  COAP_RESPONSE_CODE_CREATED                    = COAP_RESPONSE_CODE(201),
  COAP_RESPONSE_CODE_DELETED                    = COAP_RESPONSE_CODE(202),
  COAP_RESPONSE_CODE_VALID                      = COAP_RESPONSE_CODE(203),
  COAP_RESPONSE_CODE_CHANGED                    = COAP_RESPONSE_CODE(204),
  COAP_RESPONSE_CODE_CONTENT                    = COAP_RESPONSE_CODE(205),
  COAP_RESPONSE_CODE_CONTINUE                   = COAP_RESPONSE_CODE(231),
  COAP_RESPONSE_CODE_BAD_REQUEST                = COAP_RESPONSE_CODE(400),
  COAP_RESPONSE_CODE_UNAUTHORIZED               = COAP_RESPONSE_CODE(401),
  COAP_RESPONSE_CODE_BAD_OPTION                 = COAP_RESPONSE_CODE(402),
  COAP_RESPONSE_CODE_FORBIDDEN                  = COAP_RESPONSE_CODE(403),
  COAP_RESPONSE_CODE_NOT_FOUND                  = COAP_RESPONSE_CODE(404),
  COAP_RESPONSE_CODE_NOT_ALLOWED                = COAP_RESPONSE_CODE(405),
  COAP_RESPONSE_CODE_NOT_ACCEPTABLE             = COAP_RESPONSE_CODE(406),
  COAP_RESPONSE_CODE_INCOMPLETE                 = COAP_RESPONSE_CODE(408),
  COAP_RESPONSE_CODE_CONFLICT                   = COAP_RESPONSE_CODE(409),
  COAP_RESPONSE_CODE_PRECONDITION_FAILED        = COAP_RESPONSE_CODE(412),
  COAP_RESPONSE_CODE_REQUEST_TOO_LARGE          = COAP_RESPONSE_CODE(413),
  COAP_RESPONSE_CODE_UNSUPPORTED_CONTENT_FORMAT = COAP_RESPONSE_CODE(415),
  COAP_RESPONSE_CODE_UNPROCESSABLE              = COAP_RESPONSE_CODE(422),
  COAP_RESPONSE_CODE_TOO_MANY_REQUESTS          = COAP_RESPONSE_CODE(429),
  COAP_RESPONSE_CODE_INTERNAL_ERROR             = COAP_RESPONSE_CODE(500),
  COAP_RESPONSE_CODE_NOT_IMPLEMENTED            = COAP_RESPONSE_CODE(501),
  COAP_RESPONSE_CODE_BAD_GATEWAY                = COAP_RESPONSE_CODE(502),
  COAP_RESPONSE_CODE_SERVICE_UNAVAILABLE        = COAP_RESPONSE_CODE(503),
  COAP_RESPONSE_CODE_GATEWAY_TIMEOUT            = COAP_RESPONSE_CODE(504),
  COAP_RESPONSE_CODE_PROXYING_NOT_SUPPORTED     = COAP_RESPONSE_CODE(505),
  COAP_RESPONSE_CODE_HOP_LIMIT_REACHED          = COAP_RESPONSE_CODE(508),

  COAP_SIGNALING_CODE_CSM                       = COAP_SIGNALING_CSM,
  COAP_SIGNALING_CODE_PING                      = COAP_SIGNALING_PING,
  COAP_SIGNALING_CODE_PONG                      = COAP_SIGNALING_PONG,
  COAP_SIGNALING_CODE_RELEASE                   = COAP_SIGNALING_RELEASE,
  COAP_SIGNALING_CODE_ABORT                     = COAP_SIGNALING_ABORT
} coap_pdu_code_t;

/**
 * Creates a new CoAP PDU with at least enough storage space for the given
 * @p size maximum message size. The function returns a pointer to the
 * node coap_pdu_t object on success, or @c NULL on error. The storage allocated
 * for the result must be released with coap_delete_pdu() if coap_send()
 * is not called.
 *
 * @param type The type of the PDU (one of COAP_MESSAGE_CON, COAP_MESSAGE_NON,
 *             COAP_MESSAGE_ACK, COAP_MESSAGE_RST).
 * @param code The message code of the PDU.
 * @param mid  The message id to set or 0 if unknown / not applicable.
 * @param size The maximum allowed number of byte for the message.
 * @return     A pointer to the new PDU object or @c NULL on error.
 */
coap_pdu_t *coap_pdu_init(coap_pdu_type_t type, coap_pdu_code_t code,
                          coap_mid_t mid, size_t size);

/**
 * Creates a new CoAP PDU.
 *
 * @param type The type of the PDU (one of COAP_MESSAGE_CON, COAP_MESSAGE_NON,
 *             COAP_MESSAGE_ACK, COAP_MESSAGE_RST).
 * @param code The message code of the PDU.
 * @param session The session that will be using this PDU
 *
 * @return The skeletal PDU or @c NULL if failure.
 */
coap_pdu_t *coap_new_pdu(coap_pdu_type_t type, coap_pdu_code_t code,
                         coap_session_t *session);

/**
 * Dispose of an CoAP PDU and frees associated storage.
 * Not that in general you should not call this function directly.
 * When a PDU is sent with coap_send(), coap_delete_pdu() will be called
 * automatically for you.
 *
 * @param pdu The PDU for free off.
 */
void coap_delete_pdu(coap_pdu_t *pdu);

/**
 * Duplicate an existing PDU. Specific options can be ignored and not copied
 * across.  The PDU data payload is not copied across.
 *
 * @param old_pdu      The PDU to duplicate
 * @param session      The session that will be using this PDU.
 * @param token_length The length of the token to use in this duplicated PDU.
 * @param token        The token to use in this duplicated PDU.
 * @param drop_options A list of options not to copy into the duplicated PDU.
 *                     If @c NULL, then all options are copied across.
 *
 * @return The duplicated PDU or @c NULL if failure.
 */
coap_pdu_t *
coap_pdu_duplicate(const coap_pdu_t *old_pdu,
                   coap_session_t *session,
                   size_t token_length,
                   const uint8_t *token,
                   coap_opt_filter_t *drop_options);

/**
 * Adds token of length @p len to @p pdu.
 * Adding the token destroys any following contents of the pdu. Hence options
 * and data must be added after coap_add_token() has been called. In @p pdu,
 * length is set to @p len + @c 4, and max_delta is set to @c 0. This function
 * returns @c 0 on error or a value greater than zero on success.
 *
 * @param pdu  The PDU where the token is to be added.
 * @param len  The length of the new token.
 * @param data The token to add.
 *
 * @return     A value greater than zero on success, or @c 0 on error.
 */
int coap_add_token(coap_pdu_t *pdu,
                  size_t len,
                  const uint8_t *data);

/**
 * Adds option of given number to pdu that is passed as first
 * parameter.
 * coap_add_option() destroys the PDU's data, so coap_add_data() must be called
 * after all options have been added. As coap_add_token() destroys the options
 * following the token, the token must be added before coap_add_option() is
 * called. This function returns the number of bytes written or @c 0 on error.
 *
 * Note: Where possible, the option data needs to be stripped of leading zeros
 * (big endian) to reduce the amount of data needed in the PDU, as well as in
 * some cases the maximum data size of an opton can be exceeded if not stripped
 * and hence be illegal.  This is done by using coap_encode_var_safe() or
 * coap_encode_var_safe8().
 *
 * @param pdu    The PDU where the option is to be added.
 * @param number The number of the new option.
 * @param len    The length of the new option.
 * @param data   The data of the new option.
 *
 * @return The overall length of the option or @c 0 on failure.
 */
size_t coap_add_option(coap_pdu_t *pdu,
                       coap_option_num_t number,
                       size_t len,
                       const uint8_t *data);

/**
 * Adds given data to the pdu that is passed as first parameter. Note that the
 * PDU's data is destroyed by coap_add_option(). coap_add_data() must be called
 * only once per PDU, otherwise the result is undefined.
 *
 * @param pdu    The PDU where the data is to be added.
 * @param len    The length of the data.
 * @param data   The data to add.
 *
 * @return @c 1 if success, else @c 0 if failure.
 */
int coap_add_data(coap_pdu_t *pdu,
                  size_t len,
                  const uint8_t *data);

/**
 * Adds given data to the pdu that is passed as first parameter but does not
 * copy it. Note that the PDU's data is destroyed by coap_add_option().
 * coap_add_data() must be have been called once for this PDU, otherwise the
 * result is undefined.
 * The actual data must be copied at the returned location.
 *
 * @param pdu    The PDU where the data is to be added.
 * @param len    The length of the data.
 *
 * @return Where to copy the data of len to, or @c NULL is error.
 */
uint8_t *coap_add_data_after(coap_pdu_t *pdu, size_t len);

/**
 * Retrieves the length and data pointer of specified PDU. Returns 0 on error or
 * 1 if *len and *data have correct values. Note that these values are destroyed
 * with the pdu.
 *
 * @param pdu    The specified PDU.
 * @param len    Returns the length of the current data
 * @param data   Returns the ptr to the current data
 *
 * @return @c 1 if len and data are correctly filled in, else
 *         @c 0 if there is no data.
 */
int coap_get_data(const coap_pdu_t *pdu,
                  size_t *len,
                  const uint8_t **data);

/**
 * Retrieves the data from a PDU, with support for large bodies of data that
 * spans multiple PDUs.
 *
 * Note: The data pointed to on return is destroyed when the PDU is destroyed.
 *
 * @param pdu    The specified PDU.
 * @param len    Returns the length of the current data
 * @param data   Returns the ptr to the current data
 * @param offset Returns the offset of the current data from the start of the
 *               body comprising of many blocks (RFC7959)
 * @param total  Returns the total size of the body.
 *               If offset + length < total, then there is more data to follow.
 *
 * @return @c 1 if len, data, offset and total are correctly filled in, else
 *         @c 0 if there is no data.
 */
int coap_get_data_large(const coap_pdu_t *pdu,
                        size_t *len,
                        const uint8_t **data,
                        size_t *offset,
                        size_t *total);

/**
 * Gets the PDU code associated with @p pdu.
 *
 * @param pdu The PDU object.
 *
 * @return The PDU code.
 */
coap_pdu_code_t coap_pdu_get_code(const coap_pdu_t *pdu);

/**
 * Sets the PDU code in the @p pdu.
 *
 * @param pdu The PDU object.
 * @param code The code to set in the PDU.
 */
void coap_pdu_set_code(coap_pdu_t *pdu, coap_pdu_code_t code);

/**
 * Gets the PDU type associated with @p pdu.
 *
 * @param pdu The PDU object.
 *
 * @return The PDU type.
 */
coap_pdu_type_t coap_pdu_get_type(const coap_pdu_t *pdu);

/**
 * Sets the PDU type in the @p pdu.
 *
 * @param pdu The PDU object.
 * @param type The type to set for the PDU.
 */
void coap_pdu_set_type(coap_pdu_t *pdu, coap_pdu_type_t type);

/**
 * Gets the token associated with @p pdu.
 *
 * @param pdu The PDU object.
 *
 * @return The token information.
 */
coap_bin_const_t coap_pdu_get_token(const coap_pdu_t *pdu);

/**
 * Gets the message id associated with @p pdu.
 *
 * @param pdu The PDU object.
 *
 * @return The message id.
 */
coap_mid_t coap_pdu_get_mid(const coap_pdu_t *pdu);

/**
 * Sets the message id in the @p pdu.
 *
 * @param pdu The PDU object.
 * @param mid The message id value to set in the PDU.
 *
 */
void coap_pdu_set_mid(coap_pdu_t *pdu, coap_mid_t mid);

/** @} */

#endif /* COAP_PDU_H_ */
