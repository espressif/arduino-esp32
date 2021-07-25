/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 XMOS LIMITED
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

#ifndef _TUSB_DFU_H_
#define _TUSB_DFU_H_

#include "common/tusb_common.h"

#ifdef __cplusplus
  extern "C" {
#endif

//--------------------------------------------------------------------+
// Common Definitions
//--------------------------------------------------------------------+

// DFU Protocol
typedef enum
{
  DFU_PROTOCOL_RT  = 0x01,
  DFU_PROTOCOL_DFU = 0x02,
} dfu_protocol_type_t;

// DFU Descriptor Type
typedef enum
{
  DFU_DESC_FUNCTIONAL = 0x21,
} dfu_descriptor_type_t;

// DFU Requests
typedef enum {
  DFU_REQUEST_DETACH         = 0,
  DFU_REQUEST_DNLOAD         = 1,
  DFU_REQUEST_UPLOAD         = 2,
  DFU_REQUEST_GETSTATUS      = 3,
  DFU_REQUEST_CLRSTATUS      = 4,
  DFU_REQUEST_GETSTATE       = 5,
  DFU_REQUEST_ABORT          = 6,
} dfu_requests_t;

// DFU States
typedef enum {
  APP_IDLE                   = 0,
  APP_DETACH                 = 1,
  DFU_IDLE                   = 2,
  DFU_DNLOAD_SYNC            = 3,
  DFU_DNBUSY                 = 4,
  DFU_DNLOAD_IDLE            = 5,
  DFU_MANIFEST_SYNC          = 6,
  DFU_MANIFEST               = 7,
  DFU_MANIFEST_WAIT_RESET    = 8,
  DFU_UPLOAD_IDLE            = 9,
  DFU_ERROR                  = 10,
} dfu_state_t;

// DFU Status
typedef enum {
  DFU_STATUS_OK               = 0x00,
  DFU_STATUS_ERR_TARGET       = 0x01,
  DFU_STATUS_ERR_FILE         = 0x02,
  DFU_STATUS_ERR_WRITE        = 0x03,
  DFU_STATUS_ERR_ERASE        = 0x04,
  DFU_STATUS_ERR_CHECK_ERASED = 0x05,
  DFU_STATUS_ERR_PROG         = 0x06,
  DFU_STATUS_ERR_VERIFY       = 0x07,
  DFU_STATUS_ERR_ADDRESS      = 0x08,
  DFU_STATUS_ERR_NOTDONE      = 0x09,
  DFU_STATUS_ERR_FIRMWARE     = 0x0A,
  DFU_STATUS_ERR_VENDOR       = 0x0B,
  DFU_STATUS_ERR_USBR         = 0x0C,
  DFU_STATUS_ERR_POR          = 0x0D,
  DFU_STATUS_ERR_UNKNOWN      = 0x0E,
  DFU_STATUS_ERR_STALLEDPKT   = 0x0F,
} dfu_status_t;

#define DFU_ATTR_CAN_DOWNLOAD              (1u << 0)
#define DFU_ATTR_CAN_UPLOAD                (1u << 1)
#define DFU_ATTR_MANIFESTATION_TOLERANT    (1u << 2)
#define DFU_ATTR_WILL_DETACH               (1u << 3)

// DFU Status Request Payload
typedef struct TU_ATTR_PACKED
{
  uint8_t bStatus;
  uint8_t bwPollTimeout[3];
  uint8_t bState;
  uint8_t iString;
} dfu_status_response_t;

TU_VERIFY_STATIC( sizeof(dfu_status_response_t) == 6, "size is not correct");

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_DFU_H_ */
