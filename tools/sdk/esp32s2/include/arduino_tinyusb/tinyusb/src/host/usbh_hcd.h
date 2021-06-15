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

#ifndef CFG_TUH_EP_MAX
#define CFG_TUH_EP_MAX          9
#endif

//--------------------------------------------------------------------+
// USBH-HCD common data structure
//--------------------------------------------------------------------+

// TODO move to usbh.c
typedef struct {
  //------------- port -------------//
  uint8_t rhport;
  uint8_t hub_addr;
  uint8_t hub_port;
  uint8_t speed;

  //------------- device descriptor -------------//
  uint16_t vendor_id;
  uint16_t product_id;
  uint8_t  ep0_packet_size;

  //------------- configuration descriptor -------------//
  // uint8_t interface_count; // bNumInterfaces alias

  //------------- device -------------//
  struct TU_ATTR_PACKED
  {
    uint8_t connected    : 1;
    uint8_t addressed    : 1;
    uint8_t configured   : 1;
    uint8_t suspended    : 1;
  };

  volatile uint8_t state;             // device state, value from enum tusbh_device_state_t

  uint8_t itf2drv[16];  // map interface number to driver (0xff is invalid)
  uint8_t ep2drv[CFG_TUH_EP_MAX][2]; // map endpoint to driver ( 0xff is invalid )

  struct TU_ATTR_PACKED
  {
    volatile bool busy    : 1;
    volatile bool stalled : 1;
    volatile bool claimed : 1;

    // TODO merge ep2drv here, 4-bit should be sufficient
  }ep_status[CFG_TUH_EP_MAX][2];

  // Mutex for claiming endpoint, only needed when using with preempted RTOS
#if CFG_TUSB_OS != OPT_OS_NONE
  osal_mutex_def_t mutexdef;
  osal_mutex_t mutex;
#endif

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
