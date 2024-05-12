/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021, Ha Thach (tinyusb.org)
 * Copyright (c) 2024, Hardy Griech
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

// NTB buffers size for reception side, must be >> MTU to avoid TCP retransmission (driver issue ?)
// Linux use 2048 as minimal size
#ifndef CFG_TUD_NCM_OUT_NTB_MAX_SIZE
  #define CFG_TUD_NCM_OUT_NTB_MAX_SIZE 3200
#endif

// NTB buffers size for reception side, must be > MTU
// Linux use 2048 as minimal size
#ifndef CFG_TUD_NCM_IN_NTB_MAX_SIZE
  #define CFG_TUD_NCM_IN_NTB_MAX_SIZE 3200
#endif

// Number of NTB buffers for reception side
// Depending on the configuration, this parameter could be increased with the cost of additional RAM requirements
// On Full-Speed (RP2040) :
//    1  - good performance
//    2  - up to 30% more performance with iperf with small packets
//    >2 - no performance gain
// On High-Speed (STM32F7) :
//    No performance gain
#ifndef CFG_TUD_NCM_OUT_NTB_N
  #define CFG_TUD_NCM_OUT_NTB_N 1
#endif

// Number of NTB buffers for transmission side
// Depending on the configuration, this parameter could be increased with the cost of additional RAM requirements
// On Full-Speed (RP2040) :
//    1 - good performance but SystemView shows lost events (on load test)
//    2 - up to 50% more performance with iperf with small packets, "tud_network_can_xmit: request blocked"
//        happens from time to time with SystemView
//    3 - "tud_network_can_xmit: request blocked" never happens
//    >3 - no performance gain
// On High-Speed (STM32F7) :
//    No performance gain
#ifndef CFG_TUD_NCM_IN_NTB_N
  #define CFG_TUD_NCM_IN_NTB_N 1
#endif

// How many datagrams it is allowed to put into an NTB for transmission side
#ifndef CFG_TUD_NCM_IN_MAX_DATAGRAMS_PER_NTB
  #define CFG_TUD_NCM_IN_MAX_DATAGRAMS_PER_NTB 8
#endif

// This tells the host how many datagrams it is allowed to put into an NTB
#ifndef CFG_TUD_NCM_OUT_MAX_DATAGRAMS_PER_NTB
  #define CFG_TUD_NCM_OUT_MAX_DATAGRAMS_PER_NTB 6
#endif

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

#define NTH16_SIGNATURE 0x484D434E
#define NDP16_SIGNATURE_NCM0 0x304D434E
#define NDP16_SIGNATURE_NCM1 0x314D434E

typedef struct TU_ATTR_PACKED {
  uint16_t wLength;
  uint16_t bmNtbFormatsSupported;
  uint32_t dwNtbInMaxSize;
  uint16_t wNdbInDivisor;
  uint16_t wNdbInPayloadRemainder;
  uint16_t wNdbInAlignment;
  uint16_t wReserved;
  uint32_t dwNtbOutMaxSize;
  uint16_t wNdbOutDivisor;
  uint16_t wNdbOutPayloadRemainder;
  uint16_t wNdbOutAlignment;
  uint16_t wNtbOutMaxDatagrams;
} ntb_parameters_t;

typedef struct TU_ATTR_PACKED {
  uint32_t dwSignature;
  uint16_t wHeaderLength;
  uint16_t wSequence;
  uint16_t wBlockLength;
  uint16_t wNdpIndex;
} nth16_t;

typedef struct TU_ATTR_PACKED {
  uint16_t wDatagramIndex;
  uint16_t wDatagramLength;
} ndp16_datagram_t;

typedef struct TU_ATTR_PACKED {
  uint32_t dwSignature;
  uint16_t wLength;
  uint16_t wNextNdpIndex;
  //ndp16_datagram_t datagram[];
} ndp16_t;

typedef union TU_ATTR_PACKED {
  struct {
    nth16_t nth;
    ndp16_t ndp;
    ndp16_datagram_t ndp_datagram[CFG_TUD_NCM_IN_MAX_DATAGRAMS_PER_NTB + 1];
  };
  uint8_t data[CFG_TUD_NCM_IN_NTB_MAX_SIZE];
} xmit_ntb_t;

typedef union TU_ATTR_PACKED {
  struct {
    nth16_t nth;
    // only the header is at a guaranteed position
  };
  uint8_t data[CFG_TUD_NCM_OUT_NTB_MAX_SIZE];
} recv_ntb_t;

struct ncm_notify_t {
  tusb_control_request_t header;
  uint32_t downlink, uplink;
};

#endif
