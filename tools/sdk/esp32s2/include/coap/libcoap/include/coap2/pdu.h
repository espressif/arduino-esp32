/*
 * pdu.h -- CoAP message structure
 *
 * Copyright (C) 2010-2014 Olaf Bergmann <bergmann@tzi.org>
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

struct coap_session_t;

#ifdef WITH_LWIP
#include <lwip/pbuf.h>
#endif

#include <stdint.h>

#define COAP_DEFAULT_PORT      5683 /* CoAP default UDP/TCP port */
#define COAPS_DEFAULT_PORT     5684 /* CoAP default UDP/TCP port for secure transmission */
#define COAP_DEFAULT_MAX_AGE     60 /* default maximum object lifetime in seconds */
#ifndef COAP_DEFAULT_MTU
#define COAP_DEFAULT_MTU       1152
#endif /* COAP_DEFAULT_MTU */

/* TCP Message format constants, do not modify */
#define COAP_MESSAGE_SIZE_OFFSET_TCP8 13
#define COAP_MESSAGE_SIZE_OFFSET_TCP16 269 /* 13 + 256 */
#define COAP_MESSAGE_SIZE_OFFSET_TCP32 65805 /* 269 + 65536 */

/* Derived message size limits */
#define COAP_MAX_MESSAGE_SIZE_TCP0 (COAP_MESSAGE_SIZE_OFFSET_TCP8-1) /* 12 */
#define COAP_MAX_MESSAGE_SIZE_TCP8 (COAP_MESSAGE_SIZE_OFFSET_TCP16-1) /* 268 */
#define COAP_MAX_MESSAGE_SIZE_TCP16 (COAP_MESSAGE_SIZE_OFFSET_TCP32-1) /* 65804 */
#define COAP_MAX_MESSAGE_SIZE_TCP32 (COAP_MESSAGE_SIZE_OFFSET_TCP32+0xFFFFFFFF)

#ifndef COAP_DEFAULT_MAX_PDU_RX_SIZE
#if defined(WITH_CONTIKI) || defined(WITH_LWIP)
#define COAP_DEFAULT_MAX_PDU_RX_SIZE (COAP_MAX_MESSAGE_SIZE_TCP16+4)
#else
/* 8 MiB max-message-size plus some space for options */
#define COAP_DEFAULT_MAX_PDU_RX_SIZE (8*1024*1024+256)
#endif
#endif /* COAP_DEFAULT_MAX_PDU_RX_SIZE */

#ifndef COAP_DEBUG_BUF_SIZE
#if defined(WITH_CONTIKI) || defined(WITH_LWIP)
#define COAP_DEBUG_BUF_SIZE 128
#else /* defined(WITH_CONTIKI) || defined(WITH_LWIP) */
/* 1024 derived from RFC7252 4.6.  Message Size max payload */
#define COAP_DEBUG_BUF_SIZE (8 + 1024 * 2)
#endif /* defined(WITH_CONTIKI) || defined(WITH_LWIP) */
#endif /* COAP_DEBUG_BUF_SIZE */

#define COAP_DEFAULT_VERSION      1 /* version of CoAP supported */
#define COAP_DEFAULT_SCHEME  "coap" /* the default scheme for CoAP URIs */

/** well-known resources URI */
#define COAP_DEFAULT_URI_WELLKNOWN ".well-known/core"

/* CoAP message types */

#define COAP_MESSAGE_CON       0 /* confirmable message (requires ACK/RST) */
#define COAP_MESSAGE_NON       1 /* non-confirmable message (one-shot message) */
#define COAP_MESSAGE_ACK       2 /* used to acknowledge confirmable messages */
#define COAP_MESSAGE_RST       3 /* indicates error in received messages */

/* CoAP request methods */

#define COAP_REQUEST_GET       1
#define COAP_REQUEST_POST      2
#define COAP_REQUEST_PUT       3
#define COAP_REQUEST_DELETE    4
#define COAP_REQUEST_FETCH     5 /* RFC 8132 */
#define COAP_REQUEST_PATCH     6 /* RFC 8132 */
#define COAP_REQUEST_IPATCH    7 /* RFC 8132 */

/*
 * CoAP option types (be sure to update coap_option_check_critical() when
 * adding options
 */

#define COAP_OPTION_IF_MATCH        1 /* C, opaque, 0-8 B, (none) */
#define COAP_OPTION_URI_HOST        3 /* C, String, 1-255 B, destination address */
#define COAP_OPTION_ETAG            4 /* E, opaque, 1-8 B, (none) */
#define COAP_OPTION_IF_NONE_MATCH   5 /* empty, 0 B, (none) */
#define COAP_OPTION_URI_PORT        7 /* C, uint, 0-2 B, destination port */
#define COAP_OPTION_LOCATION_PATH   8 /* E, String, 0-255 B, - */
#define COAP_OPTION_URI_PATH       11 /* C, String, 0-255 B, (none) */
#define COAP_OPTION_CONTENT_FORMAT 12 /* E, uint, 0-2 B, (none) */
#define COAP_OPTION_CONTENT_TYPE COAP_OPTION_CONTENT_FORMAT
#define COAP_OPTION_MAXAGE         14 /* E, uint, 0--4 B, 60 Seconds */
#define COAP_OPTION_URI_QUERY      15 /* C, String, 1-255 B, (none) */
#define COAP_OPTION_ACCEPT         17 /* C, uint,   0-2 B, (none) */
#define COAP_OPTION_LOCATION_QUERY 20 /* E, String,   0-255 B, (none) */
#define COAP_OPTION_SIZE2          28 /* E, uint, 0-4 B, (none) */
#define COAP_OPTION_PROXY_URI      35 /* C, String, 1-1034 B, (none) */
#define COAP_OPTION_PROXY_SCHEME   39 /* C, String, 1-255 B, (none) */
#define COAP_OPTION_SIZE1          60 /* E, uint, 0-4 B, (none) */

/* option types from RFC 7641 */

#define COAP_OPTION_OBSERVE         6 /* E, empty/uint, 0 B/0-3 B, (none) */
#define COAP_OPTION_SUBSCRIPTION  COAP_OPTION_OBSERVE

/* selected option types from RFC 7959 */

#define COAP_OPTION_BLOCK2         23 /* C, uint, 0--3 B, (none) */
#define COAP_OPTION_BLOCK1         27 /* C, uint, 0--3 B, (none) */

/* selected option types from RFC 7967 */

#define COAP_OPTION_NORESPONSE    258 /* N, uint, 0--1 B, 0 */

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

/* The following definitions exist for backwards compatibility */
#if 0 /* this does not exist any more */
#define COAP_RESPONSE_100      40 /* 100 Continue */
#endif
#define COAP_RESPONSE_200      COAP_RESPONSE_CODE(200)  /* 2.00 OK */
#define COAP_RESPONSE_201      COAP_RESPONSE_CODE(201)  /* 2.01 Created */
#define COAP_RESPONSE_304      COAP_RESPONSE_CODE(203)  /* 2.03 Valid */
#define COAP_RESPONSE_400      COAP_RESPONSE_CODE(400)  /* 4.00 Bad Request */
#define COAP_RESPONSE_404      COAP_RESPONSE_CODE(404)  /* 4.04 Not Found */
#define COAP_RESPONSE_405      COAP_RESPONSE_CODE(405)  /* 4.05 Method Not Allowed */
#define COAP_RESPONSE_415      COAP_RESPONSE_CODE(415)  /* 4.15 Unsupported Media Type */
#define COAP_RESPONSE_500      COAP_RESPONSE_CODE(500)  /* 5.00 Internal Server Error */
#define COAP_RESPONSE_501      COAP_RESPONSE_CODE(501)  /* 5.01 Not Implemented */
#define COAP_RESPONSE_503      COAP_RESPONSE_CODE(503)  /* 5.03 Service Unavailable */
#define COAP_RESPONSE_504      COAP_RESPONSE_CODE(504)  /* 5.04 Gateway Timeout */
#if 0  /* these response codes do not have a valid code any more */
#  define COAP_RESPONSE_X_240    240   /* Token Option required by server */
#  define COAP_RESPONSE_X_241    241   /* Uri-Authority Option required by server */
#endif
#define COAP_RESPONSE_X_242    COAP_RESPONSE_CODE(402)  /* Critical Option not supported */

#define COAP_SIGNALING_CODE(N) (((N)/100 << 5) | (N)%100)
#define COAP_SIGNALING_CSM     COAP_SIGNALING_CODE(701)
#define COAP_SIGNALING_PING    COAP_SIGNALING_CODE(702)
#define COAP_SIGNALING_PONG    COAP_SIGNALING_CODE(703)
#define COAP_SIGNALING_RELEASE COAP_SIGNALING_CODE(704)
#define COAP_SIGNALING_ABORT   COAP_SIGNALING_CODE(705)

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

/* Note that identifiers for registered media types are in the range 0-65535. We
 * use an unallocated type here and hope for the best. */
#define COAP_MEDIATYPE_ANY                         0xff /* any media type */

/**
 * coap_tid_t is used to store CoAP transaction id, i.e. a hash value
 * built from the remote transport address and the message id of a
 * CoAP PDU.  Valid transaction ids are greater or equal zero.
 */
typedef int coap_tid_t;

/** Indicates an invalid transaction id. */
#define COAP_INVALID_TID -1

/**
 * Indicates that a response is suppressed. This will occur for error
 * responses if the request was received via IP multicast.
 */
#define COAP_DROPPED_RESPONSE -2

#define COAP_PDU_DELAYED -3

#define COAP_OPT_LONG 0x0F      /* OC == 0b1111 indicates that the option list
                                 * in a CoAP message is limited by 0b11110000
                                 * marker */

#define COAP_OPT_END 0xF0       /* end marker */

#define COAP_PAYLOAD_START 0xFF /* payload marker */

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

typedef struct coap_pdu_t {
  uint8_t type;             /**< message type */
  uint8_t code;             /**< request method (value 1--10) or response code (value 40-255) */
  uint8_t max_hdr_size;     /**< space reserved for protocol-specific header */
  uint8_t hdr_size;         /**< actaul size used for protocol-specific header */
  uint8_t token_length;     /**< length of Token */
  uint16_t tid;             /**< transaction id, if any, in regular host byte order */
  uint16_t max_delta;       /**< highest option number */
  size_t alloc_size;        /**< allocated storage for token, options and payload */
  size_t used_size;         /**< used bytes of storage for token, options and payload */
  size_t max_size;          /**< maximum size for token, options and payload, or zero for variable size pdu */
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
} coap_pdu_t;

#define COAP_PDU_IS_EMPTY(pdu)     ((pdu)->code == 0)
#define COAP_PDU_IS_REQUEST(pdu)   (!COAP_PDU_IS_EMPTY(pdu) && (pdu)->code < 32)
#define COAP_PDU_IS_RESPONSE(pdu)  ((pdu)->code >= 64 && (pdu)->code < 224)
#define COAP_PDU_IS_SIGNALING(pdu) ((pdu)->code >= 224)

#define COAP_PDU_MAX_UDP_HEADER_SIZE 4
#define COAP_PDU_MAX_TCP_HEADER_SIZE 6

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

typedef uint8_t coap_proto_t;
/**
* coap_proto_t values
*/
#define COAP_PROTO_NONE         0
#define COAP_PROTO_UDP          1
#define COAP_PROTO_DTLS         2
#define COAP_PROTO_TCP          3
#define COAP_PROTO_TLS          4

/**
 * Creates a new CoAP PDU with at least enough storage space for the given
 * @p size maximum message size. The function returns a pointer to the
 * node coap_pdu_t object on success, or @c NULL on error. The storage allocated
 * for the result must be released with coap_delete_pdu() if coap_send() is not
 * called.
 *
 * @param type The type of the PDU (one of COAP_MESSAGE_CON, COAP_MESSAGE_NON,
 *             COAP_MESSAGE_ACK, COAP_MESSAGE_RST).
 * @param code The message code.
 * @param tid  The transcation id to set or 0 if unknown / not applicable.
 * @param size The maximum allowed number of byte for the message.
 * @return     A pointer to the new PDU object or @c NULL on error.
 */
coap_pdu_t *
coap_pdu_init(uint8_t type, uint8_t code, uint16_t tid, size_t size);

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
 * Clears any contents from @p pdu and resets @c used_size,
 * and @c data pointers. @c max_size is set to @p size, any
 * other field is set to @c 0. Note that @p pdu must be a valid
 * pointer to a coap_pdu_t object created e.g. by coap_pdu_init().
 */
void coap_pdu_clear(coap_pdu_t *pdu, size_t size);

/**
 * Creates a new CoAP PDU.
 */
coap_pdu_t *coap_new_pdu(const struct coap_session_t *session);

/**
 * Dispose of an CoAP PDU and frees associated storage.
 * Not that in general you should not call this function directly.
 * When a PDU is sent with coap_send(), coap_delete_pdu() will be
 * called automatically for you.
 */

void coap_delete_pdu(coap_pdu_t *);

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
 * @param pdu     The PDU structure to.
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
 * Adds option of given type to pdu that is passed as first
 * parameter.
 * coap_add_option() destroys the PDU's data, so coap_add_data() must be called
 * after all options have been added. As coap_add_token() destroys the options
 * following the token, the token must be added before coap_add_option() is
 * called. This function returns the number of bytes written or @c 0 on error.
 */
size_t coap_add_option(coap_pdu_t *pdu,
                       uint16_t type,
                       size_t len,
                       const uint8_t *data);

/**
 * Adds option of given type to pdu that is passed as first parameter, but does
 * not write a value. It works like coap_add_option with respect to calling
 * sequence (i.e. after token and before data). This function returns a memory
 * address to which the option data has to be written before the PDU can be
 * sent, or @c NULL on error.
 */
uint8_t *coap_add_option_later(coap_pdu_t *pdu,
                               uint16_t type,
                               size_t len);

/**
 * Adds given data to the pdu that is passed as first parameter. Note that the
 * PDU's data is destroyed by coap_add_option(). coap_add_data() must be called
 * only once per PDU, otherwise the result is undefined.
 */
int coap_add_data(coap_pdu_t *pdu,
                  size_t len,
                  const uint8_t *data);

/**
 * Adds given data to the pdu that is passed as first parameter but does not
 * copyt it. Note that the PDU's data is destroyed by coap_add_option().
 * coap_add_data() must be have been called once for this PDU, otherwise the
 * result is undefined.
 * The actual data must be copied at the returned location.
 */
uint8_t *coap_add_data_after(coap_pdu_t *pdu, size_t len);

/**
 * Retrieves the length and data pointer of specified PDU. Returns 0 on error or
 * 1 if *len and *data have correct values. Note that these values are destroyed
 * with the pdu.
 */
int coap_get_data(const coap_pdu_t *pdu,
                  size_t *len,
                  uint8_t **data);

/**
 * Compose the protocol specific header for the specified PDU.
 * @param pdu A newly composed PDU.
 * @param proto The target wire protocol.
 * @return Number of header bytes prepended before pdu->token or 0 on error.
 */

size_t coap_pdu_encode_header(coap_pdu_t *pdu, coap_proto_t proto);

#endif /* COAP_PDU_H_ */
