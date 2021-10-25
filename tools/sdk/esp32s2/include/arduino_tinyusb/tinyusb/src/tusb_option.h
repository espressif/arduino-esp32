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

#ifndef _TUSB_OPTION_H_
#define _TUSB_OPTION_H_

#define TUSB_VERSION_MAJOR     0
#define TUSB_VERSION_MINOR     12
#define TUSB_VERSION_REVISION  0
#define TUSB_VERSION_STRING    TU_STRING(TUSB_VERSION_MAJOR) "." TU_STRING(TUSB_VERSION_MINOR) "." TU_STRING(TUSB_VERSION_REVISION)

//--------------------------------------------------------------------+
// Supported MCUs
// CFG_TUSB_MCU must be defined to one of following value
//--------------------------------------------------------------------+
#define TU_CHECK_MCU(_m)            (CFG_TUSB_MCU == OPT_MCU_##_m)

#define OPT_MCU_NONE                0

// LPC
#define OPT_MCU_LPC11UXX            1 ///< NXP LPC11Uxx
#define OPT_MCU_LPC13XX             2 ///< NXP LPC13xx
#define OPT_MCU_LPC15XX             3 ///< NXP LPC15xx
#define OPT_MCU_LPC175X_6X          4 ///< NXP LPC175x, LPC176x
#define OPT_MCU_LPC177X_8X          5 ///< NXP LPC177x, LPC178x
#define OPT_MCU_LPC18XX             6 ///< NXP LPC18xx
#define OPT_MCU_LPC40XX             7 ///< NXP LPC40xx
#define OPT_MCU_LPC43XX             8 ///< NXP LPC43xx
#define OPT_MCU_LPC51UXX            9 ///< NXP LPC51U6x
#define OPT_MCU_LPC54XXX           10 ///< NXP LPC54xxx
#define OPT_MCU_LPC55XX            11 ///< NXP LPC55xx

// NRF
#define OPT_MCU_NRF5X             100 ///< Nordic nRF5x series

// SAM
#define OPT_MCU_SAMD21            200 ///< MicroChip SAMD21
#define OPT_MCU_SAMD51            201 ///< MicroChip SAMD51
#define OPT_MCU_SAMG              202 ///< MicroChip SAMDG series
#define OPT_MCU_SAME5X            203 ///< MicroChip SAM E5x
#define OPT_MCU_SAMD11            204 ///< MicroChip SAMD11
#define OPT_MCU_SAML22            205 ///< MicroChip SAML22
#define OPT_MCU_SAML21            206 ///< MicroChip SAML21
#define OPT_MCU_SAMX7X            207 ///< MicroChip SAME70, S70, V70, V71 family

// STM32
#define OPT_MCU_STM32F0           300 ///< ST STM32F0
#define OPT_MCU_STM32F1           301 ///< ST STM32F1
#define OPT_MCU_STM32F2           302 ///< ST STM32F2
#define OPT_MCU_STM32F3           303 ///< ST STM32F3
#define OPT_MCU_STM32F4           304 ///< ST STM32F4
#define OPT_MCU_STM32F7           305 ///< ST STM32F7
#define OPT_MCU_STM32H7           306 ///< ST STM32H7
#define OPT_MCU_STM32L0           307 ///< ST STM32L0
#define OPT_MCU_STM32L1           308 ///< ST STM32L1
#define OPT_MCU_STM32L4           309 ///< ST STM32L4

// Sony
#define OPT_MCU_CXD56             400 ///< SONY CXD56

// TI MSP430
#define OPT_MCU_MSP430x5xx        500 ///< TI MSP430x5xx

// ValentyUSB eptri
#define OPT_MCU_VALENTYUSB_EPTRI  600 ///< Fomu eptri config

// NXP iMX RT
#define OPT_MCU_MIMXRT10XX        700 ///< NXP iMX RT10xx

// Nuvoton
#define OPT_MCU_NUC121            800
#define OPT_MCU_NUC126            801
#define OPT_MCU_NUC120            802
#define OPT_MCU_NUC505            803

// Espressif
#define OPT_MCU_ESP32S2           900 ///< Espressif ESP32-S2
#define OPT_MCU_ESP32S3           901 ///< Espressif ESP32-S3

// Dialog
#define OPT_MCU_DA1469X          1000 ///< Dialog Semiconductor DA1469x

// Raspberry Pi
#define OPT_MCU_RP2040           1100 ///< Raspberry Pi RP2040

// NXP Kinetis
#define OPT_MCU_MKL25ZXX         1200 ///< NXP MKL25Zxx
#define OPT_MCU_K32L2BXX         1201 ///< NXP K32L2Bxx

// Silabs
#define OPT_MCU_EFM32GG          1300 ///< Silabs EFM32GG
#define OPT_MCU_EFM32GG11        1301 ///< Silabs EFM32GG11
#define OPT_MCU_EFM32GG12        1302 ///< Silabs EFM32GG12

// Renesas RX
#define OPT_MCU_RX63X            1400 ///< Renesas RX63N/631
#define OPT_MCU_RX65X            1401 ///< Renesas RX65N/RX651
#define OPT_MCU_RX72N            1402 ///< Renesas RX72N

// Mind Motion
#define OPT_MCU_MM32F327X        1500 ///< Mind Motion MM32F327

// GigaDevice
#define OPT_MCU_GD32VF103        1600 ///< GigaDevice GD32VF103

//--------------------------------------------------------------------+
// Supported OS
//--------------------------------------------------------------------+

#define OPT_OS_NONE       1  ///< No RTOS
#define OPT_OS_FREERTOS   2  ///< FreeRTOS
#define OPT_OS_MYNEWT     3  ///< Mynewt OS
#define OPT_OS_CUSTOM     4  ///< Custom OS is implemented by application
#define OPT_OS_PICO       5  ///< Raspberry Pi Pico SDK
#define OPT_OS_RTTHREAD   6  ///< RT-Thread

// Allow to use command line to change the config name/location
#ifdef CFG_TUSB_CONFIG_FILE
  #include CFG_TUSB_CONFIG_FILE
#else
  #include "tusb_config.h"
#endif

//--------------------------------------------------------------------
// RootHub Mode Configuration
// CFG_TUSB_RHPORTx_MODE contains operation mode and speed for that port
//--------------------------------------------------------------------

// Lower 4-bit is operational mode
#define OPT_MODE_NONE         0x00 ///< Disabled
#define OPT_MODE_DEVICE       0x01 ///< Device Mode
#define OPT_MODE_HOST         0x02 ///< Host Mode

// Higher 4-bit is max operational speed (corresponding to tusb_speed_t)
#define OPT_MODE_FULL_SPEED   0x00 ///< Max Full Speed
#define OPT_MODE_LOW_SPEED    0x10 ///< Max Low Speed
#define OPT_MODE_HIGH_SPEED   0x20 ///< Max High Speed


#ifndef CFG_TUSB_RHPORT0_MODE
  #define CFG_TUSB_RHPORT0_MODE OPT_MODE_NONE
#endif


#ifndef CFG_TUSB_RHPORT1_MODE
  #define CFG_TUSB_RHPORT1_MODE OPT_MODE_NONE
#endif

#if (((CFG_TUSB_RHPORT0_MODE) & OPT_MODE_HOST  ) && ((CFG_TUSB_RHPORT1_MODE) & OPT_MODE_HOST  )) || \
    (((CFG_TUSB_RHPORT0_MODE) & OPT_MODE_DEVICE) && ((CFG_TUSB_RHPORT1_MODE) & OPT_MODE_DEVICE))
  #error "TinyUSB currently does not support same modes on more than 1 roothub port"
#endif

// Which roothub port is configured as host
#define TUH_OPT_RHPORT          ( ((CFG_TUSB_RHPORT0_MODE) & OPT_MODE_HOST) ? 0 : (((CFG_TUSB_RHPORT1_MODE) & OPT_MODE_HOST) ? 1 : -1) )
#define TUSB_OPT_HOST_ENABLED   ( TUH_OPT_RHPORT >= 0 )

// Which roothub port is configured as device
#define TUD_OPT_RHPORT          ( ((CFG_TUSB_RHPORT0_MODE) & OPT_MODE_DEVICE) ? 0 : (((CFG_TUSB_RHPORT1_MODE) & OPT_MODE_DEVICE) ? 1 : -1) )

#if TUD_OPT_RHPORT == 0
#define TUD_OPT_HIGH_SPEED      ( (CFG_TUSB_RHPORT0_MODE) & OPT_MODE_HIGH_SPEED )
#else
#define TUD_OPT_HIGH_SPEED      ( (CFG_TUSB_RHPORT1_MODE) & OPT_MODE_HIGH_SPEED )
#endif

#define TUSB_OPT_DEVICE_ENABLED ( TUD_OPT_RHPORT >= 0 )

//--------------------------------------------------------------------+
// COMMON OPTIONS
//--------------------------------------------------------------------+

// Debug enable to print out error message
#ifndef CFG_TUSB_DEBUG
  #define CFG_TUSB_DEBUG 0
#endif

// place data in accessible RAM for usb controller
#ifndef CFG_TUSB_MEM_SECTION
  #define CFG_TUSB_MEM_SECTION
#endif

// alignment requirement of buffer used for endpoint transferring
#ifndef CFG_TUSB_MEM_ALIGN
  #define CFG_TUSB_MEM_ALIGN      TU_ATTR_ALIGNED(4)
#endif

// OS selection
#ifndef CFG_TUSB_OS
  #define CFG_TUSB_OS             OPT_OS_NONE
#endif

#ifndef CFG_TUSB_OS_INC_PATH
  #define CFG_TUSB_OS_INC_PATH
#endif

//--------------------------------------------------------------------
// DEVICE OPTIONS
//--------------------------------------------------------------------

#ifndef CFG_TUD_ENDPOINT0_SIZE
  #define CFG_TUD_ENDPOINT0_SIZE  64
#endif

#ifndef CFG_TUD_CDC
  #define CFG_TUD_CDC             0
#endif

#ifndef CFG_TUD_MSC
  #define CFG_TUD_MSC             0
#endif

#ifndef CFG_TUD_HID
  #define CFG_TUD_HID             0
#endif

#ifndef CFG_TUD_AUDIO
  #define CFG_TUD_AUDIO           0
#endif

#ifndef CFG_TUD_VIDEO
  #define CFG_TUD_VIDEO           0
#endif

#ifndef CFG_TUD_MIDI
  #define CFG_TUD_MIDI            0
#endif

#ifndef CFG_TUD_VENDOR
  #define CFG_TUD_VENDOR          0
#endif

#ifndef CFG_TUD_USBTMC
  #define CFG_TUD_USBTMC          0
#endif

#ifndef CFG_TUD_DFU_RUNTIME
  #define CFG_TUD_DFU_RUNTIME     0
#endif

#ifndef CFG_TUD_DFU
  #define CFG_TUD_DFU             0
#endif

#ifndef CFG_TUD_BTH
  #define CFG_TUD_BTH             0
#endif

#ifndef CFG_TUD_ECM_RNDIS
  #ifdef CFG_TUD_NET
    #warning "CFG_TUD_NET is renamed to CFG_TUD_ECM_RNDIS"
    #define CFG_TUD_ECM_RNDIS   CFG_TUD_NET
  #else
    #define CFG_TUD_ECM_RNDIS   0
  #endif
#endif

#ifndef CFG_TUD_NCM
  #define CFG_TUD_NCM         0
#endif

//--------------------------------------------------------------------
// HOST OPTIONS
//--------------------------------------------------------------------
#if TUSB_OPT_HOST_ENABLED
  #ifndef CFG_TUH_DEVICE_MAX
    #define CFG_TUH_DEVICE_MAX 1
  #endif

  #ifndef CFG_TUH_ENUMERATION_BUFSIZE
    #define CFG_TUH_ENUMERATION_BUFSIZE 256
  #endif
#endif // TUSB_OPT_HOST_ENABLED

//------------- CLASS -------------//

#ifndef CFG_TUH_HUB
#define CFG_TUH_HUB    0
#endif

#ifndef CFG_TUH_CDC
#define CFG_TUH_CDC    0
#endif

#ifndef CFG_TUH_HID
#define CFG_TUH_HID    0
#endif

#ifndef CFG_TUH_MIDI
#define CFG_TUH_MIDI   0
#endif

#ifndef CFG_TUH_MSC
#define CFG_TUH_MSC    0
#endif

#ifndef CFG_TUH_VENDOR
#define CFG_TUH_VENDOR 0
#endif

//--------------------------------------------------------------------+
// Port Specific
// TUP stand for TinyUSB Port (can be renamed)
//--------------------------------------------------------------------+

//------------- Unaligned Memory -------------//

// ARMv7+ (M3-M7, M23-M33) can access unaligned memory
#if (defined(__ARM_ARCH) && (__ARM_ARCH >= 7))
  #define TUP_ARCH_STRICT_ALIGN   0
#else
  #define TUP_ARCH_STRICT_ALIGN   1
#endif

// TUP_MCU_STRICT_ALIGN will overwrite TUP_ARCH_STRICT_ALIGN.
// In case TUP_MCU_STRICT_ALIGN = 1 and TUP_ARCH_STRICT_ALIGN =0, we will not reply on compiler
// to generate unaligned access code.
// LPC_IP3511 Highspeed cannot access unaligned memory on USB_RAM
#if TUD_OPT_HIGH_SPEED && (CFG_TUSB_MCU == OPT_MCU_LPC54XXX || CFG_TUSB_MCU == OPT_MCU_LPC55XX)
  #define TUP_MCU_STRICT_ALIGN   1
#else
  #define TUP_MCU_STRICT_ALIGN   0
#endif


//------------------------------------------------------------------
// Configuration Validation
//------------------------------------------------------------------
#if CFG_TUD_ENDPOINT0_SIZE > 64
  #error Control Endpoint Max Packet Size cannot be larger than 64
#endif

#endif /* _TUSB_OPTION_H_ */

/** @} */
