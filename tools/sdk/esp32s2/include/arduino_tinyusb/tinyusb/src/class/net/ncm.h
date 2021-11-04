/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021, Ha Thach (tinyusb.org)
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


#ifndef _TUSB_NCM_H_
#define _TUSB_NCM_H_

#include "common/tusb_common.h"

#ifdef __cplusplus
 extern "C" {
#endif

// Table 4.3 Data Class Interface Protocol Codes
typedef enum
{
  NCM_DATA_PROTOCOL_NETWORK_TRANSFER_BLOCK = 0x01
} ncm_data_interface_protocol_code_t;


// Table 6.2 Class-Specific Request Codes for Network Control Model subclass
typedef enum
{
  NCM_SET_ETHERNET_MULTICAST_FILTERS               = 0x40,
  NCM_SET_ETHERNET_POWER_MANAGEMENT_PATTERN_FILTER = 0x41,
  NCM_GET_ETHERNET_POWER_MANAGEMENT_PATTERN_FILTER = 0x42,
  NCM_SET_ETHERNET_PACKET_FILTER                   = 0x43,
  NCM_GET_ETHERNET_STATISTIC                       = 0x44,
  NCM_GET_NTB_PARAMETERS                           = 0x80,
  NCM_GET_NET_ADDRESS                              = 0x81,
  NCM_SET_NET_ADDRESS                              = 0x82,
  NCM_GET_NTB_FORMAT                               = 0x83,
  NCM_SET_NTB_FORMAT                               = 0x84,
  NCM_GET_NTB_INPUT_SIZE                           = 0x85,
  NCM_SET_NTB_INPUT_SIZE                           = 0x86,
  NCM_GET_MAX_DATAGRAM_SIZE                        = 0x87,
  NCM_SET_MAX_DATAGRAM_SIZE                        = 0x88,
  NCM_GET_CRC_MODE                                 = 0x89,
  NCM_SET_CRC_MODE                                 = 0x8A,
} ncm_request_code_t;

#ifdef __cplusplus
 }
#endif

#endif
