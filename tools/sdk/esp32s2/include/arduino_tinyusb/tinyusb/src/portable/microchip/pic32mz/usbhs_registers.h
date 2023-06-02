/*******************************************************************************
* Copyright (C) 2019 Microchip Technology Inc. and its subsidiaries.
*
* Subject to your compliance with these terms, you may use Microchip software
* and any derivatives exclusively with Microchip products. It is your
* responsibility to comply with third party license terms applicable to your
* use of third party software (including open source software) that may
* accompany Microchip software.
*
* THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
* EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED
* WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A
* PARTICULAR PURPOSE.
*
* IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
* INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
* WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS
* BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO THE
* FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN
* ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
* THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
*******************************************************************************/
/*******************************************************************************
  USBHS Peripheral Library Register Definitions 

  File Name:
    usbhs_registers.h

  Summary:
    USBHS PLIB Register Definitions

  Description:
    This file contains the constants and definitions which are required by the
    the USBHS library.
*******************************************************************************/

#ifndef __USBHS_REGISTERS_H__
#define __USBHS_REGISTERS_H__

#include <p32xxxx.h>
#include <stdint.h>

/*****************************************
 * Module Register Offsets.
 *****************************************/

#define USBHS_REG_FADDR         0x000
#define USBHS_REG_POWER         0x001
#define USBHS_REG_INTRTX        0x002
#define USBHS_REG_INTRRX        0x004
#define USBHS_REG_INTRTXE       0x006
#define USBHS_REG_INTRRXE       0x008
#define USBHS_REG_INTRUSB       0x00A 
#define USBHS_REG_INTRUSBE      0x00B 
#define USBHS_REG_FRAME         0x00C
#define USBHS_REG_INDEX         0x00E
#define USBHS_REG_TESTMODE      0x00F

/*******************************************************
 * Endpoint Control Status Registers (CSR). These values 
 * should be added to either the 0x10 to access the
 * register through Indexed CSR. To access the actual 
 * CSR, see ahead in this header file.
 ******************************************************/

#define USBHS_REG_EP_TXMAXP     0x000
#define USBHS_REG_EP_CSR0L      0x002
#define USBHS_REG_EP_CSR0H      0x003
#define USBHS_REG_EP_TXCSRL     0x002
#define USBHS_REG_EP_TXCSRH     0x003
#define USBHS_REG_EP_RXMAXP     0x004
#define USBHS_REG_EP_RXCSRL     0x006
#define USBHS_REG_EP_RXCSRH     0x007
#define USBHS_REG_EP_COUNT0     0x008
#define USBHS_REG_EP_RXCOUNT    0x008
#define USBHS_REG_EP_TYPE0      0x01A
#define USBHS_REG_EP_TXTYPE     0x01A
#define USBHS_REG_EP_NAKLIMIT0  0x01B
#define USBHS_REG_EP_TXINTERVAL 0x01B
#define USBHS_REG_EP_RXTYPE     0x01C
#define USBHS_REG_EP_RXINTERVAL 0x01D
#define USBHS_REG_EP_CONFIGDATA 0x01F
#define USBHS_REG_EP_FIFOSIZE   0x01F

#define USBHS_HOST_EP0_SETUPKT_SET 0x8
#define USBHS_HOST_EP0_TXPKTRDY_SET 0x2
#define USBHS_SOFT_RST_NRST_SET 0x1
#define USBHS_SOFT_RST_NRSTX_SET 0x2
#define USBHS_EP0_DEVICE_SERVICED_RXPKTRDY 0x40
#define USBHS_EP0_DEVICE_DATAEND 0x08
#define USBHS_EP0_DEVICE_TXPKTRDY 0x02
#define USBHS_EP0_HOST_STATUS_STAGE_START 0x40
#define USBHS_EP0_HOST_REQPKT 0x20
#define USBHS_EP0_HOST_TXPKTRDY 0x02
#define USBHS_EP0_HOST_RXPKTRDY 0x01
#define USBHS_EP_DEVICE_TX_SENT_STALL 0x20
#define USBHS_EP_DEVICE_TX_SEND_STALL 0x10
#define USBHS_EP_DEVICE_RX_SENT_STALL 0x40
#define USBHS_EP_DEVICE_RX_SEND_STALL 0x20

/* FADDR - Device Function Address */
typedef union 
{
    struct __attribute__((packed)) 
    {
        unsigned FUNC:7;
        unsigned :1;
    };

    uint8_t w;  

} __USBHS_FADDR_t;

/* POWER - Control Resume and Suspend signalling */
typedef union 
{
    struct __attribute__((packed))
    {
        unsigned SUSPEN:1;
        unsigned SUSPMODE:1;
        unsigned RESUME:1;
        unsigned RESET:1;
        unsigned HSMODE:1;
        unsigned HSEN:1;
        unsigned SOFTCONN:1;
        unsigned ISOUPD:1;
    };
    struct
    {   
        uint8_t w;
    };

} __USBHS_POWER_t;

/* INTRTXE - Transmit endpoint interrupt enable */
typedef union 
{
    struct __attribute__((packed))
    {
        unsigned EP0IE:1;
        unsigned EP1TXIE:1;
        unsigned EP2TXIE:1;
        unsigned EP3TXIE:1;
        unsigned EP4TXIE:1;
        unsigned EP5TXIE:1;
        unsigned EP6TXIE:1;
        unsigned EP7TXIE:1;
        unsigned :8;
    };
    struct
    {
        uint16_t    w;
    };

} __USBHS_INTRTXE_t;

/* INTRRXE - Receive endpoint interrupt enable */
typedef union 
{
    struct __attribute__((packed))
    {
        unsigned :1;
        unsigned EP1RXIE:1;
        unsigned EP2RXIE:1;
        unsigned EP3RXIE:1;
        unsigned EP4RXIE:1;
        unsigned EP5RXIE:1;
        unsigned EP6RXIE:1;
        unsigned EP7RXIE:1;
        unsigned :8;
    };
    struct
    {
        uint16_t    w;
    };

} __USBHS_INTRRXE_t;

/* INTRUSBE - General USB Interrupt enable */
typedef union
{
    struct __attribute__((packed))
    {
        unsigned SUSPIE:1;
        unsigned RESUMEIE:1;
        unsigned RESETIE:1;
        unsigned SOFIE:1;
        unsigned CONNIE:1;
        unsigned DISCONIE:1;
        unsigned SESSRQIE:1;
        unsigned VBUSERRIE:1;
    };
    struct
    {
        uint8_t w;
    };

} __USBHS_INTRUSBE_t;

/* FRAME - Frame number */
typedef union 
{
    struct __attribute__((packed))
    {
        unsigned RFRMNUM:11;
        unsigned :5;
    };
    struct
    {
        uint16_t w;
    };

} __USBHS_FRAME_t;

/* INDEX - Endpoint index */
typedef union 
{
    struct __attribute__((packed))
    {
        unsigned ENDPOINT:4;
        unsigned :4;
    };
    struct
    {
        uint8_t w;
    };

} __USBHS_INDEX_t;

/* TESTMODE - Test mode register */
typedef union 
{
    struct __attribute__((packed))
    {
        unsigned NAK:1;
        unsigned TESTJ:1;
        unsigned TESTK:1;
        unsigned PACKET:1;
        unsigned FORCEHS:1;
        unsigned FORCEFS:1;
        unsigned FIFOACC:1;
        unsigned FORCEHST:1;
    };
    struct
    {
        uint8_t w;
    };

} __USBHS_TESTMODE_t;

/* COUNT0 - Indicates the amount of data received in endpoint 0 */ 
typedef union
{
    struct __attribute__((packed))
    {
        unsigned RXCNT:7;
        unsigned :1;
    };
    struct
    {
        uint8_t w;
    };

} __USBHS_COUNT0_t;

/* TYPE0 - Operating speed of target device */
typedef union
{
    struct __attribute__((packed))
    {
        unsigned :6;
        unsigned SPEED:2;
    };
    struct
    {
        uint8_t w;
    };

} __USBHS_TYPE0_t;

/* DEVCTL - Module control register */
typedef union
{
    struct __attribute__((packed))
    {
        unsigned SESSION:1;
        unsigned HOSTREQ:1;
        unsigned HOSTMODE:1;
        unsigned VBUS:2;
        unsigned LSDEV:1;
        unsigned FSDEV:1;
        unsigned BDEV:1;
    };
    struct
    {
        uint8_t w;
    };

} __USBHS_DEVCTL_t;

/* CSR0L Device - Endpoint Device Mode Control Status Register */
typedef union
{
    struct __attribute__((packed))
    {
        unsigned RXPKTRDY:1;
        unsigned TXPKTRDY:1;
        unsigned SENTSTALL:1;
        unsigned DATAEND:1;
        unsigned SETUPEND:1;
        unsigned SENDSTALL:1;
        unsigned SVCRPR:1;
        unsigned SVSSETEND:1;
    };
    struct
    {
        uint8_t w;
    };

} __USBHS_CSR0L_DEVICE_t;

/* CSR0L Host - Endpoint Host Mode Control Status Register */
typedef union
{
    struct __attribute__((packed))
    {
        unsigned RXPKTRDY:1;
        unsigned TXPKTRDY:1;
        unsigned RXSTALL:1;
        unsigned SETUPPKT:1;
        unsigned ERROR:1;
        unsigned REQPKT:1;
        unsigned STATPKT:1;
        unsigned NAKTMOUT:1;
    };
    struct
    {
        uint8_t w;
    };

} __USBHS_CSR0L_HOST_t;

/* TXCSRL Device - Endpoint Transmit Control Status Register Low */
typedef union
{
    struct __attribute__((packed))
    {
        unsigned TXPKTRDY:1;
        unsigned FIFOONE:1;
        unsigned UNDERRUN:1;
        unsigned FLUSH:1;
        unsigned SENDSTALL:1;
        unsigned SENTSTALL:1;
        unsigned CLRDT:1;
        unsigned INCOMPTX:1;
    };
    struct
    {
        uint8_t w;
    };

} __USBHS_TXCSRL_DEVICE_t;

/* TXCSRL Host - Endpoint Transmit Control Status Register Low */
typedef union
{
    struct __attribute__((packed))
    {
        unsigned TXPKTRDY:1;
        unsigned FIFONE:1;
        unsigned ERROR:1;
        unsigned FLUSH:1;
        unsigned SETUPPKT:1;
        unsigned RXSTALL:1;
        unsigned CLRDT:1;
        unsigned INCOMPTX:1;
    };
    struct
    {
        uint8_t w;
    };

} __USBHS_TXCSRL_HOST_t;

/* TXCSRH Device - Endpoint Transmit Control Status Register High */
typedef union
{
    struct __attribute__((packed))
    {
        unsigned :2;
        unsigned DMAREQMD:1;
        unsigned FRCDATTG:1;
        unsigned DMAREQENL:1;
        unsigned MODE:1;
        unsigned ISO:1;
        unsigned AUTOSET:1;
    };
    struct
    {
        uint8_t w;
    };

} __USBHS_TXCSRH_DEVICE_t;

/* TXCSRH Host - Endpoint Transmit Control Status Register High */
typedef union
{
    struct __attribute__((packed))
    {
        unsigned DATATGGL:1;
        unsigned DTWREN:1;
        unsigned DMAREQMD:1;
        unsigned FRCDATTG:1;
        unsigned DMAREQEN:1;
        unsigned MODE:1;
        unsigned :1;
        unsigned AUOTSET:1;
    };
    struct
    {
        uint8_t w;
    };

} __USBHS_TXCSRH_HOST_t;

/* CSR0H Device - Endpoint 0 Control Status Register High */
typedef union
{
    struct __attribute__((packed))
    {
        unsigned FLSHFIFO:1;
        unsigned :7;
    };
    struct
    {
        uint8_t w;
    };

} __USBHS_CSR0H_DEVICE_t;

/* CSR0H Host - Endpoint 0 Control Status Register High */
typedef union
{
    struct __attribute__((packed))
    {
        unsigned FLSHFIFO:1;
        unsigned DATATGGL:1;
        unsigned DTWREN:1;
        unsigned DISPING:1;
        unsigned :4;
    };
    struct
    {
        uint8_t w;
    };

} __USBHS_CSR0H_HOST_t;

/* RXMAXP - Receive Endpoint Max packet size. */
typedef union
{
    struct __attribute__((packed))
    {
        unsigned RXMAXP:11;
        unsigned MULT:5;
    };
    struct
    {
        uint16_t w;
    };

} __USBHS_RXMAXP_t;

/* RXCSRL Device - Receive endpoint Control Status Register */
typedef union
{
    struct __attribute__((packed))
    {
        unsigned RXPKTRDY:1;
        unsigned FIFOFULL:1;
        unsigned OVERRUN:1;
        unsigned DATAERR:1;
        unsigned FLUSH:1;
        unsigned SENDSTALL:1;
        unsigned SENTSTALL:1;
        unsigned CLRDT:1;
    };
    struct
    {
        uint8_t w;
    };

} __USBHS_RXCSRL_DEVICE_t;

/* RXCSRL Host - Receive endpoint Control Status Register */
typedef union
{
    struct __attribute__((packed))
    {
        unsigned RXPKTRDY:1;
        unsigned FIFOFULL:1;
        unsigned ERROR:1;
        unsigned DERRNAKT:1;
        unsigned FLUSH:1;
        unsigned REQPKT:1;
        unsigned RXSTALL:1;
        unsigned CLRDT:1;
    };
    struct
    {
        uint8_t w;
    };

} __USBHS_RXCSRL_HOST_t;

/* RXCSRH Device - Receive endpoint Control Status Register */
typedef union
{
    struct __attribute__((packed))
    {
        unsigned INCOMPRX:1;
        unsigned :2;
        unsigned DMAREQMODE:1;
        unsigned DISNYET:1;
        unsigned DMAREQEN:1;
        unsigned ISO:1;
        unsigned AUTOCLR:1;
    };
    struct
    {
        uint8_t w;
    };

} __USBHS_RXCSRH_DEVICE_t;

/* RXCSRH Host - Receive endpoint Control Status Register */
typedef union
{
    struct __attribute__((packed))
    {
        unsigned INCOMPRX:1;
        unsigned DATATGGL:1;
        unsigned DATATWEN:1;
        unsigned DMAREQMD:1;
        unsigned PIDERR:1;
        unsigned DMAREQEN:1;
        unsigned AUTORQ:1;
        unsigned AUOTCLR:1;
    };
    struct
    {
        uint8_t w;
    };

} __USBHS_RXCSRH_HOST_t;

/* RXCOUNT - Amount of data pending in RX FIFO */
typedef union
{
    struct __attribute__((packed))
    {
        unsigned RXCNT:14;
        unsigned :2;
    };
    struct
    {
        uint16_t w;
    };

} __USBHS_RXCOUNT_t;

/* TXTYPE - Specifies the target transmit endpoint */
typedef union
{
    struct __attribute__((packed))
    {
        unsigned TEP:4;
        unsigned PROTOCOL:2;
        unsigned SPEED:2;
    };
    struct
    {
        uint8_t w;
    };

} __USBHS_TXTYPE_t;

/* RXTYPE - Specifies the target receive endpoint */
typedef union
{
    struct __attribute__((packed))
    {
        unsigned TEP:4;
        unsigned PROTOCOL:2;
        unsigned SPEED:2;
    };
    struct
    {
        uint8_t w;
    };

} __USBHS_RXTYPE_t;

/* TXINTERVAL - Defines the polling interval */
typedef struct
{
    uint8_t TXINTERV;

} __USBHS_TXINTERVAL_t;

/* RXINTERVAL - Defines the polling interval */
typedef struct
{
    uint8_t RXINTERV;

} __USBHS_RXINTERVAL_t;

/* TXMAXP - Maximum amount of data that can be transferred through a TX endpoint
 * */

typedef union
{
    struct __attribute__((packed))
    {
        unsigned TXMAXP:11;
        unsigned MULT:5;
    };
    uint16_t w;

} __USBHS_TXMAXP_t;  

/* TXFIFOSZ - Size of the transmit endpoint FIFO */
typedef struct __attribute__((packed))
{
    unsigned TXFIFOSZ:4;
    unsigned TXDPB:1;
    unsigned :3;

} __USBHS_TXFIFOSZ_t;

/* RXFIFOSZ - Size of the receive endpoint FIFO */
typedef struct __attribute__((packed))
{
    unsigned RXFIFOSZ:4;
    unsigned RXDPB:1;
    unsigned :3;

} __USBHS_RXFIFOSZ_t;

/* TXFIFOADD - Start address of the transmit endpoint FIFO */
typedef union
{
    struct __attribute__((packed))
    {
        unsigned TXFIFOAD:13;
        unsigned :3;
    };
    uint16_t w;

} __USBHS_TXFIFOADD_t;

/* RXFIFOADD - Start address of the receive endpoint FIFO */
typedef union
{
    struct __attribute__((packed))
    {
        unsigned RXFIFOAD:13;
        unsigned :3;
    };
    uint16_t w;

} __USBHS_RXFIFOADD_t;

/* SOFTRST - Asserts NRSTO and NRSTOX */
typedef union
{
    struct __attribute__((packed))
    {
        unsigned NRST:1;
        unsigned NRSTX:1;
        unsigned :6;
    };
    uint8_t w;

} __USBHS_SOFTRST_t;

/* TXFUNCADDR - Target address of transmit endpoint */
typedef union
{
    struct __attribute__((packed))
    {
        unsigned TXFADDR:7;
        unsigned :1;
    };
    uint8_t w;

} __USBHS_TXFUNCADDR_t;

/* RXFUNCADDR - Target address of receive endpoint */
typedef union
{
    struct __attribute__((packed))
    {
        unsigned RXFADDR:7;
        unsigned :1;
    };
    uint8_t w;

} __USBHS_RXFUNCADDR_t;

/* TXHUBADDR - Address of the hub to which the target transmit device endpoint
 * is connected */
typedef union
{
    struct __attribute__((packed))
    {
        unsigned TXHUBADDR:7;
        unsigned MULTTRAN:1;
    };
    uint8_t w;

} __USBHS_TXHUBADDR_t;

/* RXHUBADDR - Address of the hub to which the target receive device endpoint is
 * connected */
typedef union
{
    struct __attribute__((packed))
    {
        unsigned RXHUBADDR:7;
        unsigned MULTTRAN:1;
    };
    uint8_t w;

} __USBHS_RXHUBADDR_t;

/* TXHUBPORT - Address of the hub to which the target transmit device endpoint
 * is connected. */
typedef union
{
    struct __attribute__((packed))
    {
        unsigned TXHUBPRT:7;
        unsigned :1;
    };

    uint8_t w;

} __USBHS_TXHUBPORT_t;

/* RXHUBPORT - Address of the hub to which the target receive device endpoint
 * is connected. */
typedef union
{
    struct __attribute__((packed))
    {
        unsigned RXHUBPRT:7;
        unsigned :1;
    };

    uint8_t w;

} __USBHS_RXHUBPORT_t;

/* DMACONTROL - Configures a DMA channel */
typedef union
{
    struct __attribute__((packed))
    {
        unsigned DMAEN:1;
        unsigned DMADIR:1;
        unsigned DMAMODE:1;
        unsigned DMAIE:1;
        unsigned DMAEP:4;
        unsigned DMAERR:1;
        unsigned DMABRSTM:2;
        unsigned:21;
    };

    uint32_t w;

} __USBHS_DMACNTL_t;

/* Endpoint Control and Status Register Set */    
typedef struct __attribute__((packed))
{
    volatile __USBHS_TXMAXP_t TXMAXPbits;
    union
    {
        struct
        {
            union
            {
                volatile __USBHS_CSR0L_DEVICE_t CSR0L_DEVICEbits;
                volatile __USBHS_CSR0L_HOST_t CSR0L_HOSTbits;
            };
            union
            {
                volatile __USBHS_CSR0H_DEVICE_t CSR0H_DEVICEbits;
                volatile __USBHS_CSR0H_HOST_t CSR0H_HOSTbits;
            };
        };

        struct
        {
            union
            {
                volatile __USBHS_TXCSRL_DEVICE_t TXCSRL_DEVICEbits;
                volatile __USBHS_TXCSRL_HOST_t TXCSRL_HOSTbits;
            };

            union
            {
                volatile __USBHS_TXCSRH_DEVICE_t TXCSRH_DEVICEbits;
                volatile __USBHS_TXCSRH_HOST_t TXCSRH_HOSTbits;
            };
        };
    };

    volatile __USBHS_RXMAXP_t RXMAXPbits;

    union
    {
        volatile __USBHS_RXCSRL_DEVICE_t RXCSRL_DEVICEbits;
        volatile __USBHS_RXCSRL_HOST_t RXCSRL_HOSTbits;
    };

    union
    {
        volatile __USBHS_RXCSRH_DEVICE_t RXCSRH_DEVICEbits;
        volatile __USBHS_RXCSRH_HOST_t RXCSRH_HOSTbits;
    };

    union
    {
        volatile __USBHS_COUNT0_t COUNT0bits;
        volatile __USBHS_RXCOUNT_t RXCOUNTbits;
    };

    union
    {
        volatile __USBHS_TYPE0_t TYPE0bits;
        volatile __USBHS_TXTYPE_t TXTYPEbits;
    };

    union
    {
        volatile uint8_t NAKLIMIT0;
        volatile __USBHS_TXINTERVAL_t TXINTERVALbits;
    };

    volatile __USBHS_RXTYPE_t RXTYPEbits;
    volatile __USBHS_RXINTERVAL_t RXINTERVALbits;
    unsigned :8;
    union
    {
        volatile uint8_t CONFIGDATA;
        volatile uint8_t FIFOSIZE;
    };

} __USBHS_EPCSR_t;

/* Set of registers that configure the multi-point option */
typedef struct __attribute__((packed))
{
    volatile __USBHS_TXFUNCADDR_t TXFUNCADDRbits;
    unsigned :8;
    volatile __USBHS_TXHUBADDR_t TXHUBADDRbits;
    volatile __USBHS_TXHUBPORT_t TXHUBPORTbits;
    volatile __USBHS_RXFUNCADDR_t RXFUNCADDRbits;
    unsigned :8;
    volatile __USBHS_RXHUBADDR_t RXHUBADDRbits;
    volatile __USBHS_RXHUBPORT_t RXHUBPORTbits;

} __USBHS_TARGET_ADDR_t;

/* Set of registers that configure the DMA channel */
typedef struct __attribute__((packed))
{
    volatile __USBHS_DMACNTL_t DMACNTLbits;
    volatile uint32_t DMAADDR;
    volatile uint32_t DMACOUNT;
    volatile uint32_t pad;
} __USBHS_DMA_CHANNEL_t;

/* USBHS module register set */
typedef struct __attribute__((aligned(4),packed))
{
    volatile __USBHS_FADDR_t    FADDRbits;
    volatile __USBHS_POWER_t    POWERbits;
    volatile uint16_t           INTRTX;
    volatile uint16_t           INTRRX;
    volatile __USBHS_INTRTXE_t  INTRTXEbits;
    volatile __USBHS_INTRRXE_t  INTRRXEbits;
    volatile uint8_t            INTRUSB;
    volatile __USBHS_INTRUSBE_t INTRUSBEbits;
    volatile __USBHS_FRAME_t    FRAMEbits;
    volatile __USBHS_INDEX_t    INDEXbits;
    volatile __USBHS_TESTMODE_t TESTMODEbits;
    volatile __USBHS_EPCSR_t    INDEXED_EPCSR;
    volatile uint32_t           FIFO[16];
    volatile __USBHS_DEVCTL_t   DEVCTLbits;
    volatile uint8_t            MISC;
    volatile __USBHS_TXFIFOSZ_t TXFIFOSZbits;
    volatile __USBHS_RXFIFOSZ_t RXFIFOSZbits;

    volatile __USBHS_TXFIFOADD_t   TXFIFOADDbits;
    volatile __USBHS_RXFIFOADD_t   RXFIFOADDbits;
    
    volatile uint32_t   VCONTROL;
    volatile uint16_t   HWVERS;
    volatile uint8_t    padding1[10];
    volatile uint8_t    EPINFO;
    volatile uint8_t    RAMINFO;
    volatile uint8_t    LINKINFO;
    volatile uint8_t    VPLEN;
    volatile uint8_t    HS_EOF1;
    volatile uint8_t    FS_EOF1;
    volatile uint8_t    LS_EOF1;

    volatile __USBHS_SOFTRST_t    SOFTRSTbits;

    volatile __USBHS_TARGET_ADDR_t  TADDR[16];
    volatile __USBHS_EPCSR_t        EPCSR[16];
    volatile uint32_t               DMA_INTR;
    volatile __USBHS_DMA_CHANNEL_t  DMA_CHANNEL[8]; 
    volatile uint32_t               RQPKTXOUNT[16];

} usbhs_registers_t;

#endif
