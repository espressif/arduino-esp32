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

#ifndef TUSB_MCU_H_
#define TUSB_MCU_H_

//--------------------------------------------------------------------+
// Port/Platform Specific
// TUP stand for TinyUSB Port/Platform (can be renamed)
//--------------------------------------------------------------------+

//------------- Unaligned Memory Access -------------//

#ifdef __ARM_ARCH
  // ARM Architecture set __ARM_FEATURE_UNALIGNED to 1 for mcu supports unaligned access
  #if defined(__ARM_FEATURE_UNALIGNED) && __ARM_FEATURE_UNALIGNED == 1
    #define TUP_ARCH_STRICT_ALIGN   0
  #else
    #define TUP_ARCH_STRICT_ALIGN   1
  #endif
#else
  // TODO default to strict align for others
  // Should investigate other architecture such as risv, xtensa, mips for optimal setting
  #define TUP_ARCH_STRICT_ALIGN   1
#endif

/* USB Controller Attributes for Device, Host or MCU (both)
 * - ENDPOINT_MAX: max (logical) number of endpoint
 * - ENDPOINT_EXCLUSIVE_NUMBER: endpoint number with different direction IN and OUT aren't allowed,
 *                              e.g EP1 OUT & EP1 IN cannot exist together
 * - RHPORT_HIGHSPEED: support highspeed with on-chip PHY
 */

//--------------------------------------------------------------------+
// NXP
//--------------------------------------------------------------------+
#if   TU_CHECK_MCU(OPT_MCU_LPC11UXX, OPT_MCU_LPC13XX, OPT_MCU_LPC15XX)
  #define TUP_USBIP_IP3511
  #define TUP_DCD_ENDPOINT_MAX    5

#elif TU_CHECK_MCU(OPT_MCU_LPC175X_6X, OPT_MCU_LPC177X_8X, OPT_MCU_LPC40XX)
  #define TUP_DCD_ENDPOINT_MAX    16
  #define TUP_USBIP_OHCI
  #define TUP_OHCI_RHPORTS        2

#elif TU_CHECK_MCU(OPT_MCU_LPC51UXX)
   #define TUP_USBIP_IP3511
   #define TUP_DCD_ENDPOINT_MAX   5

#elif TU_CHECK_MCU(OPT_MCU_LPC54)
  // TODO USB0 has 5, USB1 has 6
  #define TUP_USBIP_IP3511
  #define TUP_DCD_ENDPOINT_MAX    6

#elif TU_CHECK_MCU(OPT_MCU_LPC55)
  // TODO USB0 has 5, USB1 has 6
  #define TUP_USBIP_IP3511
  #define TUP_DCD_ENDPOINT_MAX    6

#elif TU_CHECK_MCU(OPT_MCU_LPC18XX, OPT_MCU_LPC43XX)
  // USB0 has 6 with HS PHY, USB1 has 4 only FS
  #define TUP_USBIP_CHIPIDEA_HS
  #define TUP_USBIP_EHCI

  #define TUP_DCD_ENDPOINT_MAX    6
  #define TUP_RHPORT_HIGHSPEED    1

#elif TU_CHECK_MCU(OPT_MCU_MCXN9)
  // USB0 is chipidea FS
  #define TUP_USBIP_CHIPIDEA_FS
  #define TUP_USBIP_CHIPIDEA_FS_MCX

  // USB1 is chipidea HS
  #define TUP_USBIP_CHIPIDEA_HS
  #define TUP_USBIP_EHCI

  #define TUP_DCD_ENDPOINT_MAX    8
  #define TUP_RHPORT_HIGHSPEED    1

#elif TU_CHECK_MCU(OPT_MCU_MCXA15)
  // USB0 is chipidea FS
  #define TUP_USBIP_CHIPIDEA_FS
  #define TUP_USBIP_CHIPIDEA_FS_MCX

  #define TUP_DCD_ENDPOINT_MAX    16

#elif TU_CHECK_MCU(OPT_MCU_MIMXRT1XXX)
  #define TUP_USBIP_CHIPIDEA_HS
  #define TUP_USBIP_EHCI

  #define TUP_DCD_ENDPOINT_MAX    8
  #define TUP_RHPORT_HIGHSPEED    1

#elif TU_CHECK_MCU(OPT_MCU_KINETIS_KL, OPT_MCU_KINETIS_K32L, OPT_MCU_KINETIS_K)
  #define TUP_USBIP_CHIPIDEA_FS
  #define TUP_USBIP_CHIPIDEA_FS_KINETIS
  #define TUP_DCD_ENDPOINT_MAX    16

#elif TU_CHECK_MCU(OPT_MCU_MM32F327X)
  #define TUP_DCD_ENDPOINT_MAX    16

//--------------------------------------------------------------------+
// Nordic
//--------------------------------------------------------------------+
#elif TU_CHECK_MCU(OPT_MCU_NRF5X)
  // 8 CBI + 1 ISO
  #define TUP_DCD_ENDPOINT_MAX    9

//--------------------------------------------------------------------+
// Microchip
//--------------------------------------------------------------------+
#elif TU_CHECK_MCU(OPT_MCU_SAMD21, OPT_MCU_SAMD51, OPT_MCU_SAME5X) || \
      TU_CHECK_MCU(OPT_MCU_SAMD11, OPT_MCU_SAML21, OPT_MCU_SAML22)
  #define TUP_DCD_ENDPOINT_MAX    8

#elif TU_CHECK_MCU(OPT_MCU_SAMG)
  #define TUP_DCD_ENDPOINT_MAX    6
  #define TUP_DCD_ENDPOINT_EXCLUSIVE_NUMBER

#elif TU_CHECK_MCU(OPT_MCU_SAMX7X)
  #define TUP_DCD_ENDPOINT_MAX    10
  #define TUP_RHPORT_HIGHSPEED    1
  #define TUP_DCD_ENDPOINT_EXCLUSIVE_NUMBER

#elif TU_CHECK_MCU(OPT_MCU_PIC32MZ)
  #define TUP_DCD_ENDPOINT_MAX    8
  #define TUP_DCD_ENDPOINT_EXCLUSIVE_NUMBER

#elif TU_CHECK_MCU(OPT_MCU_PIC32MX, OPT_MCU_PIC32MM, OPT_MCU_PIC32MK) || \
      TU_CHECK_MCU(OPT_MCU_PIC24, OPT_MCU_DSPIC33)
  #define TUP_DCD_ENDPOINT_MAX    16
  #define TUP_DCD_ENDPOINT_EXCLUSIVE_NUMBER

//--------------------------------------------------------------------+
// ST
//--------------------------------------------------------------------+
#elif TU_CHECK_MCU(OPT_MCU_STM32F0)
  #define TUP_USBIP_FSDEV
  #define TUP_USBIP_FSDEV_STM32
  #define TUP_DCD_ENDPOINT_MAX    8

#elif TU_CHECK_MCU(OPT_MCU_STM32F1)
  // - F102, F103 use fsdev
  // - F105, F107 use dwc2
  #if defined (STM32F105x8) || defined (STM32F105xB) || defined (STM32F105xC) || \
      defined (STM32F107xB) || defined (STM32F107xC)
    #define TUP_USBIP_DWC2
    #define TUP_USBIP_DWC2_STM32

    #define TUP_DCD_ENDPOINT_MAX  4
  #elif defined(STM32F102x6) || defined(STM32F102xB) || \
        defined(STM32F103x6) || defined(STM32F103xB) || defined(STM32F103xE) || defined(STM32F103xG)
    #define TUP_USBIP_FSDEV
    #define TUP_USBIP_FSDEV_STM32
    #define TUP_DCD_ENDPOINT_MAX  8
  #else
    #error "Unsupported STM32F1 mcu"
  #endif

#elif TU_CHECK_MCU(OPT_MCU_STM32F2)
  #define TUP_USBIP_DWC2
  #define TUP_USBIP_DWC2_STM32

  // FS has 4 ep, HS has 5 ep
  #define TUP_DCD_ENDPOINT_MAX    6

#elif TU_CHECK_MCU(OPT_MCU_STM32F3)
  #define TUP_USBIP_FSDEV
  #define TUP_USBIP_FSDEV_STM32
  #define TUP_DCD_ENDPOINT_MAX    8

#elif TU_CHECK_MCU(OPT_MCU_STM32F4)
  #define TUP_USBIP_DWC2
  #define TUP_USBIP_DWC2_STM32
  #define TUP_USBIP_DWC2_TEST_MODE

  // For most mcu, FS has 4, HS has 6. TODO 446/469/479 HS has 9
  #define TUP_DCD_ENDPOINT_MAX    6

#elif TU_CHECK_MCU(OPT_MCU_STM32F7)
  #define TUP_USBIP_DWC2
  #define TUP_USBIP_DWC2_STM32

  // FS has 6, HS has 9
  #define TUP_DCD_ENDPOINT_MAX    9

  // MCU with on-chip HS Phy
  #if defined(STM32F723xx) || defined(STM32F730xx) || defined(STM32F733xx)
    #define TUP_RHPORT_HIGHSPEED  1 // Port0: FS, Port1: HS
    #define TUP_USBIP_DWC2_TEST_MODE
  #endif

#elif TU_CHECK_MCU(OPT_MCU_STM32H7)
  #define TUP_USBIP_DWC2
  #define TUP_USBIP_DWC2_STM32

  #define TUP_DCD_ENDPOINT_MAX    9

#elif TU_CHECK_MCU(OPT_MCU_STM32H5)
  #define TUP_USBIP_FSDEV
  #define TUP_USBIP_FSDEV_STM32
  #define TUP_DCD_ENDPOINT_MAX    8

#elif TU_CHECK_MCU(OPT_MCU_STM32G4)
  // Device controller
  #define TUP_USBIP_FSDEV
  #define TUP_USBIP_FSDEV_STM32

  // TypeC controller
  #define TUP_USBIP_TYPEC_STM32
  #define TUP_DCD_ENDPOINT_MAX    8
  #define TUP_TYPEC_RHPORTS_NUM 1

#elif TU_CHECK_MCU(OPT_MCU_STM32G0)
  #define TUP_USBIP_FSDEV
  #define TUP_USBIP_FSDEV_STM32
  #define TUP_DCD_ENDPOINT_MAX    8

#elif TU_CHECK_MCU(OPT_MCU_STM32L0, OPT_MCU_STM32L1)
  #define TUP_USBIP_FSDEV
  #define TUP_USBIP_FSDEV_STM32
  #define TUP_DCD_ENDPOINT_MAX    8

#elif TU_CHECK_MCU(OPT_MCU_STM32L4)
  // - L4x2, L4x3 use fsdev
  // - L4x4, L4x6, L4x7, L4x9 use dwc2
  #if defined (STM32L475xx) || defined (STM32L476xx) ||                          \
      defined (STM32L485xx) || defined (STM32L486xx) || defined (STM32L496xx) || \
      defined (STM32L4A6xx) || defined (STM32L4P5xx) || defined (STM32L4Q5xx) || \
      defined (STM32L4R5xx) || defined (STM32L4R7xx) || defined (STM32L4R9xx) || \
      defined (STM32L4S5xx) || defined (STM32L4S7xx) || defined (STM32L4S9xx)
    #define TUP_USBIP_DWC2
    #define TUP_USBIP_DWC2_STM32

    #define TUP_DCD_ENDPOINT_MAX  6
  #elif defined(STM32L412xx) || defined(STM32L422xx) || defined(STM32L432xx) || defined(STM32L433xx) || \
        defined(STM32L442xx) || defined(STM32L443xx) || defined(STM32L452xx) || defined(STM32L462xx)
    #define TUP_USBIP_FSDEV
    #define TUP_USBIP_FSDEV_STM32
    #define TUP_DCD_ENDPOINT_MAX  8
  #else
    #error "Unsupported STM32L4 mcu"
  #endif

#elif TU_CHECK_MCU(OPT_MCU_STM32WB)
  #define TUP_USBIP_FSDEV
  #define TUP_USBIP_FSDEV_STM32
  #define TUP_DCD_ENDPOINT_MAX    8

#elif TU_CHECK_MCU(OPT_MCU_STM32U5)
  #define TUP_USBIP_DWC2
  #define TUP_USBIP_DWC2_STM32

  // U59x/5Ax/5Fx/5Gx are highspeed with built-in HS PHY
  #if defined(STM32U595xx) || defined(STM32U599xx) || defined(STM32U5A5xx) || defined(STM32U5A9xx) || \
      defined(STM32U5F7xx) || defined(STM32U5F9xx) || defined(STM32U5G7xx) || defined(STM32U5G9xx)
    #define TUP_DCD_ENDPOINT_MAX  9
    #define TUP_RHPORT_HIGHSPEED  1
    #define TUP_USBIP_DWC2_TEST_MODE
  #else
    #define TUP_DCD_ENDPOINT_MAX  6
  #endif

#elif TU_CHECK_MCU(OPT_MCU_STM32L5)
  #define TUP_USBIP_FSDEV
  #define TUP_USBIP_FSDEV_STM32
  #define TUP_DCD_ENDPOINT_MAX    8

//--------------------------------------------------------------------+
// Sony
//--------------------------------------------------------------------+
#elif TU_CHECK_MCU(OPT_MCU_CXD56)
  #define TUP_DCD_ENDPOINT_MAX    7
  #define TUP_RHPORT_HIGHSPEED    1
  #define TUP_DCD_ENDPOINT_EXCLUSIVE_NUMBER

//--------------------------------------------------------------------+
// TI
//--------------------------------------------------------------------+
#elif TU_CHECK_MCU(OPT_MCU_MSP430x5xx)
  #define TUP_DCD_ENDPOINT_MAX    8

#elif TU_CHECK_MCU(OPT_MCU_MSP432E4, OPT_MCU_TM4C123, OPT_MCU_TM4C129)
  #define TUP_DCD_ENDPOINT_MAX    8

//--------------------------------------------------------------------+
// ValentyUSB (Litex)
//--------------------------------------------------------------------+
#elif TU_CHECK_MCU(OPT_MCU_VALENTYUSB_EPTRI)
  #define TUP_DCD_ENDPOINT_MAX    16

//--------------------------------------------------------------------+
// Nuvoton
//--------------------------------------------------------------------+
#elif TU_CHECK_MCU(OPT_MCU_NUC121, OPT_MCU_NUC126)
  #define TUP_DCD_ENDPOINT_MAX    8

#elif TU_CHECK_MCU(OPT_MCU_NUC120)
  #define TUP_DCD_ENDPOINT_MAX    6

#elif TU_CHECK_MCU(OPT_MCU_NUC505)
  #define TUP_DCD_ENDPOINT_MAX    12
  #define TUP_RHPORT_HIGHSPEED    1

//--------------------------------------------------------------------+
// Espressif
//--------------------------------------------------------------------+
#elif TU_CHECK_MCU(OPT_MCU_ESP32S2, OPT_MCU_ESP32S3)
  #define TUP_USBIP_DWC2
  #define TUP_DCD_ENDPOINT_MAX    6

#elif TU_CHECK_MCU(OPT_MCU_ESP32, OPT_MCU_ESP32C2, OPT_MCU_ESP32C3, OPT_MCU_ESP32C6, OPT_MCU_ESP32H2) && (CFG_TUD_ENABLED || !(defined(CFG_TUH_MAX3421) && CFG_TUH_MAX3421))
  #error "MCUs are only supported with CFG_TUH_MAX3421 enabled"

//--------------------------------------------------------------------+
// Dialog
//--------------------------------------------------------------------+
#elif TU_CHECK_MCU(OPT_MCU_DA1469X)
  #define TUP_DCD_ENDPOINT_MAX    4

//--------------------------------------------------------------------+
// Raspberry Pi
//--------------------------------------------------------------------+
#elif TU_CHECK_MCU(OPT_MCU_RP2040)
  #define TUP_DCD_ENDPOINT_MAX    16

  #define TU_ATTR_FAST_FUNC       __attribute__((section(".time_critical.tinyusb")))

//--------------------------------------------------------------------+
// Silabs
//--------------------------------------------------------------------+
#elif TU_CHECK_MCU(OPT_MCU_EFM32GG)
  #define TUP_USBIP_DWC2
  #define TUP_DCD_ENDPOINT_MAX    7

//--------------------------------------------------------------------+
// Renesas
//--------------------------------------------------------------------+
#elif TU_CHECK_MCU(OPT_MCU_RX63X, OPT_MCU_RX65X, OPT_MCU_RX72N, OPT_MCU_RAXXX)
  #define TUP_USBIP_RUSB2
  #define TUP_DCD_ENDPOINT_MAX    10

//--------------------------------------------------------------------+
// GigaDevice
//--------------------------------------------------------------------+
#elif TU_CHECK_MCU(OPT_MCU_GD32VF103)
  #define TUP_USBIP_DWC2
  #define TUP_DCD_ENDPOINT_MAX    4

//--------------------------------------------------------------------+
// Broadcom
//--------------------------------------------------------------------+
#elif TU_CHECK_MCU(OPT_MCU_BCM2711, OPT_MCU_BCM2835, OPT_MCU_BCM2837)
  #define TUP_USBIP_DWC2
  #define TUP_DCD_ENDPOINT_MAX    8
  #define TUP_RHPORT_HIGHSPEED    1

//--------------------------------------------------------------------+
// Infineon
//--------------------------------------------------------------------+
#elif TU_CHECK_MCU(OPT_MCU_XMC4000)
  #define TUP_USBIP_DWC2
  #define TUP_DCD_ENDPOINT_MAX    8

//--------------------------------------------------------------------+
// BridgeTek
//--------------------------------------------------------------------+
#elif TU_CHECK_MCU(OPT_MCU_FT90X)
  #define TUP_DCD_ENDPOINT_MAX    8
  #define TUP_RHPORT_HIGHSPEED    1

#elif TU_CHECK_MCU(OPT_MCU_FT93X)
  #define TUP_DCD_ENDPOINT_MAX    16
  #define TUP_RHPORT_HIGHSPEED    1

//--------------------------------------------------------------------+
// Allwinner
//--------------------------------------------------------------------+
#elif TU_CHECK_MCU(OPT_MCU_F1C100S)
  #define TUP_DCD_ENDPOINT_MAX    4

//------------- WCH -------------//
#elif TU_CHECK_MCU(OPT_MCU_CH32V307)
  // v307 support both FS and HS
  #define TUP_USBIP_WCH_USBHS
  #define TUP_USBIP_WCH_USBFS

  #define TUP_RHPORT_HIGHSPEED    1 // default to highspeed
  #define TUP_DCD_ENDPOINT_MAX    (CFG_TUD_MAX_SPEED == OPT_MODE_HIGH_SPEED ? 16 : 8)

#elif TU_CHECK_MCU(OPT_MCU_CH32F20X)
  #define TUP_USBIP_WCH_USBHS
  #define TUP_USBIP_WCH_USBFS

  #define TUP_RHPORT_HIGHSPEED    1 // default to highspeed
  #define TUP_DCD_ENDPOINT_MAX    (CFG_TUD_MAX_SPEED == OPT_MODE_HIGH_SPEED ? 16 : 8)

#elif TU_CHECK_MCU(OPT_MCU_CH32V20X)
  #define TUP_USBIP_WCH_USBFS
  #define TUP_DCD_ENDPOINT_MAX    8

#endif


//--------------------------------------------------------------------+
// External USB controller
//--------------------------------------------------------------------+

#if defined(CFG_TUH_MAX3421) && CFG_TUH_MAX3421
  #ifndef CFG_TUH_MAX3421_ENDPOINT_TOTAL
    #define CFG_TUH_MAX3421_ENDPOINT_TOTAL  (8 + 4*(CFG_TUH_DEVICE_MAX-1))
  #endif
#endif


//--------------------------------------------------------------------+
// Default Values
//--------------------------------------------------------------------+

#ifndef TUP_MCU_MULTIPLE_CORE
#define TUP_MCU_MULTIPLE_CORE 0
#endif

#if !defined(TUP_DCD_ENDPOINT_MAX) && defined(CFG_TUD_ENABLED) && CFG_TUD_ENABLED
  #warning "TUP_DCD_ENDPOINT_MAX is not defined for this MCU, default to 8"
  #define TUP_DCD_ENDPOINT_MAX    8
#endif

// Default to fullspeed if not defined
#ifndef TUP_RHPORT_HIGHSPEED
  #define TUP_RHPORT_HIGHSPEED    0
#endif

// fast function, normally mean placing function in SRAM
#ifndef TU_ATTR_FAST_FUNC
  #define TU_ATTR_FAST_FUNC
#endif

#if defined(TUP_USBIP_DWC2) || defined(TUP_USBIP_FSDEV)
  #define TUP_DCD_EDPT_ISO_ALLOC
#endif

#if defined(TUP_USBIP_DWC2)
  #define TUP_MEM_CONST_ADDR
#endif

#endif
