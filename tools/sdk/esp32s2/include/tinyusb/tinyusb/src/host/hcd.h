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

/** \ingroup group_usbh
 * \defgroup Group_HCD Host Controller Driver (HCD)
 *  @{ */

#ifndef _TUSB_HCD_H_
#define _TUSB_HCD_H_

#include <common/tusb_common.h>

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
} hcd_eventid_t;

typedef struct
{
  uint8_t rhport;
  uint8_t event_id;

  union
  {
    struct
    {
      uint8_t hub_addr;
      uint8_t hub_port;
    } attach, remove;

    struct
    {
      uint8_t ep_addr;
      uint8_t result;
      uint32_t len;
    } xfer_complete;
  };

} hcd_event_t;

#if TUSB_OPT_HOST_ENABLED
// Max number of endpoints per device
enum {
  HCD_MAX_ENDPOINT = CFG_TUSB_HOST_DEVICE_MAX*(CFG_TUH_HUB + CFG_TUH_HID_KEYBOARD + CFG_TUH_HID_MOUSE + CFG_TUSB_HOST_HID_GENERIC +
                     CFG_TUH_MSC*2 + CFG_TUH_CDC*3),

  HCD_MAX_XFER     = HCD_MAX_ENDPOINT*2,
};

//#define HCD_MAX_ENDPOINT 16
//#define HCD_MAX_XFER 16
#endif

//--------------------------------------------------------------------+
// Controller & Port API
//--------------------------------------------------------------------+
bool hcd_init(void);
void hcd_int_handler(uint8_t rhport);
void hcd_int_enable (uint8_t rhport);
void hcd_int_disable(uint8_t rhport);

// Get micro frame number (125 us)
uint32_t hcd_uframe_number(uint8_t rhport);

// Get frame number (1ms)
static inline uint32_t hcd_frame_number(uint8_t rhport)
{
  return hcd_uframe_number(rhport) >> 3;
}

/// return the current connect status of roothub port
bool hcd_port_connect_status(uint8_t hostid);
void hcd_port_reset(uint8_t hostid);
void hcd_port_reset_end(uint8_t rhport);
tusb_speed_t hcd_port_speed_get(uint8_t hostid);

// HCD closes all opened endpoints belong to this device
void hcd_device_close(uint8_t rhport, uint8_t dev_addr);

//--------------------------------------------------------------------+
// Event function
//--------------------------------------------------------------------+
void hcd_event_handler(hcd_event_t const* event, bool in_isr);

// Helper to send device attach event
void hcd_event_device_attach(uint8_t rhport);

// Helper to send device removal event
void hcd_event_device_remove(uint8_t rhport);

// Helper to send USB transfer event
void hcd_event_xfer_complete(uint8_t dev_addr, uint8_t ep_addr, xfer_result_t event, uint32_t xferred_bytes);

//--------------------------------------------------------------------+
// Endpoints API
//--------------------------------------------------------------------+
bool hcd_setup_send(uint8_t rhport, uint8_t dev_addr, uint8_t const setup_packet[8]);
bool hcd_edpt_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_endpoint_t const * ep_desc);

bool hcd_edpt_busy(uint8_t dev_addr, uint8_t ep_addr);
bool hcd_edpt_stalled(uint8_t dev_addr, uint8_t ep_addr);
bool hcd_edpt_clear_stall(uint8_t dev_addr, uint8_t ep_addr);

// TODO merge with pipe_xfer
bool hcd_edpt_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr, uint8_t * buffer, uint16_t buflen);

//--------------------------------------------------------------------+
// PIPE API
//--------------------------------------------------------------------+
// TODO control xfer should be used via usbh layer
bool hcd_pipe_queue_xfer(uint8_t dev_addr, uint8_t ep_addr, uint8_t buffer[], uint16_t total_bytes); // only queue, not transferring yet
bool hcd_pipe_xfer(uint8_t dev_addr, uint8_t ep_addr, uint8_t buffer[], uint16_t total_bytes, bool int_on_complete);

// tusb_error_t hcd_pipe_cancel();

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_HCD_H_ */

/// @}
