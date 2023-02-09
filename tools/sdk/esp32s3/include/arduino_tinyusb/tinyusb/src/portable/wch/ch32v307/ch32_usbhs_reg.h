#ifndef _USB_CH32_USBHS_REG_H
#define _USB_CH32_USBHS_REG_H

#include <ch32v30x.h>

/******************* GLOBAL ******************/

// USB CONTROL
#define USBHS_CONTROL_OFFSET 0x00
#define USBHS_DMA_EN         (1 << 0)
#define USBHS_ALL_CLR        (1 << 1)
#define USBHS_FORCE_RST      (1 << 2)
#define USBHS_INT_BUSY_EN    (1 << 3)
#define USBHS_DEV_PU_EN      (1 << 4)
#define USBHS_SPEED_MASK     (3 << 5)
#define USBHS_FULL_SPEED     (0 << 5)
#define USBHS_HIGH_SPEED     (1 << 5)
#define USBHS_LOW_SPEED      (2 << 5)
#define USBHS_HOST_MODE      (1 << 7)

// USB_INT_EN
#define USBHS_INT_EN_OFFSET 0x02
#define USBHS_BUS_RST_EN    (1 << 0)
#define USBHS_DETECT_EN     (1 << 0)
#define USBHS_TRANSFER_EN   (1 << 1)
#define USBHS_SUSPEND_EN    (1 << 2)
#define USBHS_SOF_ACT_EN    (1 << 3)
#define USBHS_FIFO_OV_EN    (1 << 4)
#define USBHS_SETUP_ACT_EN  (1 << 5)
#define USBHS_ISO_ACT_EN    (1 << 6)
#define USBHS_DEV_NAK_EN    (1 << 7)

// USB DEV AD
#define USBHS_DEV_AD_OFFSET 0x03
// USB FRAME_NO
#define USBHS_FRAME_NO_OFFSET 0x04
// USB SUSPEND
#define USBHS_SUSPEND_OFFSET    0x06
#define USBHS_DEV_REMOTE_WAKEUP (1 << 2)
#define USBHS_LINESTATE_MASK    (2 << 4) /* Read Only */

// RESERVED0

// USB SPEED TYPE
#define USBHS_SPEED_TYPE_OFFSET 0x08
#define USBSPEED_MASK           (0x03)

// USB_MIS_ST
#define USBHS_MIS_ST_OFFSET 0x09
#define USBHS_SPLIT_CAN     (1 << 0)
#define USBHS_ATTACH        (1 << 1)
#define USBHS_SUSPEND       (1 << 2)
#define USBHS_BUS_RESET     (1 << 3)
#define USBHS_R_FIFO_RDY    (1 << 4)
#define USBHS_SIE_FREE      (1 << 5)
#define USBHS_SOF_ACT       (1 << 6)
#define USBHS_SOF_PRES      (1 << 7)

// INT_FLAG
#define USBHS_INT_FLAG_OFFSET 0x0A
#define USBHS_BUS_RST_FLAG    (1 << 0)
#define USBHS_DETECT_FLAG     (1 << 0)
#define USBHS_TRANSFER_FLAG   (1 << 1)
#define USBHS_SUSPEND_FLAG    (1 << 2)
#define USBHS_HST_SOF_FLAG    (1 << 3)
#define USBHS_FIFO_OV_FLAG    (1 << 4)
#define USBHS_SETUP_FLAG      (1 << 5)
#define USBHS_ISO_ACT_FLAG    (1 << 6)

// INT_ST
#define USBHS_INT_ST_OFFSET  0x0B
#define USBHS_DEV_UIS_IS_NAK (1 << 7)
#define USBHS_DEV_UIS_TOG_OK (1 << 6)
#define MASK_UIS_TOKEN       (3 << 4)
#define MASK_UIS_ENDP        (0x0F)
#define MASK_UIS_H_RES       (0x0F)

#define USBHS_TOGGLE_OK (0x40)
#define USBHS_HOST_RES  (0x0f)

//USB_RX_LEN
#define USBHS_RX_LEN_OFFSET 0x0C
/******************* DEVICE ******************/

//UEP_CONFIG
#define USBHS_UEP_CONFIG_OFFSET 0x10
#define USBHS_EP0_T_EN          (1 << 0)
#define USBHS_EP0_R_EN          (1 << 16)

#define USBHS_EP1_T_EN (1 << 1)
#define USBHS_EP1_R_EN (1 << 17)

#define USBHS_EP2_T_EN (1 << 2)
#define USBHS_EP2_R_EN (1 << 18)

#define USBHS_EP3_T_EN (1 << 3)
#define USBHS_EP3_R_EN (1 << 19)

#define USBHS_EP4_T_EN (1 << 4)
#define USBHS_EP4_R_EN (1 << 20)

#define USBHS_EP5_T_EN (1 << 5)
#define USBHS_EP5_R_EN (1 << 21)

#define USBHS_EP6_T_EN (1 << 6)
#define USBHS_EP6_R_EN (1 << 22)

#define USBHS_EP7_T_EN (1 << 7)
#define USBHS_EP7_R_EN (1 << 23)

#define USBHS_EP8_T_EN (1 << 8)
#define USBHS_EP8_R_EN (1 << 24)

#define USBHS_EP9_T_EN (1 << 9)
#define USBHS_EP9_R_EN (1 << 25)

#define USBHS_EP10_T_EN (1 << 10)
#define USBHS_EP10_R_EN (1 << 26)

#define USBHS_EP11_T_EN (1 << 11)
#define USBHS_EP11_R_EN (1 << 27)

#define USBHS_EP12_T_EN (1 << 12)
#define USBHS_EP12_R_EN (1 << 28)

#define USBHS_EP13_T_EN (1 << 13)
#define USBHS_EP13_R_EN (1 << 29)

#define USBHS_EP14_T_EN (1 << 14)
#define USBHS_EP14_R_EN (1 << 30)

#define USBHS_EP15_T_EN (1 << 15)
#define USBHS_EP15_R_EN (1 << 31)

//UEP_TYPE
#define USBHS_UEP_TYPE_OFFSET 0x14
#define USBHS_EP0_T_TYP       (1 << 0)
#define USBHS_EP0_R_TYP       (1 << 16)

#define USBHS_EP1_T_TYP (1 << 1)
#define USBHS_EP1_R_TYP (1 << 17)

#define USBHS_EP2_T_TYP (1 << 2)
#define USBHS_EP2_R_TYP (1 << 18)

#define USBHS_EP3_T_TYP (1 << 3)
#define USBHS_EP3_R_TYP (1 << 19)

#define USBHS_EP4_T_TYP (1 << 4)
#define USBHS_EP4_R_TYP (1 << 20)

#define USBHS_EP5_T_TYP (1 << 5)
#define USBHS_EP5_R_TYP (1 << 21)

#define USBHS_EP6_T_TYP (1 << 6)
#define USBHS_EP6_R_TYP (1 << 22)

#define USBHS_EP7_T_TYP (1 << 7)
#define USBHS_EP7_R_TYP (1 << 23)

#define USBHS_EP8_T_TYP (1 << 8)
#define USBHS_EP8_R_TYP (1 << 24)

#define USBHS_EP9_T_TYP (1 << 8)
#define USBHS_EP9_R_TYP (1 << 25)

#define USBHS_EP10_T_TYP (1 << 10)
#define USBHS_EP10_R_TYP (1 << 26)

#define USBHS_EP11_T_TYP (1 << 11)
#define USBHS_EP11_R_TYP (1 << 27)

#define USBHS_EP12_T_TYP (1 << 12)
#define USBHS_EP12_R_TYP (1 << 28)

#define USBHS_EP13_T_TYP (1 << 13)
#define USBHS_EP13_R_TYP (1 << 29)

#define USBHS_EP14_T_TYP (1 << 14)
#define USBHS_EP14_R_TYP (1 << 30)

#define USBHS_EP15_T_TYP (1 << 15)
#define USBHS_EP15_R_TYP (1 << 31)

/* BUF_MOD UEP1~15 */
#define USBHS_BUF_MOD_OFFSET  0x18
#define USBHS_EP0_BUF_MOD     (1 << 0)
#define USBHS_EP0_ISO_BUF_MOD (1 << 16)

#define USBHS_EP1_BUF_MOD     (1 << 1)
#define USBHS_EP1_ISO_BUF_MOD (1 << 17)

#define USBHS_EP2_BUF_MOD     (1 << 2)
#define USBHS_EP2_ISO_BUF_MOD (1 << 18)

#define USBHS_EP3_BUF_MOD     (1 << 3)
#define USBHS_EP3_ISO_BUF_MOD (1 << 19)

#define USBHS_EP4_BUF_MOD     (1 << 4)
#define USBHS_EP4_ISO_BUF_MOD (1 << 20)

#define USBHS_EP5_BUF_MOD     (1 << 5)
#define USBHS_EP5_ISO_BUF_MOD (1 << 21)

#define USBHS_EP6_BUF_MOD     (1 << 6)
#define USBHS_EP6_ISO_BUF_MOD (1 << 22)

#define USBHS_EP7_BUF_MOD     (1 << 7)
#define USBHS_EP7_ISO_BUF_MOD (1 << 23)

#define USBHS_EP8_BUF_MOD     (1 << 8)
#define USBHS_EP8_ISO_BUF_MOD (1 << 24)

#define USBHS_EP9_BUF_MOD     (1 << 9)
#define USBHS_EP9_ISO_BUF_MOD (1 << 25)

#define USBHS_EP10_BUF_MOD     (1 << 10)
#define USBHS_EP10_ISO_BUF_MOD (1 << 26)

#define USBHS_EP11_BUF_MOD     (1 << 11)
#define USBHS_EP11_ISO_BUF_MOD (1 << 27)

#define USBHS_EP12_BUF_MOD     (1 << 12)
#define USBHS_EP12_ISO_BUF_MOD (1 << 28)

#define USBHS_EP13_BUF_MOD     (1 << 13)
#define USBHS_EP13_ISO_BUF_MOD (1 << 29)

#define USBHS_EP14_BUF_MOD     (1 << 14)
#define USBHS_EP14_ISO_BUF_MOD (1 << 30)

#define USBHS_EP15_BUF_MOD     (1 << 15)
#define USBHS_EP15_ISO_BUF_MOD (1 << 31)
//USBHS_EPn_T_EN  USBHS_EPn_R_EN  USBHS_EPn_BUF_MOD  Description: Arrange from low to high with UEPn_DMA as the starting address
//      0               0               x            The endpoint is disabled and the UEPn_*_DMA buffers are not used.
//      1               0               0            The first address of the receive (OUT) buffer is UEPn_RX_DMA
//      1               0               1            RB_UEPn_RX_TOG[0]=0, use buffer UEPn_RX_DMA RB_UEPn_RX_TOG[0]=1, use buffer UEPn_TX_DMA
//      0               1               0            The first address of the transmit (IN) buffer is UEPn_TX_DMA.
//      0               1               1            RB_UEPn_TX_TOG[0]=0, use buffer UEPn_TX_DMA RB_UEPn_TX_TOG[0]=1, use buffer UEPn_RX_DMA

/* USB0_DMA */
#define USBHS_UEP0_DMA_OFFSET(n) (0x1C) // endpoint 0 DMA buffer address

/* USBX_RX_DMA */
#define USBHS_UEPx_RX_DMA_OFFSET(n) (0x1C + 4 * (n)) // endpoint x DMA buffer address

#define USBHS_UEPx_TX_DMA_OFFSET(n) (0x58 + 4 * (n)) // endpoint x DMA buffer address

#define USBHS_UEPx_MAX_LEN_OFFSET(n) (0x98 + 4 * (n)) // endpoint x DMA buffer address

#define USBHS_UEPx_T_LEN_OFFSET(n)   (0xD8 + 4 * (n)) // endpoint x DMA buffer address
#define USBHS_UEPx_TX_CTRL_OFFSET(n) (0xD8 + 4 * (n) + 2) // endpoint x DMA buffer address
#define USBHS_UEPx_RX_CTRL_OFFSET(n) (0xD8 + 4 * (n) + 3) // endpoint x DMA buffer address

// UEPn_T_LEN
#define USBHS_EP_T_LEN_MASK (0x7FF)

//UEPn_TX_CTRL
#define USBHS_EP_T_RES_MASK  (3 << 0)
#define USBHS_EP_T_RES_ACK   (0 << 0)
#define USBHS_EP_T_RES_NYET  (1 << 0)
#define USBHS_EP_T_RES_NAK   (2 << 0)
#define USBHS_EP_T_RES_STALL (3 << 0)

#define USBHS_EP_T_TOG_MASK (3 << 3)
#define USBHS_EP_T_TOG_0    (0 << 3)
#define USBHS_EP_T_TOG_1    (1 << 3)
#define USBHS_EP_T_TOG_2    (2 << 3)
#define USBHS_EP_T_TOG_M    (3 << 3)

#define USBHS_EP_T_AUTOTOG (1 << 5)

//UEPn_RX_CTRL
#define USBHS_EP_R_RES_MASK  (3 << 0)
#define USBHS_EP_R_RES_ACK   (0 << 0)
#define USBHS_EP_R_RES_NYET  (1 << 0)
#define USBHS_EP_R_RES_NAK   (2 << 0)
#define USBHS_EP_R_RES_STALL (3 << 0)

#define USBHS_EP_R_TOG_MASK (3 << 3)
#define USBHS_EP_R_TOG_0    (0 << 3)
#define USBHS_EP_R_TOG_1    (1 << 3)
#define USBHS_EP_R_TOG_2    (2 << 3)
#define USBHS_EP_R_TOG_M    (3 << 3)

#define USBHS_EP_R_AUTOTOG (1 << 5)

#define USBHS_TOG_MATCH (1 << 6)

/******************* HOST ******************/
// USB HOST_CTRL
#define USBHS_SEND_BUS_RESET   (1 << 0)
#define USBHS_SEND_BUS_SUSPEND (1 << 1)
#define USBHS_SEND_BUS_RESUME  (1 << 2)
#define USBHS_REMOTE_WAKE      (1 << 3)
#define USBHS_PHY_SUSPENDM     (1 << 4)
#define USBHS_UH_SOFT_FREE     (1 << 6)
#define USBHS_SEND_SOF_EN      (1 << 7)

//UH_CONFIG
#define USBHS_HOST_TX_EN (1 << 3)
#define USBHS_HOST_RX_EN (1 << 18)

// HOST_EP_TYPE
#define USBHS_ENDP_TX_ISO (1 << 3)
#define USBHS_ENDP_RX_ISO (1 << (16 + 2))

// R32_UH_EP_PID
#define USBHS_HOST_MASK_TOKEN (0x0f)
#define USBHS_HOST_MASK_ENDP  (0x0f << 4)

//R8_UH_RX_CTRL
#define USBHS_EP_R_RES_MASK  (3 << 0)
#define USBHS_EP_R_RES_ACK   (0 << 0)
#define USBHS_EP_R_RES_NYET  (1 << 0)
#define USBHS_EP_R_RES_NAK   (2 << 0)
#define USBHS_EP_R_RES_STALL (3 << 0)

#define USBHS_UH_R_RES_NO   (1 << 2)
#define USBHS_UH_R_TOG_1    (1 << 3)
#define USBHS_UH_R_TOG_2    (2 << 3)
#define USBHS_UH_R_TOG_3    (3 << 3)
#define USBHS_UH_R_TOG_AUTO (1 << 5)
#define USBHS_UH_R_DATA_NO  (1 << 6)
//R8_UH_TX_CTRL
#define USBHS_UH_T_RES_MASK  (3 << 0)
#define USBHS_UH_T_RES_ACK   (0 << 0)
#define USBHS_UH_T_RES_NYET  (1 << 0)
#define USBHS_UH_T_RES_NAK   (2 << 0)
#define USBHS_UH_T_RES_STALL (3 << 0)

#define USBHS_UH_T_RES_NO   (1 << 2)
#define USBHS_UH_T_TOG_1    (1 << 3)
#define USBHS_UH_T_TOG_2    (2 << 3)
#define USBHS_UH_T_TOG_3    (3 << 3)
#define USBHS_UH_T_TOG_AUTO (1 << 5)
#define USBHS_UH_T_DATA_NO  (1 << 6)

// 00: OUT, 01:SOF, 10:IN, 11:SETUP
#define PID_OUT   0
#define PID_SOF   1
#define PID_IN    2
#define PID_SETUP 3

#endif
