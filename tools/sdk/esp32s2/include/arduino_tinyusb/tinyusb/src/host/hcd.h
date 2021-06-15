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

#ifndef _TUSB_HCD_H_
#define _TUSB_HCD_H_

#include "common/tusb_common.h"
#include "osal/osal.h"
#include "common/tusb_fifo.h"

#ifdef __cplusplus
 extern "C" {
#endif

 //--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
typedef enum
{
  HCD_EVENT_DEVICE_ATTACH,
  HCD_EVENT_DEVICE_REMOVE,
  HCD_EVENT_XFER_COMPLETE,

  // Not an HCD event, just a convenient way to defer ISR function
  USBH_EVENT_FUNC_CALL,

  HCD_EVENT_COUNT
} hcd_eventid_t;

typedef struct
{
  uint8_t rhport;
  uint8_t event_id;
  uint8_t dev_addr;

  union
  {
    // Attach, Remove
    struct {
      uint8_t hub_addr;
      uint8_t hub_port;
    } connection;

    // XFER_COMPLETE
    struct {
      uint8_t ep_addr;
      uint8_t result;
      uint32_t len;
    } xfer_complete;

    // FUNC_CALL
    struct {
      void (*func) (void*);
      void* param;
    }func_call;
  };

} hcd_event_t;

#if TUSB_OPT_HOST_ENABLED
// Max number of endpoints per device
enum {
  // TODO better computation
  HCD_MAX_ENDPOINT = CFG_TUSB_HOST_DEVICE_MAX*(CFG_TUH_HUB + CFG_TUH_HID*2 + CFG_TUH_MSC*2 + CFG_TUH_CDC*3),
  HCD_MAX_XFER     = HCD_MAX_ENDPOINT*2,
};

//#define HCD_MAX_ENDPOINT 16
//#define HCD_MAX_XFER 16
#endif

//--------------------------------------------------------------------+
// Controller API
//--------------------------------------------------------------------+

// Initialize controller to host mode
bool hcd_init(uint8_t rhport);

// Interrupt Handler
void hcd_int_handler(uint8_t rhport);

// Enable USB interrupt
void hcd_int_enable (uint8_t rhport);

// Disable USB interrupt
void hcd_int_disable(uint8_t rhport);

// Get frame number (1ms)
uint32_t hcd_frame_number(uint8_t rhport);

//--------------------------------------------------------------------+
// Port API
//--------------------------------------------------------------------+

// Get the current connect status of roothub port
bool hcd_port_connect_status(uint8_t rhport);

// Reset USB bus on the port
void hcd_port_reset(uint8_t rhport);

// TODO implement later
void hcd_port_reset_end(uint8_t rhport);

// Get port link speed
tusb_speed_t hcd_port_speed_get(uint8_t rhport);

// HCD closes all opened endpoints belong to this device
void hcd_device_close(uint8_t rhport, uint8_t dev_addr);

//--------------------------------------------------------------------+
// Endpoints API
//--------------------------------------------------------------------+

bool hcd_setup_send(uint8_t rhport, uint8_t dev_addr, uint8_t const setup_packet[8]);
bool hcd_edpt_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_endpoint_t const * ep_desc);
bool hcd_edpt_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr, uint8_t * buffer, uint16_t buflen);

bool hcd_edpt_busy(uint8_t dev_addr, uint8_t ep_addr);
bool hcd_edpt_stalled(uint8_t dev_addr, uint8_t ep_addr);
bool hcd_edpt_clear_stall(uint8_t dev_addr, uint8_t ep_addr);

//--------------------------------------------------------------------+
// Event API (implemented by stack)
//--------------------------------------------------------------------+

// Called by HCD to notify stack
extern void hcd_event_handler(hcd_event_t const* event, bool in_isr);

// Helper to send device attach event
extern void hcd_event_device_attach(uint8_t rhport, bool in_isr);

// Helper to send device removal event
extern void hcd_event_device_remove(uint8_t rhport, bool in_isr);

// Helper to send USB transfer event
extern void hcd_event_xfer_complete(uint8_t dev_addr, uint8_t ep_addr, uint32_t xferred_bytes, xfer_result_t result, bool in_isr);

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_HCD_H_ */
