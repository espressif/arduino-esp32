/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Sylvain Munaut <tnt@246tNt.com>
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

#ifndef _TUSB_DFU_RT_DEVICE_H_
#define _TUSB_DFU_RT_DEVICE_H_

#include "common/tusb_common.h"
#include "device/usbd.h"

#ifdef __cplusplus
 extern "C" {
#endif


//--------------------------------------------------------------------+
// Common Definitions
//--------------------------------------------------------------------+

// DFU Protocol
typedef enum
{
  DFU_PROTOCOL_RT  = 1,
  DFU_PROTOCOL_DFU = 2,
} dfu_protocol_type_t;

// DFU Descriptor Type
typedef enum
{
  DFU_DESC_FUNCTIONAL = 0x21,
} dfu_descriptor_type_t;


//--------------------------------------------------------------------+
// Application Callback API (weak is optional)
//--------------------------------------------------------------------+

// Invoked when received new data
TU_ATTR_WEAK void tud_dfu_rt_reboot_to_dfu(void); // TODO rename to _cb convention

//--------------------------------------------------------------------+
// Internal Class Driver API
//--------------------------------------------------------------------+
void     dfu_rtd_init(void);
void     dfu_rtd_reset(uint8_t rhport);
uint16_t dfu_rtd_open(uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t max_len);
bool     dfu_rtd_control_request(uint8_t rhport, tusb_control_request_t const * request);
bool     dfu_rtd_control_complete(uint8_t rhport, tusb_control_request_t const * request);
bool     dfu_rtd_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t event, uint32_t xferred_bytes);

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_DFU_RT_DEVICE_H_ */
