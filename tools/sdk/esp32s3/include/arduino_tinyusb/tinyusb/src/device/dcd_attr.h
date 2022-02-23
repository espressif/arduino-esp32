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

#ifndef TUSB_DCD_ATTR_H_
#define TUSB_DCD_ATTR_H_

#include "tusb_option.h"

// Attribute includes
// - ENDPOINT_MAX: max (logical) number of endpoint
// - ENDPOINT_EXCLUSIVE_NUMBER: endpoint number with different direction IN and OUT aren't allowed,
//                              e.g EP1 OUT & EP1 IN cannot exist together
// - PORT_HIGHSPEED: mask to indicate which port support highspeed mode, bit0 for port0 and so on.

//------------- NXP -------------//
#if   TU_CHECK_MCU(OPT_MCU_LPC11UXX, OPT_MCU_LPC13XX, OPT_MCU_LPC15XX)
  #define DCD_ATTR_ENDPOINT_MAX   5

#elif TU_CHECK_MCU(OPT_MCU_LPC175X_6X, OPT_MCU_LPC177X_8X, OPT_MCU_LPC40XX)
  #define DCD_ATTR_ENDPOINT_MAX   16

#elif TU_CHECK_MCU(OPT_MCU_LPC18XX, OPT_MCU_LPC43XX)
  // TODO USB0 has 6, USB1 has 4
  #define DCD_ATTR_CONTROLLER_CHIPIDEA_HS
  #define DCD_ATTR_ENDPOINT_MAX   6

#elif TU_CHECK_MCU(OPT_MCU_LPC51UXX)
   #define DCD_ATTR_ENDPOINT_MAX   5

#elif TU_CHECK_MCU(OPT_MCU_LPC54XXX)
  // TODO USB0 has 5, USB1 has 6
  #define DCD_ATTR_ENDPOINT_MAX   6

#elif TU_CHECK_MCU(OPT_MCU_LPC55XX)
  // TODO USB0 has 5, USB1 has 6
  #define DCD_ATTR_ENDPOINT_MAX   6

#elif TU_CHECK_MCU(OPT_MCU_MIMXRT10XX)
  #define DCD_ATTR_CONTROLLER_CHIPIDEA_HS
  #define DCD_ATTR_ENDPOINT_MAX   8

#elif TU_CHECK_MCU(OPT_MCU_MKL25ZXX, OPT_MCU_K32L2BXX)
  #define DCD_ATTR_ENDPOINT_MAX   16

#elif TU_CHECK_MCU(OPT_MCU_MM32F327X)
  #define DCD_ATTR_ENDPOINT_MAX   16

//------------- Nordic -------------//
#elif TU_CHECK_MCU(OPT_MCU_NRF5X)
  // 8 CBI + 1 ISO
  #define DCD_ATTR_ENDPOINT_MAX   9

//------------- Microchip -------------//
#elif TU_CHECK_MCU(OPT_MCU_SAMD21, OPT_MCU_SAMD51, OPT_MCU_SAME5X) || \
      TU_CHECK_MCU(OPT_MCU_SAMD11, OPT_MCU_SAML21, OPT_MCU_SAML22)
  #define DCD_ATTR_ENDPOINT_MAX   8

#elif TU_CHECK_MCU(OPT_MCU_SAMG)
  #define DCD_ATTR_ENDPOINT_MAX   6
  #define DCD_ATTR_ENDPOINT_EXCLUSIVE_NUMBER

#elif TU_CHECK_MCU(OPT_MCU_SAMX7X)
  #define DCD_ATTR_ENDPOINT_MAX   10
  #define DCD_ATTR_ENDPOINT_EXCLUSIVE_NUMBER

#elif TU_CHECK_MCU(OPT_MCU_PIC32MZ)
  #define DCD_ATTR_ENDPOINT_MAX   8
  #define DCD_ATTR_ENDPOINT_EXCLUSIVE_NUMBER

//------------- ST -------------//
#elif TU_CHECK_MCU(OPT_MCU_STM32F0)
  #define DCD_ATTR_ENDPOINT_MAX   8

#elif TU_CHECK_MCU(OPT_MCU_STM32F1)
  #if defined (STM32F105x8) || defined (STM32F105xB) || defined (STM32F105xC) || \
      defined (STM32F107xB) || defined (STM32F107xC)
    #define DCD_ATTR_ENDPOINT_MAX   4
    #define DCD_ATTR_DWC2_STM32
  #else
    #define DCD_ATTR_ENDPOINT_MAX   8
  #endif

#elif TU_CHECK_MCU(OPT_MCU_STM32F2)
  // FS has 4 ep, HS has 5 ep
  #define DCD_ATTR_ENDPOINT_MAX   6
  #define DCD_ATTR_DWC2_STM32

#elif TU_CHECK_MCU(OPT_MCU_STM32F3)
  #define DCD_ATTR_ENDPOINT_MAX   8

#elif TU_CHECK_MCU(OPT_MCU_STM32F4)
  // For most mcu, FS has 4, HS has 6. TODO 446/469/479 HS has 9
  #define DCD_ATTR_ENDPOINT_MAX   6
  #define DCD_ATTR_DWC2_STM32

#elif TU_CHECK_MCU(OPT_MCU_STM32F7)
  // FS has 6, HS has 9
  #define DCD_ATTR_ENDPOINT_MAX   9
  #define DCD_ATTR_DWC2_STM32

#elif TU_CHECK_MCU(OPT_MCU_STM32H7)
  #define DCD_ATTR_ENDPOINT_MAX   9
  #define DCD_ATTR_DWC2_STM32

#elif TU_CHECK_MCU(OPT_MCU_STM32G4)
  #define DCD_ATTR_ENDPOINT_MAX   8

#elif TU_CHECK_MCU(OPT_MCU_STM32L0, OPT_MCU_STM32L1)
  #define DCD_ATTR_ENDPOINT_MAX   8

#elif TU_CHECK_MCU(OPT_MCU_STM32L4)
  #if defined (STM32L475xx) || defined (STM32L476xx) ||                          \
      defined (STM32L485xx) || defined (STM32L486xx) || defined (STM32L496xx) || \
      defined (STM32L4A6xx) || defined (STM32L4P5xx) || defined (STM32L4Q5xx) || \
      defined (STM32L4R5xx) || defined (STM32L4R7xx) || defined (STM32L4R9xx) || \
      defined (STM32L4S5xx) || defined (STM32L4S7xx) || defined (STM32L4S9xx)
    #define DCD_ATTR_ENDPOINT_MAX   6
    #define DCD_ATTR_DWC2_STM32
  #else
    #define DCD_ATTR_ENDPOINT_MAX   8
  #endif

//------------- Sony -------------//
#elif TU_CHECK_MCU(OPT_MCU_CXD56)
  #define DCD_ATTR_ENDPOINT_MAX   7
  #define DCD_ATTR_ENDPOINT_EXCLUSIVE_NUMBER

//------------- TI -------------//
#elif TU_CHECK_MCU(OPT_MCU_MSP430x5xx)
  #define DCD_ATTR_ENDPOINT_MAX   8

#elif TU_CHECK_MCU(OPT_MCU_MSP432E4, OPT_MCU_TM4C123, OPT_MCU_TM4C129)
  #define DCD_ATTR_ENDPOINT_MAX   8

//------------- ValentyUSB -------------//
#elif TU_CHECK_MCU(OPT_MCU_VALENTYUSB_EPTRI)
  #define DCD_ATTR_ENDPOINT_MAX   16

//------------- Nuvoton -------------//
#elif TU_CHECK_MCU(OPT_MCU_NUC121, OPT_MCU_NUC126)
  #define DCD_ATTR_ENDPOINT_MAX   8

#elif TU_CHECK_MCU(OPT_MCU_NUC120)
  #define DCD_ATTR_ENDPOINT_MAX   6

#elif TU_CHECK_MCU(OPT_MCU_NUC505)
  #define DCD_ATTR_ENDPOINT_MAX   12

//------------- Espressif -------------//
#elif TU_CHECK_MCU(OPT_MCU_ESP32S2, OPT_MCU_ESP32S3)
  #define DCD_ATTR_ENDPOINT_MAX   6

//------------- Dialog -------------//
#elif TU_CHECK_MCU(OPT_MCU_DA1469X)
  #define DCD_ATTR_ENDPOINT_MAX   4

//------------- Raspberry Pi -------------//
#elif TU_CHECK_MCU(OPT_MCU_RP2040)
  #define DCD_ATTR_ENDPOINT_MAX   16

//------------- Silabs -------------//
#elif TU_CHECK_MCU(OPT_MCU_EFM32GG)
  #define DCD_ATTR_ENDPOINT_MAX   7

//------------- Renesas -------------//
#elif TU_CHECK_MCU(OPT_MCU_RX63X, OPT_MCU_RX65X, OPT_MCU_RX72N)
  #define DCD_ATTR_ENDPOINT_MAX   10

//------------- GigaDevice -------------//
#elif TU_CHECK_MCU(OPT_MCU_GD32VF103)
  #define DCD_ATTR_ENDPOINT_MAX   4

//------------- Broadcom -------------//
#elif TU_CHECK_MCU(OPT_MCU_BCM2711, OPT_MCU_BCM2835, OPT_MCU_BCM2837)
  #define DCD_ATTR_ENDPOINT_MAX   8

//------------- Broadcom -------------//
#elif TU_CHECK_MCU(OPT_MCU_XMC4000)
  #define DCD_ATTR_ENDPOINT_MAX   8

//------------- BridgeTek -------------//
#elif TU_CHECK_MCU(OPT_MCU_FT90X)
  #define DCD_ATTR_ENDPOINT_MAX   8

#elif TU_CHECK_MCU(OPT_MCU_FT93X)
  #define DCD_ATTR_ENDPOINT_MAX   16

//------------ Allwinner -------------//
#elif TU_CHECK_MCU(OPT_MCU_F1C100S)
  #define DCD_ATTR_ENDPOINT_MAX   4

#else
  #warning "DCD_ATTR_ENDPOINT_MAX is not defined for this MCU, default to 8"
  #define DCD_ATTR_ENDPOINT_MAX   8
#endif

// Default to fullspeed if not defined
//#ifndef PORT_HIGHSPEED
//  #define DCD_ATTR_PORT_HIGHSPEED 0x00
//#endif

#endif
