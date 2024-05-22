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

#include "common/tusb_compiler.h"

// Version is release as major.minor.revision eg 1.0.0. though there could be notable APIs before a new release.
// For notable API changes within a release, we increase the build number.
#define TUSB_VERSION_MAJOR     0
#define TUSB_VERSION_MINOR     16
#define TUSB_VERSION_REVISION  0
#define TUSB_VERSION_BUILD     3

#define TUSB_VERSION_NUMBER    (TUSB_VERSION_MAJOR << 24 | TUSB_VERSION_MINOR << 16 | TUSB_VERSION_REVISION << 8 | TUSB_VERSION_BUILD)
#define TUSB_VERSION_STRING    TU_STRING(TUSB_VERSION_MAJOR) "." TU_STRING(TUSB_VERSION_MINOR) "." TU_STRING(TUSB_VERSION_REVISION)

//--------------------------------------------------------------------+
// Supported MCUs
// CFG_TUSB_MCU must be defined to one of following value
//--------------------------------------------------------------------+

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
#define OPT_MCU_LPC51               9 ///< NXP LPC51
#define OPT_MCU_LPC51UXX            OPT_MCU_LPC51 ///< NXP LPC51
#define OPT_MCU_LPC54              10 ///< NXP LPC54
#define OPT_MCU_LPC55              11 ///< NXP LPC55
// legacy naming
#define OPT_MCU_LPC54XXX           OPT_MCU_LPC54
#define OPT_MCU_LPC55XX            OPT_MCU_LPC55

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
#define OPT_MCU_STM32F0           300 ///< ST F0
#define OPT_MCU_STM32F1           301 ///< ST F1
#define OPT_MCU_STM32F2           302 ///< ST F2
#define OPT_MCU_STM32F3           303 ///< ST F3
#define OPT_MCU_STM32F4           304 ///< ST F4
#define OPT_MCU_STM32F7           305 ///< ST F7
#define OPT_MCU_STM32H7           306 ///< ST H7
#define OPT_MCU_STM32L1           308 ///< ST L1
#define OPT_MCU_STM32L0           307 ///< ST L0
#define OPT_MCU_STM32L4           309 ///< ST L4
#define OPT_MCU_STM32G0           310 ///< ST G0
#define OPT_MCU_STM32G4           311 ///< ST G4
#define OPT_MCU_STM32WB           312 ///< ST WB
#define OPT_MCU_STM32U5           313 ///< ST U5
#define OPT_MCU_STM32L5           314 ///< ST L5
#define OPT_MCU_STM32H5           315 ///< ST H5

// Sony
#define OPT_MCU_CXD56             400 ///< SONY CXD56

// TI
#define OPT_MCU_MSP430x5xx        500 ///< TI MSP430x5xx
#define OPT_MCU_MSP432E4          510 ///< TI MSP432E4xx
#define OPT_MCU_TM4C123           511 ///< TI Tiva-C 123x
#define OPT_MCU_TM4C129           512 ///< TI Tiva-C 129x

// ValentyUSB eptri
#define OPT_MCU_VALENTYUSB_EPTRI  600 ///< Fomu eptri config

// NXP iMX RT
#define OPT_MCU_MIMXRT1XXX        700                 ///< NXP iMX RT1xxx Series
#define OPT_MCU_MIMXRT10XX        OPT_MCU_MIMXRT1XXX  ///< RT10xx
#define OPT_MCU_MIMXRT11XX        OPT_MCU_MIMXRT1XXX  ///< RT11xx

// Nuvoton
#define OPT_MCU_NUC121            800
#define OPT_MCU_NUC126            801
#define OPT_MCU_NUC120            802
#define OPT_MCU_NUC505            803

// Espressif
#define OPT_MCU_ESP32S2           900 ///< Espressif ESP32-S2
#define OPT_MCU_ESP32S3           901 ///< Espressif ESP32-S3
#define OPT_MCU_ESP32             902 ///< Espressif ESP32 (for host max3421e)
#define OPT_MCU_ESP32C3           903 ///< Espressif ESP32-C3
#define OPT_MCU_ESP32C6           904 ///< Espressif ESP32-C6
#define OPT_MCU_ESP32C2           905 ///< Espressif ESP32-C2
#define OPT_MCU_ESP32H2           906 ///< Espressif ESP32-H2
#define TUP_MCU_ESPRESSIF         (CFG_TUSB_MCU >= 900 && CFG_TUSB_MCU < 1000) // check if Espressif MCU

// Dialog
#define OPT_MCU_DA1469X          1000 ///< Dialog Semiconductor DA1469x

// Raspberry Pi
#define OPT_MCU_RP2040           1100 ///< Raspberry Pi RP2040

// NXP Kinetis
#define OPT_MCU_KINETIS_KL       1200 ///< NXP KL series
#define OPT_MCU_KINETIS_K32L     1201 ///< NXP K32L series
#define OPT_MCU_KINETIS_K32      1201 ///< Alias to K32L
#define OPT_MCU_KINETIS_K        1202 ///< NXP K series

#define OPT_MCU_MKL25ZXX         1200 ///< Alias to KL (obsolete)
#define OPT_MCU_K32L2BXX         1201 ///< Alias to K32 (obsolete)

// Silabs
#define OPT_MCU_EFM32GG          1300 ///< Silabs EFM32GG

// Renesas RX
#define OPT_MCU_RX63X            1400 ///< Renesas RX63N/631
#define OPT_MCU_RX65X            1401 ///< Renesas RX65N/RX651
#define OPT_MCU_RX72N            1402 ///< Renesas RX72N
#define OPT_MCU_RAXXX            1403 ///< Renesas RAxxx families

// Mind Motion
#define OPT_MCU_MM32F327X        1500 ///< Mind Motion MM32F327

// GigaDevice
#define OPT_MCU_GD32VF103        1600 ///< GigaDevice GD32VF103

// Broadcom
#define OPT_MCU_BCM2711          1700 ///< Broadcom BCM2711
#define OPT_MCU_BCM2835          1701 ///< Broadcom BCM2835
#define OPT_MCU_BCM2837          1702 ///< Broadcom BCM2837

// Infineon
#define OPT_MCU_XMC4000          1800 ///< Infineon XMC4000

// PIC
#define OPT_MCU_PIC32MZ          1900 ///< MicroChip PIC32MZ family
#define OPT_MCU_PIC32MM          1901 ///< MicroChip PIC32MM family
#define OPT_MCU_PIC32MX          1902 ///< MicroChip PIC32MX family
#define OPT_MCU_PIC32MK          1903 ///< MicroChip PIC32MK family
#define OPT_MCU_PIC24            1910 ///< MicroChip PIC24 family
#define OPT_MCU_DSPIC33          1911 ///< MicroChip DSPIC33 family

// BridgeTek
#define OPT_MCU_FT90X            2000 ///< BridgeTek FT90x
#define OPT_MCU_FT93X            2001 ///< BridgeTek FT93x

// Allwinner
#define OPT_MCU_F1C100S          2100 ///< Allwinner F1C100s family

// WCH
#define OPT_MCU_CH32V307         2200 ///< WCH CH32V307
#define OPT_MCU_CH32F20X         2210 ///< WCH CH32F20x
#define OPT_MCU_CH32V20X         2220 ///< WCH CH32V20X


// NXP LPC MCX
#define OPT_MCU_MCXN9            2300  ///< NXP MCX N9 Series
#define OPT_MCU_MCXA15           2301  ///< NXP MCX A15 Series

// Check if configured MCU is one of listed
// Apply _TU_CHECK_MCU with || as separator to list of input
#define _TU_CHECK_MCU(_m)    (CFG_TUSB_MCU == _m)
#define TU_CHECK_MCU(...)    (TU_ARGS_APPLY(_TU_CHECK_MCU, ||, __VA_ARGS__))

//--------------------------------------------------------------------+
// Supported OS
//--------------------------------------------------------------------+

#define OPT_OS_NONE       1  ///< No RTOS
#define OPT_OS_FREERTOS   2  ///< FreeRTOS
#define OPT_OS_MYNEWT     3  ///< Mynewt OS
#define OPT_OS_CUSTOM     4  ///< Custom OS is implemented by application
#define OPT_OS_PICO       5  ///< Raspberry Pi Pico SDK
#define OPT_OS_RTTHREAD   6  ///< RT-Thread
#define OPT_OS_RTX4       7  ///< Keil RTX 4

// Allow to use command line to change the config name/location
#ifdef CFG_TUSB_CONFIG_FILE
  #include CFG_TUSB_CONFIG_FILE
#else
  #include "tusb_config.h"
#endif

#include "common/tusb_mcu.h"

//--------------------------------------------------------------------
// RootHub Mode Configuration
// CFG_TUSB_RHPORTx_MODE contains operation mode and speed for that port
//--------------------------------------------------------------------

// Low byte is operational mode
#define OPT_MODE_NONE           0x0000 ///< Disabled
#define OPT_MODE_DEVICE         0x0001 ///< Device Mode
#define OPT_MODE_HOST           0x0002 ///< Host Mode

// High byte is max operational speed (corresponding to tusb_speed_t)
#define OPT_MODE_DEFAULT_SPEED  0x0000 ///< Default (max) speed supported by MCU
#define OPT_MODE_LOW_SPEED      0x0100 ///< Low Speed
#define OPT_MODE_FULL_SPEED     0x0200 ///< Full Speed
#define OPT_MODE_HIGH_SPEED     0x0400 ///< High Speed
#define OPT_MODE_SPEED_MASK     0xff00

//------------- Roothub as Device -------------//

#if defined(CFG_TUSB_RHPORT0_MODE) && ((CFG_TUSB_RHPORT0_MODE) & OPT_MODE_DEVICE)
  #define TUD_RHPORT_MODE     (CFG_TUSB_RHPORT0_MODE)
  #define TUD_OPT_RHPORT      0
#elif defined(CFG_TUSB_RHPORT1_MODE) && ((CFG_TUSB_RHPORT1_MODE) & OPT_MODE_DEVICE)
  #define TUD_RHPORT_MODE     (CFG_TUSB_RHPORT1_MODE)
  #define TUD_OPT_RHPORT      1
#else
  #define TUD_RHPORT_MODE     OPT_MODE_NONE
#endif

#ifndef CFG_TUD_ENABLED
  // fallback to use CFG_TUSB_RHPORTx_MODE
  #define CFG_TUD_ENABLED     (TUD_RHPORT_MODE & OPT_MODE_DEVICE)
#endif

#ifndef CFG_TUD_MAX_SPEED
  // fallback to use CFG_TUSB_RHPORTx_MODE
  #define CFG_TUD_MAX_SPEED   (TUD_RHPORT_MODE & OPT_MODE_SPEED_MASK)
#endif

// For backward compatible
#define TUSB_OPT_DEVICE_ENABLED CFG_TUD_ENABLED

// highspeed support indicator
#define TUD_OPT_HIGH_SPEED    (CFG_TUD_MAX_SPEED ? (CFG_TUD_MAX_SPEED & OPT_MODE_HIGH_SPEED) : TUP_RHPORT_HIGHSPEED)

//------------- Roothub as Host -------------//

#if defined(CFG_TUSB_RHPORT0_MODE) && ((CFG_TUSB_RHPORT0_MODE) & OPT_MODE_HOST)
  #define TUH_RHPORT_MODE  (CFG_TUSB_RHPORT0_MODE)
  #define TUH_OPT_RHPORT   0
#elif defined(CFG_TUSB_RHPORT1_MODE) && ((CFG_TUSB_RHPORT1_MODE) & OPT_MODE_HOST)
  #define TUH_RHPORT_MODE  (CFG_TUSB_RHPORT1_MODE)
  #define TUH_OPT_RHPORT   1
#else
  #define TUH_RHPORT_MODE   OPT_MODE_NONE
#endif

#ifndef CFG_TUH_ENABLED
  // fallback to use CFG_TUSB_RHPORTx_MODE
  #define CFG_TUH_ENABLED     (TUH_RHPORT_MODE & OPT_MODE_HOST)
#endif

#ifndef CFG_TUH_MAX_SPEED
  // fallback to use CFG_TUSB_RHPORTx_MODE
  #define CFG_TUH_MAX_SPEED   (TUH_RHPORT_MODE & OPT_MODE_SPEED_MASK)
#endif

// For backward compatible
#define TUSB_OPT_HOST_ENABLED   CFG_TUH_ENABLED

// highspeed support indicator
#define TUH_OPT_HIGH_SPEED    (CFG_TUH_MAX_SPEED ? (CFG_TUH_MAX_SPEED & OPT_MODE_HIGH_SPEED) : TUP_RHPORT_HIGHSPEED)


//--------------------------------------------------------------------+
// TODO move later
//--------------------------------------------------------------------+

// TUP_MCU_STRICT_ALIGN will overwrite TUP_ARCH_STRICT_ALIGN.
// In case TUP_MCU_STRICT_ALIGN = 1 and TUP_ARCH_STRICT_ALIGN =0, we will not reply on compiler
// to generate unaligned access code.
// LPC_IP3511 Highspeed cannot access unaligned memory on USB_RAM
#if TUD_OPT_HIGH_SPEED && TU_CHECK_MCU(OPT_MCU_LPC54XXX, OPT_MCU_LPC55XX)
  #define TUP_MCU_STRICT_ALIGN   1
#else
  #define TUP_MCU_STRICT_ALIGN   0
#endif


//--------------------------------------------------------------------+
// Common Options (Default)
//--------------------------------------------------------------------+

// Debug enable to print out error message
#ifndef CFG_TUSB_DEBUG
  #define CFG_TUSB_DEBUG 0
#endif

// Level where CFG_TUSB_DEBUG must be at least for USBH is logged
#ifndef CFG_TUH_LOG_LEVEL
  #define CFG_TUH_LOG_LEVEL   2
#endif

// Level where CFG_TUSB_DEBUG must be at least for USBD is logged
#ifndef CFG_TUD_LOG_LEVEL
  #define CFG_TUD_LOG_LEVEL   2
#endif

// Memory section for placing buffer used for usb transferring. If MEM_SECTION is different for
// host and device use: CFG_TUD_MEM_SECTION, CFG_TUH_MEM_SECTION instead
#ifndef CFG_TUSB_MEM_SECTION
  #define CFG_TUSB_MEM_SECTION
#endif

// Alignment requirement of buffer used for usb transferring. if MEM_ALIGN is different for
// host and device controller use: CFG_TUD_MEM_ALIGN, CFG_TUH_MEM_ALIGN instead
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
// Device Options (Default)
//--------------------------------------------------------------------

// Attribute to place data in accessible RAM for device controller (default: CFG_TUSB_MEM_SECTION)
#ifndef CFG_TUD_MEM_SECTION
  #define CFG_TUD_MEM_SECTION     CFG_TUSB_MEM_SECTION
#endif

// Attribute to align memory for device controller (default: CFG_TUSB_MEM_ALIGN)
#ifndef CFG_TUD_MEM_ALIGN
  #define CFG_TUD_MEM_ALIGN       CFG_TUSB_MEM_ALIGN
#endif

#ifndef CFG_TUD_ENDPOINT0_SIZE
  #define CFG_TUD_ENDPOINT0_SIZE  64
#endif

#ifndef CFG_TUD_INTERFACE_MAX
  #define CFG_TUD_INTERFACE_MAX   16
#endif

// USB 2.0 compliance test mode support
#ifndef CFG_TUD_TEST_MODE
  #define CFG_TUD_TEST_MODE       0
#endif

//------------- Device Class Driver -------------//
#ifndef CFG_TUD_BTH
  #define CFG_TUD_BTH             0
#endif

#if CFG_TUD_BTH && !defined(CFG_TUD_BTH_ISO_ALT_COUNT)
#error CFG_TUD_BTH_ISO_ALT_COUNT must be defined to tell Bluetooth driver the number of ISO endpoints to use
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
// Host Options (Default)
//--------------------------------------------------------------------
#if CFG_TUH_ENABLED
  #ifndef CFG_TUH_DEVICE_MAX
    #define CFG_TUH_DEVICE_MAX 1
  #endif

  #ifndef CFG_TUH_ENUMERATION_BUFSIZE
    #define CFG_TUH_ENUMERATION_BUFSIZE 256
  #endif
#endif // CFG_TUH_ENABLED

// Attribute to place data in accessible RAM for host controller (default: CFG_TUSB_MEM_SECTION)
#ifndef CFG_TUH_MEM_SECTION
  #define CFG_TUH_MEM_SECTION   CFG_TUSB_MEM_SECTION
#endif

// Attribute to align memory for host controller
#ifndef CFG_TUH_MEM_ALIGN
  #define CFG_TUH_MEM_ALIGN     CFG_TUSB_MEM_ALIGN
#endif

//------------- CLASS -------------//

#ifndef CFG_TUH_HUB
  #define CFG_TUH_HUB    0
#endif

#ifndef CFG_TUH_CDC
  #define CFG_TUH_CDC    0
#endif

#ifndef CFG_TUH_CDC_FTDI
  // FTDI is not part of CDC class, only to re-use CDC driver API
  #define CFG_TUH_CDC_FTDI 0
#endif

#ifndef CFG_TUH_CDC_FTDI_VID_PID_LIST
  // List of product IDs that can use the FTDI CDC driver. 0x0403 is FTDI's VID
  #define CFG_TUH_CDC_FTDI_VID_PID_LIST \
    {0x0403, 0x6001}, {0x0403, 0x6006}, {0x0403, 0x6010}, {0x0403, 0x6011}, \
    {0x0403, 0x6014}, {0x0403, 0x6015}, {0x0403, 0x8372}, {0x0403, 0xFBFA}, \
    {0x0403, 0xCD18}
#endif

#ifndef CFG_TUH_CDC_CP210X
  // CP210X is not part of CDC class, only to re-use CDC driver API
  #define CFG_TUH_CDC_CP210X 0
#endif

#ifndef CFG_TUH_CDC_CP210X_VID_PID_LIST
  // List of product IDs that can use the CP210X CDC driver. 0x10C4 is Silicon Labs' VID
  #define CFG_TUH_CDC_CP210X_VID_PID_LIST \
    {0x10C4, 0xEA60}, {0x10C4, 0xEA70}
#endif

#ifndef CFG_TUH_CDC_CH34X
  // CH34X is not part of CDC class, only to re-use CDC driver API
  #define CFG_TUH_CDC_CH34X 0
#endif

#ifndef CFG_TUH_CDC_CH34X_VID_PID_LIST
  // List of product IDs that can use the CH34X CDC driver
  #define CFG_TUH_CDC_CH34X_VID_PID_LIST \
    { 0x1a86, 0x5523 }, /* ch341 chip */ \
    { 0x1a86, 0x7522 }, /* ch340k chip */ \
    { 0x1a86, 0x7523 }, /* ch340 chip */ \
    { 0x1a86, 0xe523 }, /* ch330 chip */ \
    { 0x4348, 0x5523 }, /* ch340 custom chip */ \
    { 0x2184, 0x0057 }, /* overtaken from Linux Kernel driver /drivers/usb/serial/ch341.c */ \
    { 0x9986, 0x7523 }  /* overtaken from Linux Kernel driver /drivers/usb/serial/ch341.c */
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

#ifndef CFG_TUH_API_EDPT_XFER
  #define CFG_TUH_API_EDPT_XFER 0
#endif

// Enable PIO-USB software host controller
#ifndef CFG_TUH_RPI_PIO_USB
  #define CFG_TUH_RPI_PIO_USB 0
#endif

#ifndef CFG_TUD_RPI_PIO_USB
  #define CFG_TUD_RPI_PIO_USB 0
#endif

// MAX3421 Host controller option
#ifndef CFG_TUH_MAX3421
  #define CFG_TUH_MAX3421  0
#endif

//--------------------------------------------------------------------+
// TypeC Options (Default)
//--------------------------------------------------------------------+

#ifndef CFG_TUC_ENABLED
#define CFG_TUC_ENABLED 0

#define tuc_int_handler(_p)
#endif

//------------------------------------------------------------------
// Configuration Validation
//------------------------------------------------------------------
#if CFG_TUD_ENDPOINT0_SIZE > 64
  #error Control Endpoint Max Packet Size cannot be larger than 64
#endif

// To avoid GCC compiler warnings when -pedantic option is used (strict ISO C)
typedef int make_iso_compilers_happy;

#endif /* _TUSB_OPTION_H_ */

/** @} */
