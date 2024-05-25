/*
* The MIT License (MIT)
 *
 * Copyright (c) 2024, hathach (tinyusb.org)
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
 */
/** <h2><center>&copy; Copyright (c) 2016 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  */

#ifndef TUSB_FSDEV_CH32_H
#define TUSB_FSDEV_CH32_H

#include "common/tusb_compiler.h"

#if CFG_TUSB_MCU == OPT_MCU_CH32V20X
  #include <ch32v20x.h>

#elif CFG_TUSB_MCU == OPT_MCU_CH32F20X
  #include <ch32f20x.h>
#endif

#define FSDEV_PMA_SIZE (512u)

// volatile 32-bit aligned
#define _va32     volatile TU_ATTR_ALIGNED(4)

typedef struct {
  _va32 uint16_t EP0R;            // 00: USB Endpoint 0 register
  _va32 uint16_t EP1R;            // 04: USB Endpoint 1 register
  _va32 uint16_t EP2R;            // 08: USB Endpoint 2 register
  _va32 uint16_t EP3R;            // 0C: USB Endpoint 3 register
  _va32 uint16_t EP4R;            // 10: USB Endpoint 4 register
  _va32 uint16_t EP5R;            // 14: USB Endpoint 5 register
  _va32 uint16_t EP6R;            // 18: USB Endpoint 6 register
  _va32 uint16_t EP7R;            // 1C: USB Endpoint 7 register
  _va32 uint16_t RESERVED7[16];   // Reserved
  _va32 uint16_t CNTR;            // 40: Control register
  _va32 uint16_t ISTR;            // 44: Interrupt status register
  _va32 uint16_t FNR;             // 48: Frame number register
  _va32 uint16_t DADDR;           // 4C: Device address register
  _va32 uint16_t BTABLE;          // 50: Buffer Table address register
} USB_TypeDef;

TU_VERIFY_STATIC(sizeof(USB_TypeDef) == 0x54, "Size is not correct");
TU_VERIFY_STATIC(offsetof(USB_TypeDef, CNTR) == 0x40, "Wrong offset");

#define USB_BASE            (APB1PERIPH_BASE + 0x00005C00UL) /*!< USB_IP Peripheral Registers base address */
#define USB_PMAADDR         (APB1PERIPH_BASE + 0x00006000UL) /*!< USB_IP Packet Memory Area base address */
#define USB                 ((USB_TypeDef *)USB_BASE)

/******************************************************************************/
/*                                                                            */
/*                         USB Device General registers                       */
/*                                                                            */
/******************************************************************************/
#define USB_CNTR                             (USB_BASE + 0x40U)             /*!< Control register */
#define USB_ISTR                             (USB_BASE + 0x44U)             /*!< Interrupt status register */
#define USB_FNR                              (USB_BASE + 0x48U)             /*!< Frame number register */
#define USB_DADDR                            (USB_BASE + 0x4CU)             /*!< Device address register */
#define USB_BTABLE                           (USB_BASE + 0x50U)             /*!< Buffer Table address register */

/****************************  ISTR interrupt events  *************************/
#define USB_ISTR_CTR                         ((uint16_t)0x8000U)               /*!< Correct TRansfer (clear-only bit) */
#define USB_ISTR_PMAOVR                      ((uint16_t)0x4000U)               /*!< DMA OVeR/underrun (clear-only bit) */
#define USB_ISTR_ERR                         ((uint16_t)0x2000U)               /*!< ERRor (clear-only bit) */
#define USB_ISTR_WKUP                        ((uint16_t)0x1000U)               /*!< WaKe UP (clear-only bit) */
#define USB_ISTR_SUSP                        ((uint16_t)0x0800U)               /*!< SUSPend (clear-only bit) */
#define USB_ISTR_RESET                       ((uint16_t)0x0400U)               /*!< RESET (clear-only bit) */
#define USB_ISTR_SOF                         ((uint16_t)0x0200U)               /*!< Start Of Frame (clear-only bit) */
#define USB_ISTR_ESOF                        ((uint16_t)0x0100U)               /*!< Expected Start Of Frame (clear-only bit) */
#define USB_ISTR_DIR                         ((uint16_t)0x0010U)               /*!< DIRection of transaction (read-only bit)  */
#define USB_ISTR_EP_ID                       ((uint16_t)0x000FU)               /*!< EndPoint IDentifier (read-only bit)  */

/* Legacy defines */
#define USB_ISTR_PMAOVRM USB_ISTR_PMAOVR

#define USB_CLR_CTR                          (~USB_ISTR_CTR)             /*!< clear Correct TRansfer bit */
#define USB_CLR_PMAOVR                       (~USB_ISTR_PMAOVR)          /*!< clear DMA OVeR/underrun bit*/
#define USB_CLR_ERR                          (~USB_ISTR_ERR)             /*!< clear ERRor bit */
#define USB_CLR_WKUP                         (~USB_ISTR_WKUP)            /*!< clear WaKe UP bit */
#define USB_CLR_SUSP                         (~USB_ISTR_SUSP)            /*!< clear SUSPend bit */
#define USB_CLR_RESET                        (~USB_ISTR_RESET)           /*!< clear RESET bit */
#define USB_CLR_SOF                          (~USB_ISTR_SOF)             /*!< clear Start Of Frame bit */
#define USB_CLR_ESOF                         (~USB_ISTR_ESOF)            /*!< clear Expected Start Of Frame bit */

/* Legacy defines */
#define USB_CLR_PMAOVRM USB_CLR_PMAOVR

/*************************  CNTR control register bits definitions  ***********/
#define USB_CNTR_CTRM                        ((uint16_t)0x8000U)               /*!< Correct TRansfer Mask */
#define USB_CNTR_PMAOVR                      ((uint16_t)0x4000U)               /*!< DMA OVeR/underrun Mask */
#define USB_CNTR_ERRM                        ((uint16_t)0x2000U)               /*!< ERRor Mask */
#define USB_CNTR_WKUPM                       ((uint16_t)0x1000U)               /*!< WaKe UP Mask */
#define USB_CNTR_SUSPM                       ((uint16_t)0x0800U)               /*!< SUSPend Mask */
#define USB_CNTR_RESETM                      ((uint16_t)0x0400U)               /*!< RESET Mask   */
#define USB_CNTR_SOFM                        ((uint16_t)0x0200U)               /*!< Start Of Frame Mask */
#define USB_CNTR_ESOFM                       ((uint16_t)0x0100U)               /*!< Expected Start Of Frame Mask */
#define USB_CNTR_RESUME                      ((uint16_t)0x0010U)               /*!< RESUME request */
#define USB_CNTR_FSUSP                       ((uint16_t)0x0008U)               /*!< Force SUSPend */
#define USB_CNTR_LPMODE                      ((uint16_t)0x0004U)               /*!< Low-power MODE */
#define USB_CNTR_PDWN                        ((uint16_t)0x0002U)               /*!< Power DoWN */
#define USB_CNTR_FRES                        ((uint16_t)0x0001U)               /*!< Force USB RESet */

/* Legacy defines */
#define USB_CNTR_PMAOVRM USB_CNTR_PMAOVR
#define USB_CNTR_LP_MODE USB_CNTR_LPMODE

/********************  FNR Frame Number Register bit definitions   ************/
#define USB_FNR_RXDP                         ((uint16_t)0x8000U)               /*!< status of D+ data line */
#define USB_FNR_RXDM                         ((uint16_t)0x4000U)               /*!< status of D- data line */
#define USB_FNR_LCK                          ((uint16_t)0x2000U)               /*!< LoCKed */
#define USB_FNR_LSOF                         ((uint16_t)0x1800U)               /*!< Lost SOF */
#define USB_FNR_FN                           ((uint16_t)0x07FFU)               /*!< Frame Number */

/********************  DADDR Device ADDRess bit definitions    ****************/
#define USB_DADDR_EF                         ((uint8_t)0x80U)                  /*!< USB device address Enable Function */
#define USB_DADDR_ADD                        ((uint8_t)0x7FU)                  /*!< USB device address */

/******************************  Endpoint register    *************************/
#define USB_EP0R                             USB_BASE                    /*!< endpoint 0 register address */
#define USB_EP1R                             (USB_BASE + 0x04U)           /*!< endpoint 1 register address */
#define USB_EP2R                             (USB_BASE + 0x08U)           /*!< endpoint 2 register address */
#define USB_EP3R                             (USB_BASE + 0x0CU)           /*!< endpoint 3 register address */
#define USB_EP4R                             (USB_BASE + 0x10U)           /*!< endpoint 4 register address */
#define USB_EP5R                             (USB_BASE + 0x14U)           /*!< endpoint 5 register address */
#define USB_EP6R                             (USB_BASE + 0x18U)           /*!< endpoint 6 register address */
#define USB_EP7R                             (USB_BASE + 0x1CU)           /*!< endpoint 7 register address */
/* bit positions */
#define USB_EP_CTR_RX                        ((uint16_t)0x8000U)               /*!<  EndPoint Correct TRansfer RX */
#define USB_EP_DTOG_RX                       ((uint16_t)0x4000U)               /*!<  EndPoint Data TOGGLE RX */
#define USB_EPRX_STAT                        ((uint16_t)0x3000U)               /*!<  EndPoint RX STATus bit field */
#define USB_EP_SETUP                         ((uint16_t)0x0800U)               /*!<  EndPoint SETUP */
#define USB_EP_T_FIELD                       ((uint16_t)0x0600U)               /*!<  EndPoint TYPE */
#define USB_EP_KIND                          ((uint16_t)0x0100U)               /*!<  EndPoint KIND */
#define USB_EP_CTR_TX                        ((uint16_t)0x0080U)               /*!<  EndPoint Correct TRansfer TX */
#define USB_EP_DTOG_TX                       ((uint16_t)0x0040U)               /*!<  EndPoint Data TOGGLE TX */
#define USB_EPTX_STAT                        ((uint16_t)0x0030U)               /*!<  EndPoint TX STATus bit field */
#define USB_EPADDR_FIELD                     ((uint16_t)0x000FU)               /*!<  EndPoint ADDRess FIELD */

/* EndPoint REGister MASK (no toggle fields) */
#define USB_EPREG_MASK     (USB_EP_CTR_RX|USB_EP_SETUP|USB_EP_T_FIELD|USB_EP_KIND|USB_EP_CTR_TX|USB_EPADDR_FIELD)
                                                                               /*!< EP_TYPE[1:0] EndPoint TYPE */
#define USB_EP_TYPE_MASK                     ((uint16_t)0x0600U)               /*!< EndPoint TYPE Mask */
#define USB_EP_BULK                          ((uint16_t)0x0000U)               /*!< EndPoint BULK */
#define USB_EP_CONTROL                       ((uint16_t)0x0200U)               /*!< EndPoint CONTROL */
#define USB_EP_ISOCHRONOUS                   ((uint16_t)0x0400U)               /*!< EndPoint ISOCHRONOUS */
#define USB_EP_INTERRUPT                     ((uint16_t)0x0600U)               /*!< EndPoint INTERRUPT */
#define USB_EP_T_MASK                        ((uint16_t) ~USB_EP_T_FIELD & USB_EPREG_MASK)

#define USB_EPKIND_MASK                      ((uint16_t) ~USB_EP_KIND & USB_EPREG_MASK)            /*!< EP_KIND EndPoint KIND */
                                                                               /*!< STAT_TX[1:0] STATus for TX transfer */
#define USB_EP_TX_DIS                        ((uint16_t)0x0000U)               /*!< EndPoint TX DISabled */
#define USB_EP_TX_STALL                      ((uint16_t)0x0010U)               /*!< EndPoint TX STALLed */
#define USB_EP_TX_NAK                        ((uint16_t)0x0020U)               /*!< EndPoint TX NAKed */
#define USB_EP_TX_VALID                      ((uint16_t)0x0030U)               /*!< EndPoint TX VALID */
#define USB_EPTX_DTOG1                       ((uint16_t)0x0010U)               /*!< EndPoint TX Data TOGgle bit1 */
#define USB_EPTX_DTOG2                       ((uint16_t)0x0020U)               /*!< EndPoint TX Data TOGgle bit2 */
#define USB_EPTX_DTOGMASK  (USB_EPTX_STAT|USB_EPREG_MASK)
                                                                               /*!< STAT_RX[1:0] STATus for RX transfer */
#define USB_EP_RX_DIS                        ((uint16_t)0x0000U)               /*!< EndPoint RX DISabled */
#define USB_EP_RX_STALL                      ((uint16_t)0x1000U)               /*!< EndPoint RX STALLed */
#define USB_EP_RX_NAK                        ((uint16_t)0x2000U)               /*!< EndPoint RX NAKed */
#define USB_EP_RX_VALID                      ((uint16_t)0x3000U)               /*!< EndPoint RX VALID */
#define USB_EPRX_DTOG1                       ((uint16_t)0x1000U)               /*!< EndPoint RX Data TOGgle bit1 */
#define USB_EPRX_DTOG2                       ((uint16_t)0x2000U)               /*!< EndPoint RX Data TOGgle bit1 */
#define USB_EPRX_DTOGMASK  (USB_EPRX_STAT|USB_EPREG_MASK)


//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

#if CFG_TUSB_MCU == OPT_MCU_CH32V20X
static const IRQn_Type fsdev_irq[] = {
  USB_HP_CAN1_TX_IRQn,
  USB_LP_CAN1_RX0_IRQn,
  USBWakeUp_IRQn
};
enum { FSDEV_IRQ_NUM = TU_ARRAY_SIZE(fsdev_irq) };
#else
  #error "Unsupported MCU"
#endif

void dcd_int_enable(uint8_t rhport) {
  (void)rhport;
  for(uint8_t i=0; i < FSDEV_IRQ_NUM; i++) {
    NVIC_EnableIRQ(fsdev_irq[i]);
  }
}

void dcd_int_disable(uint8_t rhport) {
  (void)rhport;
  for(uint8_t i=0; i < FSDEV_IRQ_NUM; i++) {
    NVIC_DisableIRQ(fsdev_irq[i]);
  }
}

void dcd_disconnect(uint8_t rhport) {
  (void) rhport;
  EXTEN->EXTEN_CTR &= ~EXTEN_USBD_PU_EN;
}

void dcd_connect(uint8_t rhport) {
  (void) rhport;
  EXTEN->EXTEN_CTR |= EXTEN_USBD_PU_EN;
}

#endif
