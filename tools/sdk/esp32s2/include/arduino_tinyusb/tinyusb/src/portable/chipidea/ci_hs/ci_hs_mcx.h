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

#ifndef _CI_HS_MCX_H_
#define _CI_HS_MCX_H_

#include "fsl_device_registers.h"

// NOTE: MCX N9 has 2 different USB Controller
// - USB0 is KHCI FullSpeed
// - USB1 is ChipIdea HighSpeed, therefore rhport = 1 is actually index 0

static const ci_hs_controller_t _ci_controller[] = {
    {.reg_base = USBHS1__USBC_BASE, .irqnum = USB1_HS_IRQn}
};

TU_ATTR_ALWAYS_INLINE static inline ci_hs_regs_t* CI_HS_REG(uint8_t port) {
  (void) port;
  return ((ci_hs_regs_t*) _ci_controller[0].reg_base);
}

#define CI_DCD_INT_ENABLE(_p)   do { (void) _p; NVIC_EnableIRQ (_ci_controller[0].irqnum); } while (0)
#define CI_DCD_INT_DISABLE(_p)  do { (void) _p; NVIC_DisableIRQ(_ci_controller[0].irqnum); } while (0)

#define CI_HCD_INT_ENABLE(_p)   NVIC_EnableIRQ (_ci_controller[_p].irqnum)
#define CI_HCD_INT_DISABLE(_p)  NVIC_DisableIRQ(_ci_controller[_p].irqnum)


#endif
