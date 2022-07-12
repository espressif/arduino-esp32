/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021, Ha Thach (tinyusb.org)
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

#ifndef _TUSB_DWC2_BCM_H_
#define _TUSB_DWC2_BCM_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include "broadcom/defines.h"
#include "broadcom/interrupts.h"
#include "broadcom/caches.h"

#define DWC2_EP_MAX         8

static const dwc2_controller_t _dwc2_controller[] =
{
  { .reg_base = USB_OTG_GLOBAL_BASE, .irqnum = USB_IRQn, .ep_count = DWC2_EP_MAX, .ep_fifo_size = 4096 }
};

#define dcache_clean(_addr, _size)              data_clean(_addr, _size)
#define dcache_invalidate(_addr, _size)         data_invalidate(_addr, _size)
#define dcache_clean_invalidate(_addr, _size)   data_clean_and_invalidate(_addr, _size)

TU_ATTR_ALWAYS_INLINE
static inline void dwc2_dcd_int_enable(uint8_t rhport)
{
  BP_EnableIRQ(_dwc2_controller[rhport].irqnum);
}

TU_ATTR_ALWAYS_INLINE
static inline void dwc2_dcd_int_disable (uint8_t rhport)
{
  BP_DisableIRQ(_dwc2_controller[rhport].irqnum);
}

static inline void dwc2_remote_wakeup_delay(void)
{
  // try to delay for 1 ms
  // TODO implement later
}

// MCU specific PHY init, called BEFORE core reset
static inline void dwc2_phy_init(dwc2_regs_t * dwc2, uint8_t hs_phy_type)
{
  (void) dwc2;
  (void) hs_phy_type;

  // nothing to do
}

// MCU specific PHY update, it is called AFTER init() and core reset
static inline void dwc2_phy_update(dwc2_regs_t * dwc2, uint8_t hs_phy_type)
{
  (void) dwc2;
  (void) hs_phy_type;

  // nothing to do
}

#ifdef __cplusplus
 }
#endif

#endif
