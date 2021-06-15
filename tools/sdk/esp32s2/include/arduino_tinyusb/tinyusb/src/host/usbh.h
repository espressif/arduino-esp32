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

#include "common/tusb_common.h"
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
  #if CFG_TUSB_DEBUG >= 2
  char const* name;
  #endif

  uint8_t class_code;

  void (* const init       )(void);
  bool (* const open       )(uint8_t rhport, uint8_t dev_addr, tusb_desc_interface_t const * itf_desc, uint16_t* outlen);
  bool (* const set_config )(uint8_t dev_addr, uint8_t itf_num);
  bool (* const xfer_cb    )(uint8_t dev_addr, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes);
  void (* const close      )(uint8_t dev_addr);
} usbh_class_driver_t;

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

tusb_speed_t tuh_device_get_speed (uint8_t dev_addr);

// Check if device is configured
bool tuh_device_configured(uint8_t dev_addr);

// Check if device is ready to communicate with
TU_ATTR_ALWAYS_INLINE
static inline bool tuh_device_ready(uint8_t dev_addr)
{
  return tuh_device_configured(dev_addr);
}

// Carry out control transfer
bool tuh_control_xfer (uint8_t dev_addr, tusb_control_request_t const* request, void* buffer, tuh_control_complete_cb_t complete_cb);

//--------------------------------------------------------------------+
// APPLICATION CALLBACK
//--------------------------------------------------------------------+
//TU_ATTR_WEAK uint8_t tuh_attach_cb (tusb_desc_device_t const *desc_device);

/** Callback invoked when device is mounted (configured) */
TU_ATTR_WEAK void tuh_mount_cb (uint8_t dev_addr);

/** Callback invoked when device is unmounted (bus reset/unplugged) */
TU_ATTR_WEAK void tuh_umount_cb(uint8_t dev_addr);

//--------------------------------------------------------------------+
// CLASS-USBH & INTERNAL API
// TODO move to usbh_pvt.h
//--------------------------------------------------------------------+

bool usbh_edpt_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_endpoint_t const * ep_desc);
bool usbh_edpt_xfer(uint8_t dev_addr, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes);

// Claim an endpoint before submitting a transfer.
// If caller does not make any transfer, it must release endpoint for others.
bool usbh_edpt_claim(uint8_t dev_addr, uint8_t ep_addr);

void usbh_driver_set_config_complete(uint8_t dev_addr, uint8_t itf_num);

uint8_t usbh_get_rhport(uint8_t dev_addr);

uint8_t* usbh_get_enum_buf(void);

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_USBH_H_ */

/** @} */
