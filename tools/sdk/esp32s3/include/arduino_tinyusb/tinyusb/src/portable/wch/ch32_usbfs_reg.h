/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Matthew Tran
 * Copyright (c) 2024 hathach
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

#ifndef USB_CH32_USBFS_REG_H
#define USB_CH32_USBFS_REG_H

// https://github.com/openwch/ch32v307/pull/90
// https://github.com/openwch/ch32v20x/pull/12
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#endif

#if CFG_TUSB_MCU == OPT_MCU_CH32F20X
  #include <ch32f20x.h>
#elif CFG_TUSB_MCU == OPT_MCU_CH32V103
  #include <ch32v10x.h>
  typedef struct
  {
    __IO uint8_t  BASE_CTRL;
    __IO uint8_t  UDEV_CTRL;
    __IO uint8_t  INT_EN;
    __IO uint8_t  DEV_ADDR;
    __IO uint8_t  Reserve0;
    __IO uint8_t  MIS_ST;
    __IO uint8_t  INT_FG;
    __IO uint8_t  INT_ST;
    __IO uint32_t RX_LEN;
    __IO uint8_t  UEP4_1_MOD;
    __IO uint8_t  UEP2_3_MOD;
    __IO uint8_t  UEP5_6_MOD;
    __IO uint8_t  UEP7_MOD;
    __IO uint32_t UEP0_DMA;
    __IO uint32_t UEP1_DMA;
    __IO uint32_t UEP2_DMA;
    __IO uint32_t UEP3_DMA;
    __IO uint32_t UEP4_DMA;
    __IO uint32_t UEP5_DMA;
    __IO uint32_t UEP6_DMA;
    __IO uint32_t UEP7_DMA;
    __IO uint16_t UEP0_TX_LEN;
    __IO uint8_t  UEP0_TX_CTRL;
    __IO uint8_t  UEP0_RX_CTRL;
    __IO uint16_t UEP1_TX_LEN;
    __IO uint8_t  UEP1_TX_CTRL;
    __IO uint8_t  UEP1_RX_CTRL;
    __IO uint16_t UEP2_TX_LEN;
    __IO uint8_t  UEP2_TX_CTRL;
    __IO uint8_t  UEP2_RX_CTRL;
    __IO uint16_t UEP3_TX_LEN;
    __IO uint8_t  UEP3_TX_CTRL;
    __IO uint8_t  UEP3_RX_CTRL;
    __IO uint16_t UEP4_TX_LEN;
    __IO uint8_t  UEP4_TX_CTRL;
    __IO uint8_t  UEP4_RX_CTRL;
    __IO uint16_t UEP5_TX_LEN;
    __IO uint8_t  UEP5_TX_CTRL;
    __IO uint8_t  UEP5_RX_CTRL;
    __IO uint16_t UEP6_TX_LEN;
    __IO uint8_t  UEP6_TX_CTRL;
    __IO uint8_t  UEP6_RX_CTRL;
    __IO uint16_t UEP7_TX_LEN;
    __IO uint8_t  UEP7_TX_CTRL;
    __IO uint8_t  UEP7_RX_CTRL;
    __IO uint32_t Reserve1;
    __IO uint32_t OTG_CR;
    __IO uint32_t OTG_SR;
  } USBOTG_FS_TypeDef;

  #define USBOTG_FS ((USBOTG_FS_TypeDef *) 0x40023400)
#elif CFG_TUSB_MCU == OPT_MCU_CH32V20X
  #include <ch32v20x.h>
#elif CFG_TUSB_MCU == OPT_MCU_CH32V307
  #include <ch32v30x.h>
  #define USBHD_IRQn OTG_FS_IRQn
#endif

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

// CTRL
#define USBFS_CTRL_DMA_EN    (1 << 0)
#define USBFS_CTRL_CLR_ALL   (1 << 1)
#define USBFS_CTRL_RESET_SIE (1 << 2)
#define USBFS_CTRL_INT_BUSY  (1 << 3)
#define USBFS_CTRL_SYS_CTRL  (1 << 4)
#define USBFS_CTRL_DEV_PUEN  (1 << 5)
#define USBFS_CTRL_LOW_SPEED (1 << 6)
#define USBFS_CTRL_HOST_MODE (1 << 7)

// INT_EN
#define USBFS_INT_EN_BUS_RST  (1 << 0)
#define USBFS_INT_EN_DETECT   (1 << 0)
#define USBFS_INT_EN_TRANSFER (1 << 1)
#define USBFS_INT_EN_SUSPEND  (1 << 2)
#define USBFS_INT_EN_HST_SOF  (1 << 3)
#define USBFS_INT_EN_FIFO_OV  (1 << 4)
#define USBFS_INT_EN_DEV_NAK  (1 << 6)
#define USBFS_INT_EN_DEV_SOF  (1 << 7)

// INT_FG
#define USBFS_INT_FG_BUS_RST  (1 << 0)
#define USBFS_INT_FG_DETECT   (1 << 0)
#define USBFS_INT_FG_TRANSFER (1 << 1)
#define USBFS_INT_FG_SUSPEND  (1 << 2)
#define USBFS_INT_FG_HST_SOF  (1 << 3)
#define USBFS_INT_FG_FIFO_OV  (1 << 4)
#define USBFS_INT_FG_SIE_FREE (1 << 5)
#define USBFS_INT_FG_TOG_OK   (1 << 6)
#define USBFS_INT_FG_IS_NAK   (1 << 7)

// INT_ST
#define USBFS_INT_ST_MASK_UIS_ENDP(x)  (((x) >> 0) & 0x0F)
#define USBFS_INT_ST_MASK_UIS_TOKEN(x) (((x) >> 4) & 0x03)

// UDEV_CTRL
#define USBFS_UDEV_CTRL_PORT_EN   (1 << 0)
#define USBFS_UDEV_CTRL_GP_BIT    (1 << 1)
#define USBFS_UDEV_CTRL_LOW_SPEED (1 << 2)
#define USBFS_UDEV_CTRL_DM_PIN    (1 << 4)
#define USBFS_UDEV_CTRL_DP_PIN    (1 << 5)
#define USBFS_UDEV_CTRL_PD_DIS    (1 << 7)

// TX_CTRL
#define USBFS_EP_T_RES_MASK (3 << 0)
#define USBFS_EP_T_TOG      (1 << 2)
#define USBFS_EP_T_AUTO_TOG (1 << 3)

#define USBFS_EP_T_RES_ACK   (0 << 0)
#define USBFS_EP_T_RES_NYET  (1 << 0)
#define USBFS_EP_T_RES_NAK   (2 << 0)
#define USBFS_EP_T_RES_STALL (3 << 0)

// RX_CTRL
#define USBFS_EP_R_RES_MASK (3 << 0)
#define USBFS_EP_R_TOG      (1 << 2)
#define USBFS_EP_R_AUTO_TOG (1 << 3)

#define USBFS_EP_R_RES_ACK   (0 << 0)
#define USBFS_EP_R_RES_NYET  (1 << 0)
#define USBFS_EP_R_RES_NAK   (2 << 0)
#define USBFS_EP_R_RES_STALL (3 << 0)

// token PID
#define PID_OUT   0
#define PID_SOF   1
#define PID_IN    2
#define PID_SETUP 3

#endif // USB_CH32_USBFS_REG_H
