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


#ifndef CLASS_USBTMC_USBTMC_DEVICE_H_
#define CLASS_USBTMC_USBTMC_DEVICE_H_

#include "usbtmc.h"

// Enable 488 mode by default
#if !defined(CFG_TUD_USBTMC_ENABLE_488)
#define CFG_TUD_USBTMC_ENABLE_488 (1)
#endif

// USB spec says that full-speed must be 8,16,32, or 64.
// However, this driver implementation requires it to be >=32
#define USBTMCD_MAX_PACKET_SIZE (64u)

/***********************************************
 *  Functions to be implemeted by the class implementation
 */

// In order to proceed, app must call call tud_usbtmc_start_bus_read(rhport) during or soon after:
// * tud_usbtmc_open_cb
// * tud_usbtmc_msg_data_cb
// * tud_usbtmc_msgBulkIn_complete_cb
// * tud_usbtmc_msg_trigger_cb
// * (successful) tud_usbtmc_check_abort_bulk_out_cb
// * (successful) tud_usbtmc_check_abort_bulk_in_cb
// * (successful) tud_usmtmc_bulkOut_clearFeature_cb

#if (CFG_TUD_USBTMC_ENABLE_488)
usbtmc_response_capabilities_488_t const * tud_usbtmc_get_capabilities_cb(void);
#else
usbtmc_response_capabilities_t const * tud_usbtmc_get_capabilities_cb(void);
#endif

void tud_usbtmc_open_cb(uint8_t interface_id);

bool tud_usbtmc_msgBulkOut_start_cb(usbtmc_msg_request_dev_dep_out const * msgHeader);
// transfer_complete does not imply that a message is complete.
bool tud_usbtmc_msg_data_cb( void *data, size_t len, bool transfer_complete);
void tud_usbtmc_bulkOut_clearFeature_cb(void); // Notice to clear and abort the pending BULK out transfer

bool tud_usbtmc_msgBulkIn_request_cb(usbtmc_msg_request_dev_dep_in const * request);
bool tud_usbtmc_msgBulkIn_complete_cb(void);
void tud_usbtmc_bulkIn_clearFeature_cb(void); // Notice to clear and abort the pending BULK out transfer

bool tud_usbtmc_initiate_abort_bulk_in_cb(uint8_t *tmcResult);
bool tud_usbtmc_initiate_abort_bulk_out_cb(uint8_t *tmcResult);
bool tud_usbtmc_initiate_clear_cb(uint8_t *tmcResult);

bool tud_usbtmc_check_abort_bulk_in_cb(usbtmc_check_abort_bulk_rsp_t *rsp);
bool tud_usbtmc_check_abort_bulk_out_cb(usbtmc_check_abort_bulk_rsp_t *rsp);
bool tud_usbtmc_check_clear_cb(usbtmc_get_clear_status_rsp_t *rsp);

// Indicator pulse should be 0.5 to 1.0 seconds long
TU_ATTR_WEAK bool tud_usbtmc_indicator_pulse_cb(tusb_control_request_t const * msg, uint8_t *tmcResult);

#if (CFG_TUD_USBTMC_ENABLE_488)
uint8_t tud_usbtmc_get_stb_cb(uint8_t *tmcResult);
TU_ATTR_WEAK bool tud_usbtmc_msg_trigger_cb(usbtmc_msg_generic_t* msg);
//TU_ATTR_WEAK bool tud_usbtmc_app_go_to_local_cb();
#endif

/*******************************************
 * Called from app
 *
 * We keep a reference to the buffer, so it MUST not change until the app is
 * notified that the transfer is complete.
 ******************************************/

bool tud_usbtmc_transmit_dev_msg_data(
    const void * data, size_t len,
    bool endOfMessage, bool usingTermChar);

bool tud_usbtmc_start_bus_read(void);


/* "callbacks" from USB device core */

uint16_t usbtmcd_open_cb(uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t max_len);
void     usbtmcd_reset_cb(uint8_t rhport);
bool     usbtmcd_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes);
bool     usbtmcd_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request);
void     usbtmcd_init_cb(void);

/************************************************************
 * USBTMC Descriptor Templates
 *************************************************************/


#endif /* CLASS_USBTMC_USBTMC_DEVICE_H_ */
