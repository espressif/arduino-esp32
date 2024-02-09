/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 Ha Thach (thach@tinyusb.org) for Adafruit Industries
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

#ifndef _TUSB_TCD_H_
#define _TUSB_TCD_H_

#include "common/tusb_common.h"
#include "pd_types.h"

#include "osal/osal.h"
#include "common/tusb_fifo.h"

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

enum {
  TCD_EVENT_INVALID = 0,
  TCD_EVENT_CC_CHANGED,
  TCD_EVENT_RX_COMPLETE,
  TCD_EVENT_TX_COMPLETE,
};

typedef struct TU_ATTR_PACKED {
  uint8_t rhport;
  uint8_t event_id;

  union {
    struct {
      uint8_t cc_state[2];
    } cc_changed;

    struct TU_ATTR_PACKED {
      uint16_t result : 2;
      uint16_t xferred_bytes : 14;
    } xfer_complete;
  };

} tcd_event_t;;

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

// Initialize controller
bool tcd_init(uint8_t rhport, uint32_t port_type);

// Enable interrupt
void tcd_int_enable (uint8_t rhport);

// Disable interrupt
void tcd_int_disable(uint8_t rhport);

// Interrupt Handler
void tcd_int_handler(uint8_t rhport);

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

bool tcd_msg_receive(uint8_t rhport, uint8_t* buffer, uint16_t total_bytes);
bool tcd_msg_send(uint8_t rhport, uint8_t const* buffer, uint16_t total_bytes);

//--------------------------------------------------------------------+
// Event API (implemented by stack)
// Called by TCD to notify stack
//--------------------------------------------------------------------+

extern void tcd_event_handler(tcd_event_t const * event, bool in_isr);

TU_ATTR_ALWAYS_INLINE static inline
void tcd_event_cc_changed(uint8_t rhport, uint8_t cc1, uint8_t cc2, bool in_isr) {
  tcd_event_t event = {
    .rhport   = rhport,
    .event_id = TCD_EVENT_CC_CHANGED,
    .cc_changed = {
        .cc_state = {cc1, cc2 }
    }
  };

  tcd_event_handler(&event, in_isr);
}

TU_ATTR_ALWAYS_INLINE static inline
void tcd_event_rx_complete(uint8_t rhport, uint16_t xferred_bytes, uint8_t result, bool in_isr) {
  tcd_event_t event = {
    .rhport   = rhport,
    .event_id = TCD_EVENT_RX_COMPLETE,
    .xfer_complete = {
        .xferred_bytes = xferred_bytes,
        .result        = result
    }
  };

  tcd_event_handler(&event, in_isr);
}

TU_ATTR_ALWAYS_INLINE static inline
void tcd_event_tx_complete(uint8_t rhport, uint16_t xferred_bytes, uint8_t result, bool in_isr) {
  tcd_event_t event = {
      .rhport   = rhport,
      .event_id = TCD_EVENT_TX_COMPLETE,
      .xfer_complete = {
          .xferred_bytes = xferred_bytes,
          .result        = result
      }
  };

  tcd_event_handler(&event, in_isr);
}

#ifdef __cplusplus
}
#endif

#endif
