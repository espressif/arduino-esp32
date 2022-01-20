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
#if TU_CHECK_MCU(OPT_MCU_LPC175X_6X, OPT_MCU_LPC177X_8X, OPT_MCU_LPC40XX)
  #define HCD_ATTR_OHCI

#elif TU_CHECK_MCU(OPT_MCU_LPC18XX, OPT_MCU_LPC43XX)
  #define HCD_ATTR_EHCI_TRANSDIMENSION

#elif TU_CHECK_MCU(OPT_MCU_LPC54XXX)
  // #define HCD_ATTR_EHCI_NXP_PTD

#elif TU_CHECK_MCU(OPT_MCU_LPC55XX)
  // #define HCD_ATTR_EHCI_NXP_PTD

#elif TU_CHECK_MCU(OPT_MCU_MIMXRT10XX)
  #define HCD_ATTR_EHCI_TRANSDIMENSION

#elif TU_CHECK_MCU(OPT_MCU_MKL25ZXX)

//------------- Microchip -------------//
#elif TU_CHECK_MCU(OPT_MCU_SAMD21, OPT_MCU_SAMD51, OPT_MCU_SAME5X) || \
      TU_CHECK_MCU(OPT_MCU_SAMD11, OPT_MCU_SAML21, OPT_MCU_SAML22)

#elif TU_CHECK_MCU(OPT_MCU_SAMG)

#elif TU_CHECK_MCU(OPT_MCU_SAMX7X)

//------------- ST -------------//
#elif TU_CHECK_MCU(OPT_MCU_STM32F0, OPT_MCU_STM32F1, OPT_MCU_STM32F3) || \
      TU_CHECK_MCU(OPT_MCU_STM32L0, OPT_MCU_STM32L1, OPT_MCU_STM32L4)

#elif TU_CHECK_MCU(OPT_MCU_STM32F2, OPT_MCU_STM32F3, OPT_MCU_STM32F4)

#elif TU_CHECK_MCU(OPT_MCU_STM32F7)

#elif TU_CHECK_MCU(OPT_MCU_STM32H7)

//------------- Sony -------------//
#elif TU_CHECK_MCU(OPT_MCU_CXD56)

//------------- Nuvoton -------------//
#elif TU_CHECK_MCU(OPT_MCU_NUC505)

//------------- Espressif -------------//
#elif TU_CHECK_MCU(OPT_MCU_ESP32S2, OPT_MCU_ESP32S3)

//------------- Raspberry Pi -------------//
#elif TU_CHECK_MCU(OPT_MCU_RP2040)

//------------- Silabs -------------//
#elif TU_CHECK_MCU(OPT_MCU_EFM32GG)

//------------- Renesas -------------//
#elif TU_CHECK_MCU(OPT_MCU_RX63X, OPT_MCU_RX65X, OPT_MCU_RX72N)

//#elif TU_CHECK_MCU(OPT_MCU_MM32F327X)
//  #define DCD_ATTR_ENDPOINT_MAX not known yet

//------------- GigaDevice -------------//
#elif TU_CHECK_MCU(OPT_MCU_GD32VF103)

#else
//  #warning "DCD_ATTR_ENDPOINT_MAX is not defined for this MCU, default to 8"
#endif

// Default to fullspeed if not defined
//#ifndef PORT_HIGHSPEED
//  #define DCD_ATTR_PORT_HIGHSPEED 0x00
//#endif

#endif
