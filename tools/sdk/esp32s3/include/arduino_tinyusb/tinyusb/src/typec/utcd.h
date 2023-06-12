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

#ifndef _TUSB_UTCD_H_
#define _TUSB_UTCD_H_

#include "common/tusb_common.h"
#include "pd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------+
// TypeC Configuration
//--------------------------------------------------------------------+

#ifndef CFG_TUC_TASK_QUEUE_SZ
#define CFG_TUC_TASK_QUEUE_SZ   8
#endif

//--------------------------------------------------------------------+
// Application API
//--------------------------------------------------------------------+

// Init typec stack on a port
bool tuc_init(uint8_t rhport, uint32_t port_type);

// Check if typec port is initialized
bool tuc_inited(uint8_t rhport);

// Task function should be called in main/rtos loop, extended version of tud_task()
// - timeout_ms: millisecond to wait, zero = no wait, 0xFFFFFFFF = wait forever
// - in_isr: if function is called in ISR
void tuc_task_ext(uint32_t timeout_ms, bool in_isr);

// Task function should be called in main/rtos loop
TU_ATTR_ALWAYS_INLINE static inline
void tuc_task (void) {
  tuc_task_ext(UINT32_MAX, false);
}

#ifndef _TUSB_TCD_H_
extern void tcd_int_handler(uint8_t rhport);
#endif

// Interrupt handler, name alias to TCD
#define tuc_int_handler tcd_int_handler

//--------------------------------------------------------------------+
// Callbacks
//--------------------------------------------------------------------+

TU_ATTR_WEAK bool tuc_pd_data_received_cb(uint8_t rhport, pd_header_t const* header, uint8_t const* dobj, uint8_t const* p_end);
TU_ATTR_WEAK bool tuc_pd_control_received_cb(uint8_t rhport, pd_header_t const* header);

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

bool tuc_msg_request(uint8_t rhport, void const* rdo);


#ifdef __cplusplus
}
#endif

#endif
