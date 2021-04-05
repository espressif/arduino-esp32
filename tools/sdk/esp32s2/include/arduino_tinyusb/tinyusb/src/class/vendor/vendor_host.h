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

/** \ingroup group_class
 *  \defgroup Group_Custom Custom Class (not supported yet)
 *  @{ */

#ifndef _TUSB_VENDOR_HOST_H_
#define _TUSB_VENDOR_HOST_H_

#include "common/tusb_common.h"
#include "host/usbh.h"

#ifdef __cplusplus
 extern "C" {
#endif

typedef struct {
  pipe_handle_t pipe_in;
  pipe_handle_t pipe_out;
}custom_interface_info_t;

//--------------------------------------------------------------------+
// USBH-CLASS DRIVER API
//--------------------------------------------------------------------+
static inline bool tusbh_custom_is_mounted(uint8_t dev_addr, uint16_t vendor_id, uint16_t product_id)
{
  (void) vendor_id; // TODO check this later
  (void) product_id;
//  return (tusbh_device_get_mounted_class_flag(dev_addr) & TU_BIT(TUSB_CLASS_MAPPED_INDEX_END-1) ) != 0;
  return false;
}

tusb_error_t tusbh_custom_read(uint8_t dev_addr, uint16_t vendor_id, uint16_t product_id, void * p_buffer, uint16_t length);
tusb_error_t tusbh_custom_write(uint8_t dev_addr, uint16_t vendor_id, uint16_t product_id, void const * p_data, uint16_t length);

//--------------------------------------------------------------------+
// Internal Class Driver API
//--------------------------------------------------------------------+
void         cush_init(void);
tusb_error_t cush_open_subtask(uint8_t dev_addr, tusb_desc_interface_t const *p_interface_desc, uint16_t *p_length);
void         cush_isr(pipe_handle_t pipe_hdl, xfer_result_t event);
void         cush_close(uint8_t dev_addr);

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_VENDOR_HOST_H_ */

/** @} */
