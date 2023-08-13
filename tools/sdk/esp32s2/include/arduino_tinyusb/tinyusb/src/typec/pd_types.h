/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 Ha Thach (tinyusb.org)
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

#ifndef _TUSB_PD_TYPES_H_
#define _TUSB_PD_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "common/tusb_compiler.h"

// Start of all packed definitions for compiler without per-type packed
TU_ATTR_PACKED_BEGIN
TU_ATTR_BIT_FIELD_ORDER_BEGIN

//--------------------------------------------------------------------+
// TYPE-C
//--------------------------------------------------------------------+

typedef enum {
  TUSB_TYPEC_PORT_SRC,
  TUSB_TYPEC_PORT_SNK,
  TUSB_TYPEC_PORT_DRP
} tusb_typec_port_type_t;

enum {
  PD_CTRL_RESERVED = 0,            // 0b00000: 0
  PD_CTRL_GOOD_CRC,                // 0b00001: 1
  PD_CTRL_GO_TO_MIN,               // 0b00010: 2
  PD_CTRL_ACCEPT,                  // 0b00011: 3
  PD_CTRL_REJECT,                  // 0b00100: 4
  PD_CTRL_PING,                    // 0b00101: 5
  PD_CTRL_PS_READY,                // 0b00110: 6
  PD_CTRL_GET_SOURCE_CAP,          // 0b00111: 7
  PD_CTRL_GET_SINK_CAP,            // 0b01000: 8
  PD_CTRL_DR_SWAP,                 // 0b01001: 9
  PD_CTRL_PR_SWAP,                 // 0b01010: 10
  PD_CTRL_VCONN_SWAP,              // 0b01011: 11
  PD_CTRL_WAIT,                    // 0b01100: 12
  PD_CTRL_SOFT_RESET,              // 0b01101: 13
  PD_CTRL_DATA_RESET,              // 0b01110: 14
  PD_CTRL_DATA_RESET_COMPLETE,     // 0b01111: 15
  PD_CTRL_NOT_SUPPORTED,           // 0b10000: 16
  PD_CTRL_GET_SOURCE_CAP_EXTENDED, // 0b10001: 17
  PD_CTRL_GET_STATUS,              // 0b10010: 18
  PD_CTRL_FR_SWAP,                 // 0b10011: 19
  PD_CTRL_GET_PPS_STATUS,          // 0b10100: 20
  PD_CTRL_GET_COUNTRY_CODES,       // 0b10101: 21
  PD_CTRL_GET_SINK_CAP_EXTENDED,   // 0b10110: 22
  PD_CTRL_GET_SOURCE_INFO,         // 0b10111: 23
  PD_CTRL_REVISION,                // 0b11000: 24
};

enum {
  PD_DATA_RESERVED = 0,     // 0b00000: 0
  PD_DATA_SOURCE_CAP,       // 0b00001: 1
  PD_DATA_REQUEST,          // 0b00010: 2
  PD_DATA_BIST,             // 0b00011: 3
  PD_DATA_SINK_CAP,         // 0b00100: 4
  PD_DATA_BATTERY_STATUS,   // 0b00101: 5
  PD_DATA_ALERT,            // 0b00110: 6
  PD_DATA_GET_COUNTRY_INFO, // 0b00111: 7
  PD_DATA_ENTER_USB,        // 0b01000: 8
  PD_DATA_EPR_REQUEST,      // 0b01001: 9
  PD_DATA_EPR_MODE,         // 0b01010: 10
  PD_DATA_SRC_INFO,         // 0b01011: 11
  PD_DATA_REVISION,         // 0b01100: 12
  PD_DATA_RESERVED_13,      // 0b01101: 13
  PD_DATA_RESERVED_14,      // 0b01110: 14
  PD_DATA_VENDOR_DEFINED,   // 0b01111: 15
};

enum {
  PD_REV_10	= 0x0,
  PD_REV_20	= 0x1,
  PD_REV_30	= 0x2,
};

enum {
  PD_DATA_ROLE_UFP	= 0x0,
  PD_DATA_ROLE_DFP	= 0x1,
};

enum {
  PD_POWER_ROLE_SINK	= 0x0,
  PD_POWER_ROLE_SOURCE	= 0x1,
};

typedef struct TU_ATTR_PACKED {
  uint16_t msg_type   : 5; // [0:4]
  uint16_t data_role  : 1; // [5] SOP only: 0 UFP, 1 DFP
  uint16_t specs_rev  : 2; // [6:7]
  uint16_t power_role : 1; // [8] SOP only: 0 Sink, 1 Source
  uint16_t msg_id     : 3; // [9:11]
  uint16_t n_data_obj : 3; // [12:14]
  uint16_t extended   : 1; // [15]
} pd_header_t;
TU_VERIFY_STATIC(sizeof(pd_header_t) == 2, "size is not correct");

typedef struct TU_ATTR_PACKED {
  uint16_t data_size     : 9; // [0:8]
  uint16_t reserved      : 1; // [9]
  uint16_t request_chunk : 1; // [10]
  uint16_t chunk_number  : 4; // [11:14]
  uint16_t chunked       : 1; // [15]
} pd_header_extended_t;
TU_VERIFY_STATIC(sizeof(pd_header_extended_t) == 2, "size is not correct");

//--------------------------------------------------------------------+
// Source Capability
//--------------------------------------------------------------------+

// All table references are from USBPD Specification rev3.1 version 1.8
enum {
  PD_PDO_TYPE_FIXED = 0, // Vmin = Vmax
  PD_PDO_TYPE_BATTERY,
  PD_PDO_TYPE_VARIABLE, // non-battery
  PD_PDO_TYPE_APDO, // Augmented Power Data Object
};

// Fixed Power Data Object (PDO) table 6-9
typedef struct TU_ATTR_PACKED {
  uint32_t current_max_10ma          : 10; // [9..0] Max current in 10mA unit
  uint32_t voltage_50mv              : 10; // [19..10] Voltage in 50mV unit
  uint32_t current_peak              :  2; // [21..20] Peak current
  uint32_t reserved                  :  1; // [22] Reserved
  uint32_t epr_mode_capable          :  1; // [23] epr_mode_capable
  uint32_t unchunked_ext_msg_support :  1; // [24] UnChunked Extended Message Supported
  uint32_t dual_role_data            :  1; // [25] Dual Role Data
  uint32_t usb_comm_capable          :  1; // [26] USB Communications Capable
  uint32_t unconstrained_power       :  1; // [27] Unconstrained Power
  uint32_t usb_suspend_supported     :  1; // [28] USB Suspend Supported
  uint32_t dual_role_power           :  1; // [29] Dual Role Power
  uint32_t type                      :  2; // [30] Fixed Supply type = PD_PDO_TYPE_FIXED
} pd_pdo_fixed_t;
TU_VERIFY_STATIC(sizeof(pd_pdo_fixed_t) == 4, "Invalid size");

// Battery Power Data Object (PDO) table 6-12
typedef struct TU_ATTR_PACKED {
  uint32_t power_max_250mw   : 10; // [9..0] Max allowable power in 250mW unit
  uint32_t voltage_min_50mv  : 10; // [19..10] Minimum voltage in 50mV unit
  uint32_t voltage_max_50mv  : 10; // [29..20] Maximum voltage in 50mV unit
  uint32_t type              :  2; // [31..30] Battery type = PD_PDO_TYPE_BATTERY
} pd_pdo_battery_t;
TU_VERIFY_STATIC(sizeof(pd_pdo_battery_t) == 4, "Invalid size");

// Variable Power Data Object (PDO) table 6-11
typedef struct TU_ATTR_PACKED {
  uint32_t current_max_10ma  : 10; // [9..0] Max current in 10mA unit
  uint32_t voltage_min_50mv  : 10; // [19..10] Minimum voltage in 50mV unit
  uint32_t voltage_max_50mv  : 10; // [29..20] Maximum voltage in 50mV unit
  uint32_t type              :  2; // [31..30] Variable Supply type = PD_PDO_TYPE_VARIABLE
} pd_pdo_variable_t;
TU_VERIFY_STATIC(sizeof(pd_pdo_variable_t) == 4, "Invalid size");

// Augmented Power Data Object (PDO) table 6-13
typedef struct TU_ATTR_PACKED {
  uint32_t current_max_50ma  :  7; // [6..0] Max current in 50mA unit
  uint32_t reserved1         :  1; // [7] Reserved
  uint32_t voltage_min_100mv :  8; // [15..8] Minimum Voltage in 100mV unit
  uint32_t reserved2         :  1; // [16] Reserved
  uint32_t voltage_max_100mv :  8; // [24..17] Maximum Voltage in 100mV unit
  uint32_t reserved3         :  2; // [26..25] Reserved
  uint32_t pps_power_limited :  1; // [27] PPS Power Limited
  uint32_t spr_programmable  :  2; // [29..28] SPR Programmable Power Supply
  uint32_t type              :  2; // [31..30] Augmented Power Data Object = PD_PDO_TYPE_APDO
} pd_pdo_apdo_t;
TU_VERIFY_STATIC(sizeof(pd_pdo_apdo_t) == 4, "Invalid size");

//--------------------------------------------------------------------+
// Request
//--------------------------------------------------------------------+

typedef struct TU_ATTR_PACKED {
  uint32_t current_extremum_10ma     : 10; // [9..0] Max (give back = 0) or Min (give back = 1) current in 10mA unit
  uint32_t current_operate_10ma      : 10; // [19..10] Operating current in 10mA unit
  uint32_t reserved                  :  2; // [21..20] Reserved
  uint32_t epr_mode_capable          :  1; // [22] EPR mode capable
  uint32_t unchunked_ext_msg_support :  1; // [23] UnChunked Extended Message Supported
  uint32_t no_usb_suspend            :  1; // [24] No USB Suspend
  uint32_t usb_comm_capable          :  1; // [25] USB Communications Capable
  uint32_t capability_mismatch       :  1; // [26] Capability Mismatch
  uint32_t give_back_flag            :  1; // [27] GiveBack Flag: 0 = Max, 1 = Min
  uint32_t object_position           :  4; // [31..28] Object Position
} pd_rdo_fixed_variable_t;
TU_VERIFY_STATIC(sizeof(pd_rdo_fixed_variable_t) == 4, "Invalid size");

typedef struct TU_ATTR_PACKED {
  uint32_t power_extremum_250mw      : 10; // [9..0] Max (give back = 0) or Min (give back = 1) operating power in 250mW unit
  uint32_t power_operate_250mw       : 10; // [19..10] Operating power in 250mW unit
  uint32_t reserved                  :  2; // [21..20] Reserved
  uint32_t epr_mode_capable          :  1; // [22] EPR mode capable
  uint32_t unchunked_ext_msg_support :  1; // [23] UnChunked Extended Message Supported
  uint32_t no_usb_suspend            :  1; // [24] No USB Suspend
  uint32_t usb_comm_capable          :  1; // [25] USB Communications Capable
  uint32_t capability_mismatch       :  1; // [26] Capability Mismatch
  uint32_t give_back_flag            :  1; // [27] GiveBack Flag: 0 = Max, 1 = Min
  uint32_t object_position           :  4; // [31..28] Object Position
} pd_rdo_battery_t;
TU_VERIFY_STATIC(sizeof(pd_rdo_battery_t) == 4, "Invalid size");


TU_ATTR_PACKED_END  // End of all packed definitions
TU_ATTR_BIT_FIELD_ORDER_END


#ifdef __cplusplus
}
#endif

#endif
