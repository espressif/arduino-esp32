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

#ifndef _CI_HS_IMXRT_H_
#define _CI_HS_IMXRT_H_

#include "fsl_device_registers.h"

#if !defined(USB1_BASE) && defined(USB_OTG1_BASE)
#define USB1_BASE USB_OTG1_BASE
#endif

#if !defined(USB2_BASE) && defined(USB_OTG2_BASE)
#define USB2_BASE USB_OTG2_BASE
#endif

// RT1040 calls its only USB USB_OTG (no 1)
#if defined(MIMXRT1042_SERIES)
#define USB_OTG1_IRQn USB_OTG_IRQn
#endif

static const ci_hs_controller_t _ci_controller[] =
{
  // RT1010 and RT1020 only has 1 USB controller
  #if FSL_FEATURE_SOC_USBHS_COUNT == 1
    { .reg_base = USB_BASE , .irqnum = USB_OTG1_IRQn }
  #else
    { .reg_base = USB1_BASE, .irqnum = USB_OTG1_IRQn},
    { .reg_base = USB2_BASE, .irqnum = USB_OTG2_IRQn}
  #endif
};

#define CI_HS_REG(_port)        ((ci_hs_regs_t*) _ci_controller[_port].reg_base)

//------------- DCD -------------//
#define CI_DCD_INT_ENABLE(_p)   NVIC_EnableIRQ (_ci_controller[_p].irqnum)
#define CI_DCD_INT_DISABLE(_p)  NVIC_DisableIRQ(_ci_controller[_p].irqnum)

//------------- HCD -------------//
#define CI_HCD_INT_ENABLE(_p)   NVIC_EnableIRQ (_ci_controller[_p].irqnum)
#define CI_HCD_INT_DISABLE(_p)  NVIC_DisableIRQ(_ci_controller[_p].irqnum)

//------------- DCache -------------//
TU_ATTR_ALWAYS_INLINE static inline bool imxrt_is_cache_mem(uintptr_t addr) {
  return !(0x20000000 <= addr && addr < 0x20100000);
}

TU_ATTR_ALWAYS_INLINE static inline bool imxrt_dcache_clean(void const* addr, uint32_t data_size) {
  const uintptr_t addr32 = (uintptr_t) addr;
  if (imxrt_is_cache_mem(addr32)) {
    TU_ASSERT(tu_is_aligned32(addr32));
    SCB_CleanDCache_by_Addr((uint32_t *) addr32, (int32_t) data_size);
  }
  return true;
}

TU_ATTR_ALWAYS_INLINE static inline bool imxrt_dcache_invalidate(void const* addr, uint32_t data_size) {
  const uintptr_t addr32 = (uintptr_t) addr;
  if (imxrt_is_cache_mem(addr32)) {
    // Invalidating does not push cached changes back to RAM so we need to be
    // *very* careful when we do it. If we're not aligned, then we risk resetting
    // values back to their RAM state.
    TU_ASSERT(tu_is_aligned32(addr32));
    SCB_InvalidateDCache_by_Addr((void*) addr32, (int32_t) data_size);
  }
  return true;
}

TU_ATTR_ALWAYS_INLINE static inline bool imxrt_dcache_clean_invalidate(void const* addr, uint32_t data_size) {
  const uintptr_t addr32 = (uintptr_t) addr;
  if (imxrt_is_cache_mem(addr32)) {
    TU_ASSERT(tu_is_aligned32(addr32));
    SCB_CleanInvalidateDCache_by_Addr((uint32_t *) addr32, (int32_t) data_size);
  }
  return true;
}

#endif
