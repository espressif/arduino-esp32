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


#ifndef _DWC2_ESP32_H_
#define _DWC2_ESP32_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include "esp_intr_alloc.h"
#include "soc/periph_defs.h"
//#include "soc/usb_periph.h"

#define DWC2_REG_BASE       0x60080000UL
#define DWC2_EP_MAX         6             // USB_OUT_EP_NUM. TODO ESP32Sx only has 5 tx fifo (5 endpoint IN)

static const dwc2_controller_t _dwc2_controller[] =
{
  { .reg_base = DWC2_REG_BASE, .irqnum = 0, .ep_count = DWC2_EP_MAX, .ep_fifo_size = 1024 }
};

static intr_handle_t usb_ih;

static void dcd_int_handler_wrap(void* arg)
{
  (void) arg;
  dcd_int_handler(0);
}

TU_ATTR_ALWAYS_INLINE
static inline void dwc2_dcd_int_enable (uint8_t rhport)
{
  (void) rhport;
  esp_intr_alloc(ETS_USB_INTR_SOURCE, ESP_INTR_FLAG_LOWMED, dcd_int_handler_wrap, NULL, &usb_ih);
}

TU_ATTR_ALWAYS_INLINE
static inline void dwc2_dcd_int_disable (uint8_t rhport)
{
  (void) rhport;
  esp_intr_free(usb_ih);
}

static inline void dwc2_remote_wakeup_delay(void)
{
  vTaskDelay(pdMS_TO_TICKS(1));
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

#endif /* _DWC2_ESP32_H_ */
