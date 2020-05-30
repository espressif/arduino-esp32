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

#ifndef _TUSB_VENDOR_DEVICE_H_
#define _TUSB_VENDOR_DEVICE_H_

#include "common/tusb_common.h"
#include "device/usbd.h"

#ifndef CFG_TUD_VENDOR_EPSIZE
#define CFG_TUD_VENDOR_EPSIZE     64
#endif

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// Application API (Multiple Interfaces)
//--------------------------------------------------------------------+
bool     tud_vendor_n_mounted         (uint8_t itf);

uint32_t tud_vendor_n_available       (uint8_t itf);
uint32_t tud_vendor_n_read            (uint8_t itf, void* buffer, uint32_t bufsize);
bool     tud_vendor_n_peek            (uint8_t itf, int pos, uint8_t* u8);

uint32_t tud_vendor_n_write           (uint8_t itf, void const* buffer, uint32_t bufsize);
uint32_t tud_vendor_n_write_available (uint8_t itf);

static inline
uint32_t tud_vendor_n_write_str       (uint8_t itf, char const* str);

//--------------------------------------------------------------------+
// Application API (Single Port)
//--------------------------------------------------------------------+
static inline bool     tud_vendor_mounted         (void);
static inline uint32_t tud_vendor_available       (void);
static inline uint32_t tud_vendor_read            (void* buffer, uint32_t bufsize);
static inline bool     tud_vendor_peek            (int pos, uint8_t* u8);
static inline uint32_t tud_vendor_write           (void const* buffer, uint32_t bufsize);
static inline uint32_t tud_vendor_write_str       (char const* str);
static inline uint32_t tud_vendor_write_available (void);

//--------------------------------------------------------------------+
// Application Callback API (weak is optional)
//--------------------------------------------------------------------+

// Invoked when received new data
TU_ATTR_WEAK void tud_vendor_rx_cb(uint8_t itf);

//--------------------------------------------------------------------+
// Inline Functions
//--------------------------------------------------------------------+

static inline uint32_t tud_vendor_n_write_str (uint8_t itf, char const* str)
{
  return tud_vendor_n_write(itf, str, strlen(str));
}

static inline bool tud_vendor_mounted (void)
{
  return tud_vendor_n_mounted(0);
}

static inline uint32_t tud_vendor_available (void)
{
  return tud_vendor_n_available(0);
}

static inline uint32_t tud_vendor_read (void* buffer, uint32_t bufsize)
{
  return tud_vendor_n_read(0, buffer, bufsize);
}

static inline bool tud_vendor_peek (int pos, uint8_t* u8)
{
  return tud_vendor_n_peek(0, pos, u8);
}

static inline uint32_t tud_vendor_write (void const* buffer, uint32_t bufsize)
{
  return tud_vendor_n_write(0, buffer, bufsize);
}

static inline uint32_t tud_vendor_write_str (char const* str)
{
  return tud_vendor_n_write_str(0, str);
}

static inline uint32_t tud_vendor_write_available (void)
{
  return tud_vendor_n_write_available(0);
}

//--------------------------------------------------------------------+
// Internal Class Driver API
//--------------------------------------------------------------------+
void vendord_init(void);
void vendord_reset(uint8_t rhport);
bool vendord_open(uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t *p_length);
bool vendord_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t event, uint32_t xferred_bytes);

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_VENDOR_DEVICE_H_ */
