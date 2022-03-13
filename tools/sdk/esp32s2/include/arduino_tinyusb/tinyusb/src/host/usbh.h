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

#ifndef _TUSB_USBH_H_
#define _TUSB_USBH_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include "common/tusb_common.h"
#include "hcd.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

typedef bool (*tuh_control_complete_cb_t)(uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result);

//--------------------------------------------------------------------+
// APPLICATION API
//--------------------------------------------------------------------+

// Init host stack
bool tuh_init(uint8_t rhport);

// Check if host stack is already initialized
bool tuh_inited(void);

// Task function should be called in main/rtos loop
void tuh_task(void);

// Interrupt handler, name alias to HCD
extern void hcd_int_handler(uint8_t rhport);
#define tuh_int_handler   hcd_int_handler

bool tuh_vid_pid_get(uint8_t dev_addr, uint16_t* vid, uint16_t* pid);
tusb_speed_t tuh_speed_get(uint8_t dev_addr);

// Check if device is connected and configured
bool tuh_mounted(uint8_t dev_addr);

// Check if device is suspended
static inline bool tuh_suspended(uint8_t dev_addr)
{
  // TODO implement suspend & resume on host
  (void) dev_addr;
  return false;
}

// Check if device is ready to communicate with
TU_ATTR_ALWAYS_INLINE
static inline bool tuh_ready(uint8_t dev_addr)
{
  return tuh_mounted(dev_addr) && !tuh_suspended(dev_addr);
}

// Carry out control transfer
bool tuh_control_xfer (uint8_t dev_addr, tusb_control_request_t const* request, void* buffer, tuh_control_complete_cb_t complete_cb);

//--------------------------------------------------------------------+
// APPLICATION CALLBACK
//--------------------------------------------------------------------+
//TU_ATTR_WEAK uint8_t tuh_attach_cb (tusb_desc_device_t const *desc_device);

// Invoked when device is mounted (configured)
TU_ATTR_WEAK void tuh_mount_cb (uint8_t dev_addr);

/// Invoked when device is unmounted (bus reset/unplugged)
TU_ATTR_WEAK void tuh_umount_cb(uint8_t dev_addr);

#ifdef __cplusplus
 }
#endif

#endif
