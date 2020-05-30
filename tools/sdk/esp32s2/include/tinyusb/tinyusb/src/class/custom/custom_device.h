/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
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

#ifndef _TUSB_CUSTOM_DEVICE_H_
#define _TUSB_CUSTOM_DEVICE_H_

#include "common/tusb_common.h"
#include "device/usbd.h"

//--------------------------------------------------------------------+
// APPLICATION API (Multiple Root Ports)
// Should be used only with MCU that support more than 1 ports
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// APPLICATION API (Single Port)
// Should be used with MCU supporting only 1 USB port for code simplicity
//--------------------------------------------------------------------+


//--------------------------------------------------------------------+
// APPLICATION CALLBACK API (WEAK is optional)
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// Internal Class Driver API
//--------------------------------------------------------------------+
void cusd_init(void);
bool cusd_open(uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t *p_length);
bool cusd_control_request_st(uint8_t rhport, tusb_control_request_t const * p_request);
bool cusd_control_request_complete (uint8_t rhport, tusb_control_request_t const * p_request);
bool cusd_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t event, uint32_t xferred_bytes);
void cusd_reset(uint8_t rhport);

#endif /* _TUSB_CUSTOM_DEVICE_H_ */
