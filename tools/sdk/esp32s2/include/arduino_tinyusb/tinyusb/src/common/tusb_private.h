/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2022, Ha Thach (tinyusb.org)
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


#ifndef _TUSB_PRIVATE_H_
#define _TUSB_PRIVATE_H_

#ifdef __cplusplus
 extern "C" {
#endif

typedef struct TU_ATTR_PACKED
{
  volatile uint8_t busy    : 1;
  volatile uint8_t stalled : 1;
  volatile uint8_t claimed : 1;
}tu_edpt_state_t;

//--------------------------------------------------------------------+
// Internal Helper used by Host and Device Stack
//--------------------------------------------------------------------+

// Check if endpoint descriptor is valid per USB specs
bool tu_edpt_validate(tusb_desc_endpoint_t const * desc_ep, tusb_speed_t speed);

// Bind all endpoint of a interface descriptor to class driver
void tu_edpt_bind_driver(uint8_t ep2drv[][2], tusb_desc_interface_t const* p_desc, uint16_t desc_len, uint8_t driver_id);

// Calculate total length of n interfaces (depending on IAD)
uint16_t tu_desc_get_interface_total_len(tusb_desc_interface_t const* desc_itf, uint8_t itf_count, uint16_t max_len);

// Claim an endpoint with provided mutex
bool tu_edpt_claim(tu_edpt_state_t* ep_state, osal_mutex_t mutex);

// Release an endpoint with provided mutex
bool tu_edpt_release(tu_edpt_state_t* ep_state, osal_mutex_t mutex);

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_PRIVATE_H_ */
