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

#ifndef TUSB_DCD_H_
#define TUSB_DCD_H_

#include "common/tusb_common.h"
#include "osal/osal.h"
#include "common/tusb_fifo.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

typedef enum {
  DCD_EVENT_INVALID = 0,
  DCD_EVENT_BUS_RESET,
  DCD_EVENT_UNPLUGGED,
  DCD_EVENT_SOF,
  DCD_EVENT_SUSPEND, // TODO LPM Sleep L1 support
  DCD_EVENT_RESUME,

  DCD_EVENT_SETUP_RECEIVED,
  DCD_EVENT_XFER_COMPLETE,

  // Not an DCD event, just a convenient way to defer ISR function
  USBD_EVENT_FUNC_CALL,

  DCD_EVENT_COUNT
} dcd_eventid_t;

typedef struct TU_ATTR_ALIGNED(4) {
  uint8_t rhport;
  uint8_t event_id;

  union {
    // BUS RESET
    struct {
      tusb_speed_t speed;
    } bus_reset;

    // SOF
    struct {
      uint32_t frame_count;
    }sof;

    // SETUP_RECEIVED
    tusb_control_request_t setup_received;

    // XFER_COMPLETE
    struct {
      uint8_t  ep_addr;
      uint8_t  result;
      uint32_t len;
    }xfer_complete;

    // FUNC_CALL
    struct {
      void (*func) (void*);
      void* param;
    }func_call;
  };
} dcd_event_t;

typedef enum {
  TEST_J = 1,
  TEST_K,
  TEST_SE0_NAK,
  TEST_PACKET,
  TEST_FORCE_ENABLE,
} test_mode_t;

//TU_VERIFY_STATIC(sizeof(dcd_event_t) <= 12, "size is not correct");

//--------------------------------------------------------------------+
// Memory API
//--------------------------------------------------------------------+

// clean/flush data cache: write cache -> memory.
// Required before an DMA TX transfer to make sure data is in memory
void dcd_dcache_clean(void const* addr, uint32_t data_size) TU_ATTR_WEAK;

// invalidate data cache: mark cache as invalid, next read will read from memory
// Required BOTH before and after an DMA RX transfer
void dcd_dcache_invalidate(void const* addr, uint32_t data_size) TU_ATTR_WEAK;

// clean and invalidate data cache
// Required before an DMA transfer where memory is both read/write by DMA
void dcd_dcache_clean_invalidate(void const* addr, uint32_t data_size) TU_ATTR_WEAK;

//--------------------------------------------------------------------+
// Controller API
//--------------------------------------------------------------------+

// Initialize controller to device mode
void dcd_init(uint8_t rhport);

// Deinitialize controller, unset device mode.
bool dcd_deinit(uint8_t rhport);

// Interrupt Handler
void dcd_int_handler(uint8_t rhport);

// Enable device interrupt
void dcd_int_enable (uint8_t rhport);

// Disable device interrupt
void dcd_int_disable(uint8_t rhport);

// Receive Set Address request, mcu port must also include status IN response
void dcd_set_address(uint8_t rhport, uint8_t dev_addr);

// Wake up host
void dcd_remote_wakeup(uint8_t rhport);

// Connect by enabling internal pull-up resistor on D+/D-
void dcd_connect(uint8_t rhport);

// Disconnect by disabling internal pull-up resistor on D+/D-
void dcd_disconnect(uint8_t rhport);

// Enable/Disable Start-of-frame interrupt. Default is disabled
void dcd_sof_enable(uint8_t rhport, bool en);

#if CFG_TUD_TEST_MODE
// Check if the test mode is supported, returns true is test mode selector is supported
bool dcd_check_test_mode_support(test_mode_t test_selector) TU_ATTR_WEAK;

// Put device into a test mode (needs power cycle to quit)
void dcd_enter_test_mode(uint8_t rhport, test_mode_t test_selector) TU_ATTR_WEAK;
#endif
//--------------------------------------------------------------------+
// Endpoint API
//--------------------------------------------------------------------+

// Invoked when a control transfer's status stage is complete.
// May help DCD to prepare for next control transfer, this API is optional.
void dcd_edpt0_status_complete(uint8_t rhport, tusb_control_request_t const * request);

// Configure endpoint's registers according to descriptor
bool dcd_edpt_open            (uint8_t rhport, tusb_desc_endpoint_t const * desc_ep);

// Close all non-control endpoints, cancel all pending transfers if any.
// Invoked when switching from a non-zero Configuration by SET_CONFIGURE therefore
// required for multiple configuration support.
void dcd_edpt_close_all       (uint8_t rhport);

// Close an endpoint.
// Since it is weak, caller must TU_ASSERT this function's existence before calling it.
void dcd_edpt_close           (uint8_t rhport, uint8_t ep_addr) TU_ATTR_WEAK;

// Submit a transfer, When complete dcd_event_xfer_complete() is invoked to notify the stack
bool dcd_edpt_xfer            (uint8_t rhport, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes);

// Submit an transfer using fifo, When complete dcd_event_xfer_complete() is invoked to notify the stack
// This API is optional, may be useful for register-based for transferring data.
bool dcd_edpt_xfer_fifo       (uint8_t rhport, uint8_t ep_addr, tu_fifo_t * ff, uint16_t total_bytes) TU_ATTR_WEAK;

// Stall endpoint, any queuing transfer should be removed from endpoint
void dcd_edpt_stall           (uint8_t rhport, uint8_t ep_addr);

// clear stall, data toggle is also reset to DATA0
// This API never calls with control endpoints, since it is auto cleared when receiving setup packet
void dcd_edpt_clear_stall     (uint8_t rhport, uint8_t ep_addr);

// Allocate packet buffer used by ISO endpoints
// Some MCU need manual packet buffer allocation, we allocate the largest size to avoid clustering
TU_ATTR_WEAK bool dcd_edpt_iso_alloc(uint8_t rhport, uint8_t ep_addr, uint16_t largest_packet_size);

// Configure and enable an ISO endpoint according to descriptor
TU_ATTR_WEAK bool dcd_edpt_iso_activate(uint8_t rhport, tusb_desc_endpoint_t const * p_endpoint_desc);

//--------------------------------------------------------------------+
// Event API (implemented by stack)
//--------------------------------------------------------------------+

// Called by DCD to notify device stack
extern void dcd_event_handler(dcd_event_t const * event, bool in_isr);

// helper to send bus signal event
TU_ATTR_ALWAYS_INLINE static inline void dcd_event_bus_signal (uint8_t rhport, dcd_eventid_t eid, bool in_isr) {
  dcd_event_t event = { .rhport = rhport, .event_id = eid };
  dcd_event_handler(&event, in_isr);
}

// helper to send bus reset event
TU_ATTR_ALWAYS_INLINE static inline  void dcd_event_bus_reset (uint8_t rhport, tusb_speed_t speed, bool in_isr) {
  dcd_event_t event = { .rhport = rhport, .event_id = DCD_EVENT_BUS_RESET };
  event.bus_reset.speed = speed;
  dcd_event_handler(&event, in_isr);
}

// helper to send setup received
TU_ATTR_ALWAYS_INLINE static inline void dcd_event_setup_received(uint8_t rhport, uint8_t const * setup, bool in_isr) {
  dcd_event_t event = { .rhport = rhport, .event_id = DCD_EVENT_SETUP_RECEIVED };
  memcpy(&event.setup_received, setup, sizeof(tusb_control_request_t));

  dcd_event_handler(&event, in_isr);
}

// helper to send transfer complete event
TU_ATTR_ALWAYS_INLINE static inline void dcd_event_xfer_complete (uint8_t rhport, uint8_t ep_addr, uint32_t xferred_bytes, uint8_t result, bool in_isr) {
  dcd_event_t event = { .rhport = rhport, .event_id = DCD_EVENT_XFER_COMPLETE };

  event.xfer_complete.ep_addr = ep_addr;
  event.xfer_complete.len     = xferred_bytes;
  event.xfer_complete.result  = result;

  dcd_event_handler(&event, in_isr);
}

TU_ATTR_ALWAYS_INLINE static inline void dcd_event_sof(uint8_t rhport, uint32_t frame_count, bool in_isr) {
  dcd_event_t event = { .rhport = rhport, .event_id = DCD_EVENT_SOF };
  event.sof.frame_count = frame_count;
  dcd_event_handler(&event, in_isr);
}

#ifdef __cplusplus
 }
#endif

#endif
