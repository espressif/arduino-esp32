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

/** \ingroup Group_HCD
 *  @{ */

#ifndef _TUSB_USBH_HCD_H_
#define _TUSB_USBH_HCD_H_

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "common/tusb_common.h"
#include "osal/osal.h"

//--------------------------------------------------------------------+
// USBH-HCD common data structure
//--------------------------------------------------------------------+
typedef struct {
  //------------- port -------------//
  uint8_t rhport;
  uint8_t hub_addr;
  uint8_t hub_port;
  uint8_t speed;

  //------------- device descriptor -------------//
  uint16_t vendor_id;
  uint16_t product_id;
  uint8_t  configure_count; // bNumConfigurations alias

  //------------- configuration descriptor -------------//
  uint8_t interface_count; // bNumInterfaces alias

  //------------- device -------------//
  volatile uint8_t state;             // device state, value from enum tusbh_device_state_t

  //------------- control pipe -------------//
  struct {
    volatile uint8_t pipe_status;
//    uint8_t xferred_bytes; TODO not yet necessary
    tusb_control_request_t request;

    osal_semaphore_def_t sem_def;
    osal_semaphore_t sem_hdl;  // used to synchronize with HCD when control xfer complete

    osal_mutex_def_t mutex_def;
    osal_mutex_t mutex_hdl;    // used to exclusively occupy control pipe
  } control;

  uint8_t itf2drv[16];  // map interface number to driver (0xff is invalid)
  uint8_t ep2drv[8][2]; // map endpoint to driver ( 0xff is invalid )
} usbh_device_t;

extern usbh_device_t _usbh_devices[CFG_TUSB_HOST_DEVICE_MAX+1]; // including zero-address

//--------------------------------------------------------------------+
// callback from HCD ISR
//--------------------------------------------------------------------+



#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_USBH_HCD_H_ */

/** @} */
