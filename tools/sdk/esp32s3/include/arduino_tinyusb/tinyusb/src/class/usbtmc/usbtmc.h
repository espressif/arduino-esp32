
/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 N Conrad
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef _TUSB_USBTMC_H__
#define _TUSB_USBTMC_H__

#include "common/tusb_common.h"


/* Implements USBTMC Revision 1.0, April 14, 2003

 String descriptors must have a "LANGID=0x409"/US English string.
 Characters must be 0x20 (' ') to 0x7E ('~') ASCII,
   But MUST not contain: "/:?\*
   Also must not have leading or trailing space (' ')
 Device descriptor must state USB version 0x0200 or greater

 If USB488DeviceCapabilites.D2 = 1 (SR1), then there must be a INT endpoint.
*/

#define USBTMC_VERSION 0x0100
#define USBTMC_488_VERSION 0x0100

typedef enum {
  USBTMC_MSGID_DEV_DEP_MSG_OUT = 1u,
  USBTMC_MSGID_DEV_DEP_MSG_IN = 2u,
  USBTMC_MSGID_VENDOR_SPECIFIC_MSG_OUT = 126u,
  USBTMC_MSGID_VENDOR_SPECIFIC_IN = 127u,
  USBTMC_MSGID_USB488_TRIGGER = 128u,
} usbtmc_msgid_enum;

/// \brief Message header (For BULK OUT and BULK IN); 4 bytes
typedef struct TU_ATTR_PACKED
{
  uint8_t MsgID              ; ///< Message type ID (usbtmc_msgid_enum)
  uint8_t bTag    		       ; ///< Transfer ID 1<=bTag<=255
  uint8_t bTagInverse        ; ///< Complement of the tag
  uint8_t _reserved           ; ///< Must be 0x00
} usbtmc_msg_header_t;

typedef struct TU_ATTR_PACKED
{
  usbtmc_msg_header_t header;
  uint8_t data[8];
} usbtmc_msg_generic_t;

/* Uses on the bulk-out endpoint: */
// Next 8 bytes are message-specific
typedef struct TU_ATTR_PACKED {
	usbtmc_msg_header_t header ; ///< Header
	uint32_t TransferSize      ; ///< Transfer size; LSB first
	struct TU_ATTR_PACKED
	{
	  unsigned int EOM  : 1         ; ///< EOM set on last byte
  } bmTransferAttributes;
  uint8_t _reserved[3];
} usbtmc_msg_request_dev_dep_out;

TU_VERIFY_STATIC(sizeof(usbtmc_msg_request_dev_dep_out) == 12u, "struct wrong length");

// Next 8 bytes are message-specific
typedef struct TU_ATTR_PACKED
{
  usbtmc_msg_header_t header ; ///< Header
  uint32_t TransferSize      ; ///< Transfer size; LSB first
  struct TU_ATTR_PACKED
  {
    unsigned int TermCharEnabled  : 1 ; ///< "The Bulk-IN transfer must terminate on the specified TermChar."; CAPABILITIES must list TermChar
  } bmTransferAttributes;
  uint8_t TermChar;
  uint8_t _reserved[2];
} usbtmc_msg_request_dev_dep_in;

TU_VERIFY_STATIC(sizeof(usbtmc_msg_request_dev_dep_in) == 12u, "struct wrong length");

/* Bulk-in headers */

typedef struct TU_ATTR_PACKED
{
  usbtmc_msg_header_t header;
  uint32_t TransferSize;
  struct TU_ATTR_PACKED
  {
    uint8_t EOM: 1;           ///< Last byte of transfer is the end of the message
    uint8_t UsingTermChar: 1; ///< Support TermChar && Request.TermCharEnabled && last char in transfer is TermChar
  } bmTransferAttributes;
  uint8_t _reserved[3];
} usbtmc_msg_dev_dep_msg_in_header_t;

TU_VERIFY_STATIC(sizeof(usbtmc_msg_dev_dep_msg_in_header_t) == 12u, "struct wrong length");

/* Unsupported vendor things.... Are these ever used?*/

typedef struct TU_ATTR_PACKED
{
  usbtmc_msg_header_t header ; ///< Header
  uint32_t TransferSize      ; ///< Transfer size; LSB first
  uint8_t _reserved[4];
} usbtmc_msg_request_vendor_specific_out;


TU_VERIFY_STATIC(sizeof(usbtmc_msg_request_vendor_specific_out) == 12u, "struct wrong length");

typedef struct TU_ATTR_PACKED
{
  usbtmc_msg_header_t header ; ///< Header
  uint32_t TransferSize      ; ///< Transfer size; LSB first
  uint8_t _reserved[4];
} usbtmc_msg_request_vendor_specific_in;

TU_VERIFY_STATIC(sizeof(usbtmc_msg_request_vendor_specific_in) == 12u, "struct wrong length");

// Control request type should use tusb_control_request_t

/*
typedef struct TU_ATTR_PACKED {
  struct {
    unsigned int Recipient  : 5         ; ///< EOM set on last byte
    unsigned int Type       : 2         ; ///< EOM set on last byte
    unsigned int DirectionToHost  : 1   ; ///< 0 is OUT, 1 is IN
  } bmRequestType;
  uint8_t bRequest                 ; ///< If bmRequestType.Type = Class, see usmtmc_request_type_enum
  uint16_t wValue                  ;
  uint16_t wIndex                  ;
  uint16_t wLength                 ; // Number of bytes in data stage
} usbtmc_class_specific_control_req;

*/
// bulk-in protocol errors
enum {
  USBTMC_BULK_IN_ERR_INCOMPLETE_HEADER = 1u,
  USBTMC_BULK_IN_ERR_UNSUPPORTED = 2u,
  USBTMC_BULK_IN_ERR_BAD_PARAMETER = 3u,
  USBTMC_BULK_IN_ERR_DATA_TOO_SHORT = 4u,
  USBTMC_BULK_IN_ERR_DATA_TOO_LONG = 5u,
};
// bult-in halt errors
enum {
  USBTMC_BULK_IN_ERR = 1u, ///< receives a USBTMC command message that expects a response while a
                           /// Bulk-IN transfer is in progress
};

typedef enum {
  USBTMC_bREQUEST_INITIATE_ABORT_BULK_OUT      = 1u,
  USBTMC_bREQUEST_CHECK_ABORT_BULK_OUT_STATUS  = 2u,
  USBTMC_bREQUEST_INITIATE_ABORT_BULK_IN       = 3u,
  USBTMC_bREQUEST_CHECK_ABORT_BULK_IN_STATUS   = 4u,
  USBTMC_bREQUEST_INITIATE_CLEAR               = 5u,
  USBTMC_bREQUEST_CHECK_CLEAR_STATUS           = 6u,
  USBTMC_bREQUEST_GET_CAPABILITIES             = 7u,

  USBTMC_bREQUEST_INDICATOR_PULSE               = 64u, // Optional

  /****** USBTMC 488 *************/
  USB488_bREQUEST_READ_STATUS_BYTE  = 128u,
  USB488_bREQUEST_REN_CONTROL       = 160u,
  USB488_bREQUEST_GO_TO_LOCAL       = 161u,
  USB488_bREQUEST_LOCAL_LOCKOUT     = 162u,

} usmtmc_request_type_enum;

typedef enum {
  USBTMC_STATUS_SUCCESS = 0x01,
  USBTMC_STATUS_PENDING = 0x02,
  USBTMC_STATUS_FAILED = 0x80,
  USBTMC_STATUS_TRANSFER_NOT_IN_PROGRESS = 0x81,
  USBTMC_STATUS_SPLIT_NOT_IN_PROGRESS = 0x82,
  USBTMC_STATUS_SPLIT_IN_PROGRESS  = 0x83
} usbtmc_status_enum;

/************************************************************
 * Control Responses
 */

typedef struct TU_ATTR_PACKED {
  uint8_t USBTMC_status;                 ///< usbtmc_status_enum
  uint8_t _reserved;
  uint16_t bcdUSBTMC;                    ///< USBTMC_VERSION

  struct TU_ATTR_PACKED
  {
    unsigned int listenOnly :1;
    unsigned int talkOnly :1;
    unsigned int supportsIndicatorPulse :1;
  } bmIntfcCapabilities;
  struct TU_ATTR_PACKED
  {
    unsigned int canEndBulkInOnTermChar :1;
  } bmDevCapabilities;
  uint8_t _reserved2[6];
  uint8_t _reserved3[12];
} usbtmc_response_capabilities_t;

TU_VERIFY_STATIC(sizeof(usbtmc_response_capabilities_t) == 0x18, "struct wrong length");

typedef struct TU_ATTR_PACKED
{
  uint8_t USBTMC_status;
  struct TU_ATTR_PACKED
  {
    unsigned int BulkInFifoBytes :1;
  } bmClear;
} usbtmc_get_clear_status_rsp_t;

TU_VERIFY_STATIC(sizeof(usbtmc_get_clear_status_rsp_t) == 2u, "struct wrong length");

// Used for both abort bulk IN and bulk OUT
typedef struct TU_ATTR_PACKED
{
  uint8_t USBTMC_status;
  uint8_t bTag;
} usbtmc_initiate_abort_rsp_t;

TU_VERIFY_STATIC(sizeof(usbtmc_get_clear_status_rsp_t) == 2u, "struct wrong length");

// Used for both check_abort_bulk_in_status and check_abort_bulk_out_status
typedef struct TU_ATTR_PACKED
{
  uint8_t USBTMC_status;
  struct TU_ATTR_PACKED
  {
    unsigned int BulkInFifoBytes : 1; ///< Has queued data or a short packet that is queued
  } bmAbortBulkIn;
  uint8_t _reserved[2];               ///< Must be zero
  uint32_t NBYTES_RXD_TXD;
} usbtmc_check_abort_bulk_rsp_t;

TU_VERIFY_STATIC(sizeof(usbtmc_check_abort_bulk_rsp_t) == 8u, "struct wrong length");

typedef struct TU_ATTR_PACKED
{
  uint8_t USBTMC_status;                 ///< usbtmc_status_enum
  uint8_t _reserved;
  uint16_t bcdUSBTMC;                    ///< USBTMC_VERSION

  struct TU_ATTR_PACKED
  {
    unsigned int listenOnly :1;
    unsigned int talkOnly :1;
    unsigned int supportsIndicatorPulse :1;
  } bmIntfcCapabilities;

  struct TU_ATTR_PACKED
  {
    unsigned int canEndBulkInOnTermChar :1;
  } bmDevCapabilities;

  uint8_t _reserved2[6];
  uint16_t bcdUSB488;

  struct TU_ATTR_PACKED
  {
    unsigned int is488_2 :1;
    unsigned int supportsREN_GTL_LLO :1;
    unsigned int supportsTrigger :1;
  } bmIntfcCapabilities488;

  struct TU_ATTR_PACKED
  {
    unsigned int SCPI :1;
    unsigned int SR1 :1;
    unsigned int RL1 :1;
    unsigned int DT1 :1;
  } bmDevCapabilities488;
  uint8_t _reserved3[8];
} usbtmc_response_capabilities_488_t;

TU_VERIFY_STATIC(sizeof(usbtmc_response_capabilities_488_t) == 0x18, "struct wrong length");

typedef struct TU_ATTR_PACKED
{
  uint8_t USBTMC_status;
  uint8_t bTag;
  uint8_t statusByte;
} usbtmc_read_stb_rsp_488_t;

TU_VERIFY_STATIC(sizeof(usbtmc_read_stb_rsp_488_t) == 3u, "struct wrong length");

typedef struct TU_ATTR_PACKED
{
  struct TU_ATTR_PACKED
  {
      unsigned int bTag : 7;
      unsigned int one  : 1;
  } bNotify1;
  uint8_t StatusByte;
} usbtmc_read_stb_interrupt_488_t;

TU_VERIFY_STATIC(sizeof(usbtmc_read_stb_interrupt_488_t) == 2u, "struct wrong length");

#endif

