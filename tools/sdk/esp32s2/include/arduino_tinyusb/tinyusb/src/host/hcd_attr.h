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

#ifndef TUSB_HCD_ATTR_H_
#define TUSB_HCD_ATTR_H_

#include "tusb_option.h"

// Attribute includes
// - ENDPOINT_MAX: max (logical) number of endpoint
// - PORT_HIGHSPEED: mask to indicate which port support highspeed mode, bit0 for port0 and so on.

//------------- NXP -------------//
#if TU_CHECK_MCU(LPC175X_6X) || TU_CHECK_MCU(LPC177X_8X) || TU_CHECK_MCU(LPC40XX)
  #define HCD_ATTR_OHCI

#elif TU_CHECK_MCU(LPC18XX) || TU_CHECK_MCU(LPC43XX)
  #define HCD_ATTR_EHCI_TRANSDIMENSION

#elif TU_CHECK_MCU(LPC54XXX)
  // #define HCD_ATTR_EHCI_NXP_PTD

#elif TU_CHECK_MCU(LPC55XX)
  // #define HCD_ATTR_EHCI_NXP_PTD

#elif TU_CHECK_MCU(MIMXRT10XX)
  #define HCD_ATTR_EHCI_TRANSDIMENSION

#elif TU_CHECK_MCU(MKL25ZXX)

//------------- Microchip -------------//
#elif TU_CHECK_MCU(SAMD21) || TU_CHECK_MCU(SAMD51) || TU_CHECK_MCU(SAME5X) || \
      TU_CHECK_MCU(SAMD11) || TU_CHECK_MCU(SAML21) || TU_CHECK_MCU(SAML22)

#elif TU_CHECK_MCU(SAMG)

#elif TU_CHECK_MCU(SAMX7X)

//------------- ST -------------//
#elif TU_CHECK_MCU(STM32F0) || TU_CHECK_MCU(STM32F1) || TU_CHECK_MCU(STM32F3) || \
      TU_CHECK_MCU(STM32L0) || TU_CHECK_MCU(STM32L1) || TU_CHECK_MCU(STM32L4)

#elif TU_CHECK_MCU(STM32F2) || TU_CHECK_MCU(STM32F4) || TU_CHECK_MCU(STM32F3)

#elif TU_CHECK_MCU(STM32F7)

#elif TU_CHECK_MCU(STM32H7)

//------------- Sony -------------//
#elif TU_CHECK_MCU(CXD56)

//------------- Nuvoton -------------//
#elif TU_CHECK_MCU(NUC505)

//------------- Espressif -------------//
#elif TU_CHECK_MCU(ESP32S2) || TU_CHECK_MCU(ESP32S3)

//------------- Raspberry Pi -------------//
#elif TU_CHECK_MCU(RP2040)

//------------- Silabs -------------//
#elif TU_CHECK_MCU(EFM32GG) || TU_CHECK_MCU(EFM32GG11) || TU_CHECK_MCU(EFM32GG12)

//------------- Renesas -------------//
#elif TU_CHECK_MCU(RX63X) || TU_CHECK_MCU(RX65X) || TU_CHECK_MCU(RX72N)

//#elif TU_CHECK_MCU(MM32F327X)
//  #define DCD_ATTR_ENDPOINT_MAX not known yet

//------------- GigaDevice -------------//
#elif TU_CHECK_MCU(GD32VF103)

#else
//  #warning "DCD_ATTR_ENDPOINT_MAX is not defined for this MCU, default to 8"
#endif

// Default to fullspeed if not defined
//#ifndef PORT_HIGHSPEED
//  #define DCD_ATTR_PORT_HIGHSPEED 0x00
//#endif

#endif
