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

#ifndef COMMON_TRANSDIMENSION_H_
#define COMMON_TRANSDIMENSION_H_

#ifdef __cplusplus
 extern "C" {
#endif

// USBCMD
enum {
  USBCMD_RUN_STOP         = TU_BIT(0),
  USBCMD_RESET            = TU_BIT(1),
  USBCMD_SETUP_TRIPWIRE   = TU_BIT(13),
  USBCMD_ADD_QTD_TRIPWIRE = TU_BIT(14)  ///< This bit is used as a semaphore to ensure the to proper addition of a new dTD to an active (primed) endpoint’s linked list. This bit is set and cleared by software during the process of adding a new dTD
// Interrupt Threshold bit 23:16
};

// PORTSC1
#define PORTSC1_PORT_SPEED_POS    26

enum {
  PORTSC1_CURRENT_CONNECT_STATUS = TU_BIT(0),
  PORTSC1_FORCE_PORT_RESUME      = TU_BIT(6),
  PORTSC1_SUSPEND                = TU_BIT(7),
  PORTSC1_FORCE_FULL_SPEED       = TU_BIT(24),
  PORTSC1_PORT_SPEED             = TU_BIT(26) | TU_BIT(27)
};

// OTGSC
enum {
  OTGSC_VBUS_DISCHARGE          = TU_BIT(0),
  OTGSC_VBUS_CHARGE             = TU_BIT(1),
//  OTGSC_HWASSIST_AUTORESET    = TU_BIT(2),
  OTGSC_OTG_TERMINATION         = TU_BIT(3), ///< Must set to 1 when OTG go to device mode
  OTGSC_DATA_PULSING            = TU_BIT(4),
  OTGSC_ID_PULLUP               = TU_BIT(5),
//  OTGSC_HWASSIT_DATA_PULSE    = TU_BIT(6),
//  OTGSC_HWASSIT_BDIS_ACONN    = TU_BIT(7),
  OTGSC_ID                      = TU_BIT(8), ///< 0 = A device, 1 = B Device
  OTGSC_A_VBUS_VALID            = TU_BIT(9),
  OTGSC_A_SESSION_VALID         = TU_BIT(10),
  OTGSC_B_SESSION_VALID         = TU_BIT(11),
  OTGSC_B_SESSION_END           = TU_BIT(12),
  OTGSC_1MS_TOGGLE              = TU_BIT(13),
  OTGSC_DATA_BUS_PULSING_STATUS = TU_BIT(14),
};

// USBMode
enum {
  USBMODE_CM_DEVICE = 2,
  USBMODE_CM_HOST   = 3,

  USBMODE_SLOM = TU_BIT(3),
  USBMODE_SDIS = TU_BIT(4),

  USBMODE_VBUS_POWER_SELECT = TU_BIT(5), // Need to be enabled for LPC18XX/43XX in host mode
};

// Device Registers
typedef struct
{
  //------------- ID + HW Parameter Registers-------------//
  __I  uint32_t TU_RESERVED[64]; ///< For iMX RT10xx, but not used by LPC18XX/LPC43XX

  //------------- Capability Registers-------------//
  __I  uint8_t  CAPLENGTH;       ///< Capability Registers Length
  __I  uint8_t  TU_RESERVED[1];
  __I  uint16_t HCIVERSION;      ///< Host Controller Interface Version

  __I  uint32_t HCSPARAMS;       ///< Host Controller Structural Parameters
  __I  uint32_t HCCPARAMS;       ///< Host Controller Capability Parameters
  __I  uint32_t TU_RESERVED[5];

  __I  uint16_t DCIVERSION;      ///< Device Controller Interface Version
  __I  uint8_t  TU_RESERVED[2];

  __I  uint32_t DCCPARAMS;       ///< Device Controller Capability Parameters
  __I  uint32_t TU_RESERVED[6];

  //------------- Operational Registers -------------//
  __IO uint32_t USBCMD;          ///< USB Command Register
  __IO uint32_t USBSTS;          ///< USB Status Register
  __IO uint32_t USBINTR;         ///< Interrupt Enable Register
  __IO uint32_t FRINDEX;         ///< USB Frame Index
  __I  uint32_t TU_RESERVED;
  __IO uint32_t DEVICEADDR;      ///< Device Address
  __IO uint32_t ENDPTLISTADDR;   ///< Endpoint List Address
  __I  uint32_t TU_RESERVED;
  __IO uint32_t BURSTSIZE;       ///< Programmable Burst Size
  __IO uint32_t TXFILLTUNING;    ///< TX FIFO Fill Tuning
       uint32_t TU_RESERVED[4];
  __IO uint32_t ENDPTNAK;        ///< Endpoint NAK
  __IO uint32_t ENDPTNAKEN;      ///< Endpoint NAK Enable
  __I  uint32_t TU_RESERVED;
  __IO uint32_t PORTSC1;         ///< Port Status & Control
  __I  uint32_t TU_RESERVED[7];
  __IO uint32_t OTGSC;           ///< On-The-Go Status & control
  __IO uint32_t USBMODE;         ///< USB Device Mode
  __IO uint32_t ENDPTSETUPSTAT;  ///< Endpoint Setup Status
  __IO uint32_t ENDPTPRIME;      ///< Endpoint Prime
  __IO uint32_t ENDPTFLUSH;      ///< Endpoint Flush
  __I  uint32_t ENDPTSTAT;       ///< Endpoint Status
  __IO uint32_t ENDPTCOMPLETE;   ///< Endpoint Complete
  __IO uint32_t ENDPTCTRL[8];    ///< Endpoint Control 0 - 7
} dcd_registers_t, hcd_registers_t;

#ifdef __cplusplus
 }
#endif

#endif /* COMMON_TRANSDIMENSION_H_ */
