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

#ifndef _TUSB_DFU_DEVICE_H_
#define _TUSB_DFU_DEVICE_H_

#include "dfu.h"

#ifdef __cplusplus
  extern "C" {
#endif


//--------------------------------------------------------------------+
// Application Callback API (weak is optional)
//--------------------------------------------------------------------+
// Invoked during DFU_MANIFEST_SYNC get status request to check if firmware
// is valid
bool tud_dfu_firmware_valid_check_cb(void);

// Invoked when a DFU_DNLOAD request is received
// This callback takes the wBlockNum chunk of length length and provides it
// to the application at the data pointer.  This data is only valid for this
// call, so the app must use it not or copy it.
void tud_dfu_req_dnload_data_cb(uint16_t wBlockNum, uint8_t* data, uint16_t length);

// Must be called when the application is done using the last block of data
// provided by tud_dfu_req_dnload_data_cb
void tud_dfu_dnload_complete(void);

// Invoked during the last DFU_DNLOAD request, signifying that the host believes
// it is done transmitting data.
// Return true if the application agrees there is no more data
// Return false if the device disagrees, which will stall the pipe, and the Host
//              should initiate a recovery procedure
bool tud_dfu_device_data_done_check_cb(void);

// Invoked when the Host has terminated a download or upload transfer
TU_ATTR_WEAK void tud_dfu_abort_cb(void);

// Invoked when a DFU_UPLOAD request is received
// This callback must populate data with up to length bytes
// Return the number of bytes to write
uint16_t tud_dfu_req_upload_data_cb(uint16_t block_num, uint8_t* data, uint16_t length);

//--------------------------------------------------------------------+
// Internal Class Driver API
//--------------------------------------------------------------------+
void     dfu_moded_init(void);
void     dfu_moded_reset(uint8_t rhport);
uint16_t dfu_moded_open(uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t max_len);
bool     dfu_moded_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request);


#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_DFU_MODE_DEVICE_H_ */
