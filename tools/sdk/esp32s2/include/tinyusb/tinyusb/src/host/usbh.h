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

/** \ingroup group_usbh USB Host Core (USBH)
 *  @{ */

#ifndef _TUSB_USBH_H_
#define _TUSB_USBH_H_

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "osal/osal.h" // TODO refractor move to common.h ?
#include "hcd.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
typedef enum tusb_interface_status_{
  TUSB_INTERFACE_STATUS_READY = 0,
  TUSB_INTERFACE_STATUS_BUSY,
  TUSB_INTERFACE_STATUS_COMPLETE,
  TUSB_INTERFACE_STATUS_ERROR,
  TUSB_INTERFACE_STATUS_INVALID_PARA
} tusb_interface_status_t;

typedef struct {
  uint8_t class_code;

  void (* const init) (void);
  bool (* const open)(uint8_t rhport, uint8_t dev_addr, tusb_desc_interface_t const * itf_desc, uint16_t* outlen);
  void (* const isr) (uint8_t dev_addr, uint8_t ep_addr, xfer_result_t result, uint32_t len);
  void (* const close) (uint8_t);
} host_class_driver_t;
//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// APPLICATION API
//--------------------------------------------------------------------+
void tuh_task(void);

// Interrupt handler, name alias to HCD
#define tuh_isr   hcd_isr

tusb_device_state_t tuh_device_get_state (uint8_t dev_addr);
static inline bool tuh_device_is_configured(uint8_t dev_addr)
{
  return tuh_device_get_state(dev_addr) == TUSB_DEVICE_STATE_CONFIGURED;
}

//--------------------------------------------------------------------+
// APPLICATION CALLBACK
//--------------------------------------------------------------------+
TU_ATTR_WEAK uint8_t tuh_device_attached_cb (tusb_desc_device_t const *p_desc_device);

/** Callback invoked when device is mounted (configured) */
TU_ATTR_WEAK void tuh_mount_cb (uint8_t dev_addr);

/** Callback invoked when device is unmounted (bus reset/unplugged) */
TU_ATTR_WEAK void tuh_umount_cb(uint8_t dev_addr);

//--------------------------------------------------------------------+
// CLASS-USBH & INTERNAL API
//--------------------------------------------------------------------+
bool usbh_init(void);
bool usbh_control_xfer (uint8_t dev_addr, tusb_control_request_t* request, uint8_t* data);

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_USBH_H_ */

/** @} */
