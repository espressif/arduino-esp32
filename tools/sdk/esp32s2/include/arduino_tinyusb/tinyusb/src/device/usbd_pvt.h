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
#ifndef USBD_PVT_H_
#define USBD_PVT_H_

#include "osal/osal.h"
#include "common/tusb_fifo.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// Class Drivers
//--------------------------------------------------------------------+

typedef struct
{
  #if CFG_TUSB_DEBUG >= 2
  char const* name;
  #endif

  void     (* init             ) (void);
  void     (* reset            ) (uint8_t rhport);
  uint16_t (* open             ) (uint8_t rhport, tusb_desc_interface_t const * desc_intf, uint16_t max_len);
  bool     (* control_xfer_cb  ) (uint8_t rhport, uint8_t stage, tusb_control_request_t const * request);
  bool     (* xfer_cb          ) (uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes);
  void     (* sof              ) (uint8_t rhport); /* optional */
} usbd_class_driver_t;

// Invoked when initializing device stack to get additional class drivers.
// Can optionally implemented by application to extend/overwrite class driver support.
// Note: The drivers array must be accessible at all time when stack is active
usbd_class_driver_t const* usbd_app_driver_get_cb(uint8_t* driver_count) TU_ATTR_WEAK;


typedef bool (*usbd_control_xfer_cb_t)(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request);

//--------------------------------------------------------------------+
// USBD Endpoint API
//--------------------------------------------------------------------+

// Open an endpoint
bool usbd_edpt_open(uint8_t rhport, tusb_desc_endpoint_t const * desc_ep);

// Close an endpoint
void usbd_edpt_close(uint8_t rhport, uint8_t ep_addr);

// Submit a usb transfer
bool usbd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes);

// Submit a usb ISO transfer by use of a FIFO (ring buffer) - all bytes in FIFO get transmitted
bool usbd_edpt_xfer_fifo(uint8_t rhport, uint8_t ep_addr, tu_fifo_t * ff, uint16_t total_bytes);

// Claim an endpoint before submitting a transfer.
// If caller does not make any transfer, it must release endpoint for others.
bool usbd_edpt_claim(uint8_t rhport, uint8_t ep_addr);

// Release an endpoint without submitting a transfer
bool usbd_edpt_release(uint8_t rhport, uint8_t ep_addr);

// Check if endpoint transferring is complete
bool usbd_edpt_busy(uint8_t rhport, uint8_t ep_addr);

// Stall endpoint
void usbd_edpt_stall(uint8_t rhport, uint8_t ep_addr);

// Clear stalled endpoint
void usbd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr);

// Check if endpoint is stalled
bool usbd_edpt_stalled(uint8_t rhport, uint8_t ep_addr);

TU_ATTR_ALWAYS_INLINE static inline
bool usbd_edpt_ready(uint8_t rhport, uint8_t ep_addr)
{
  return !usbd_edpt_busy(rhport, ep_addr) && !usbd_edpt_stalled(rhport, ep_addr);
}

/*------------------------------------------------------------------*/
/* Helper
 *------------------------------------------------------------------*/

bool usbd_open_edpt_pair(uint8_t rhport, uint8_t const* p_desc, uint8_t ep_count, uint8_t xfer_type, uint8_t* ep_out, uint8_t* ep_in);
void usbd_defer_func( osal_task_func_t func, void* param, bool in_isr );


#ifdef __cplusplus
 }
#endif

#endif /* USBD_PVT_H_ */
