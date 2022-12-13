/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 Koji KITAYAMA
 * Copyright (c) 2021 Tian Yunhao (t123yh)
 * Copyright (c) 2021 Ha Thach (tinyusb.org)
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

#ifndef _TUSB_MUSB_DEF
#define _TUSB_MUSB_DEF


#define  USBC_Readb(reg)	                    (*(volatile unsigned char *)(reg))
#define  USBC_Readw(reg)	                    (*(volatile unsigned short *)(reg))
#define  USBC_Readl(reg)	                    (*(volatile unsigned long *)(reg))

#define  USBC_Writeb(value, reg)                (*(volatile unsigned char *)(reg) = (value))
#define  USBC_Writew(value, reg)	            (*(volatile unsigned short *)(reg) = (value))
#define  USBC_Writel(value, reg)	            (*(volatile unsigned long *)(reg) = (value))


#define USBC_SetBit_Mask_b(reg,mask)	do {																		\
																				unsigned char _r = USBC_Readb(reg);	\
																			   _r  |=  (unsigned char)(mask);				\
																				USBC_Writeb(_r,reg); 								\
																		}while(0)
#define USBC_SetBit_Mask_w(reg,mask)	do {																		\
																				unsigned short _r = USBC_Readw(reg);	\
																			   _r  |=  (unsigned short)(mask);				\
																				USBC_Writew(_r,reg); 								\
																		}while(0)
#define USBC_SetBit_Mask_l(reg,mask)	do {																		\
																				unsigned int _r = USBC_Readl(reg);	\
																			   _r  |=  (unsigned int)(mask);				\
																				USBC_Writel(_r,reg); 								\
																		}while(0)


#define USBC_ClrBit_Mask_b(reg,mask)	do {																		\
																				unsigned char _r = USBC_Readb(reg);	\
																			   _r  &=  (~(unsigned char)(mask));			\
																				USBC_Writeb(_r,reg); 								\
																		}while(0);
#define USBC_ClrBit_Mask_w(reg,mask)	do {																		\
																				unsigned short _r = USBC_Readw(reg);	\
																			   _r  &=  (~(unsigned short)(mask));				\
																				USBC_Writew(_r,reg); 								\
																		}while(0)
#define USBC_ClrBit_Mask_l(reg,mask)	do {																		\
																				unsigned int _r = USBC_Readl(reg);	\
																			   _r  &=  (~(unsigned int)(mask));				\
																				USBC_Writel(_r,reg); 								\
																		}while(0)
#define  USBC_REG_test_bit_b(bp, reg)         	(USBC_Readb(reg) & (1 << (bp)))
#define  USBC_REG_test_bit_w(bp, reg)   	    (USBC_Readw(reg) & (1 << (bp)))
#define  USBC_REG_test_bit_l(bp, reg)   	    (USBC_Readl(reg) & (1 << (bp)))

#define  USBC_REG_set_bit_b(bp, reg) 			(USBC_Writeb((USBC_Readb(reg) | (1 << (bp))) , (reg)))
#define  USBC_REG_set_bit_w(bp, reg) 	 		(USBC_Writew((USBC_Readw(reg) | (1 << (bp))) , (reg)))
#define  USBC_REG_set_bit_l(bp, reg) 	 		(USBC_Writel((USBC_Readl(reg) | (1 << (bp))) , (reg)))

#define  USBC_REG_clear_bit_b(bp, reg)	 	 	(USBC_Writeb((USBC_Readb(reg) & (~ (1 << (bp)))) , (reg)))
#define  USBC_REG_clear_bit_w(bp, reg)	 	 	(USBC_Writew((USBC_Readw(reg) & (~ (1 << (bp)))) , (reg)))
#define  USBC_REG_clear_bit_l(bp, reg)	 	 	(USBC_Writel((USBC_Readl(reg) & (~ (1 << (bp)))) , (reg)))

#define SW_UDC_EPNUMS 3

#define SUNXI_SRAMC_BASE 0x01c00000
//---------------------------------------------------------------
//   reg base
//---------------------------------------------------------------
#define  USBC0_BASE                 0x01c13000
#define  USBC1_BASE                 0x01c14000
#define  USBC2_BASE                 0x01c1E000

//Some reg within musb
#define USBPHY_CLK_REG 		0x01c200CC
#define USBPHY_CLK_RST_BIT 0
#define USBPHY_CLK_GAT_BIT 1

#define BUS_CLK_RST_REG	0x01c202c0 //Bus Clock Reset Register Bit24 : USB CLK RST
#define BUS_RST_USB_BIT	24

#define BUS_CLK_GATE0_REG	0x01c20060 //Bus Clock Gating Register Bit24 : USB CLK GATE 0: Mask 1 : Pass
#define BUS_CLK_USB_BIT	24

//#define USB_INTR

#define NDMA_CFG_REG
//-----------------------------------------------------------------------
//   musb reg offset
//-----------------------------------------------------------------------

#define  USBC_REG_o_FADDR		    0x0098
#define  USBC_REG_o_PCTL		    0x0040
#define  USBC_REG_o_INTTx		    0x0044
#define  USBC_REG_o_INTRx		    0x0046
#define  USBC_REG_o_INTTxE		    0x0048
#define  USBC_REG_o_INTRxE		    0x004A
#define  USBC_REG_o_INTUSB		    0x004C
#define  USBC_REG_o_INTUSBE         0x0050
#define  USBC_REG_o_FRNUM		    0x0054
#define  USBC_REG_o_EPIND		    0x0042
#define  USBC_REG_o_TMCTL		    0x007C

#define  USBC_REG_o_TXMAXP		    0x0080
#define  USBC_REG_o_CSR0		    0x0082
#define  USBC_REG_o_TXCSR		    0x0082
#define  USBC_REG_o_RXMAXP		    0x0084
#define  USBC_REG_o_RXCSR		    0x0086
#define  USBC_REG_o_COUNT0		    0x0088
#define  USBC_REG_o_RXCOUNT		    0x0088
#define  USBC_REG_o_EP0TYPE		    0x008C
#define  USBC_REG_o_TXTYPE		    0x008C
#define  USBC_REG_o_NAKLIMIT0	    0x008D
#define  USBC_REG_o_TXINTERVAL      0x008D
#define  USBC_REG_o_RXTYPE		    0x008E
#define  USBC_REG_o_RXINTERVAL	    0x008F

//#define  USBC_REG_o_CONFIGDATA	0x001F   //

#define  USBC_REG_o_EPFIFO0		    0x0000
#define  USBC_REG_o_EPFIFO1		    0x0004
#define  USBC_REG_o_EPFIFO2		    0x0008
#define  USBC_REG_o_EPFIFO3		    0x000C
#define  USBC_REG_o_EPFIFO4		    0x0010
#define  USBC_REG_o_EPFIFO5		    0x0014
#define  USBC_REG_o_EPFIFOx(n)	    (0x0000 + (n<<2))

#define  USBC_REG_o_DEVCTL		    0x0041

#define  USBC_REG_o_TXFIFOSZ	    0x0090
#define  USBC_REG_o_RXFIFOSZ	    0x0094
#define  USBC_REG_o_TXFIFOAD	    0x0092
#define  USBC_REG_o_RXFIFOAD	    0x0096

#define  USBC_REG_o_VEND0		    0x0043
#define  USBC_REG_o_VEND1		    0x007D
#define  USBC_REG_o_VEND3		    0x007E

//#define  USBC_REG_o_PHYCTL		0x006C
#define  USBC_REG_o_EPINFO		    0x0078
#define  USBC_REG_o_RAMINFO		    0x0079
#define  USBC_REG_o_LINKINFO	    0x007A
#define  USBC_REG_o_VPLEN		    0x007B
#define  USBC_REG_o_HSEOF		    0x007C
#define  USBC_REG_o_FSEOF		    0x007D
#define  USBC_REG_o_LSEOF		    0x007E

//new
#define  USBC_REG_o_FADDR0          0x0098
#define  USBC_REG_o_HADDR0          0x009A
#define  USBC_REG_o_HPORT0          0x009B
#define  USBC_REG_o_TXFADDRx 		0x0098
#define  USBC_REG_o_TXHADDRx		0x009A
#define  USBC_REG_o_TXHPORTx		0x009B
#define  USBC_REG_o_RXFADDRx		0x009C
#define  USBC_REG_o_RXHADDRx		0x009E
#define  USBC_REG_o_RXHPORTx		0x009F


#define  USBC_REG_o_RPCOUNT			0x008A

//new
#define  USBC_REG_o_ISCR            0x0400
#define  USBC_REG_o_PHYCTL          0x0404
#define  USBC_REG_o_PHYBIST         0x0408
#define  USBC_REG_o_PHYTUNE         0x040c

#define  USBC_REG_o_CSR			0x0410

#define USBC_REG_o_PMU_IRQ	 0x0800

//-----------------------------------------------------------------------
//   registers
//-----------------------------------------------------------------------

#define  USBC_REG_FADDR(usbc_base_addr)		        ((usbc_base_addr) + USBC_REG_o_FADDR		)
#define  USBC_REG_PCTL(usbc_base_addr)		        ((usbc_base_addr) + USBC_REG_o_PCTL			)
#define  USBC_REG_INTTx(usbc_base_addr)		        ((usbc_base_addr) + USBC_REG_o_INTTx		)
#define  USBC_REG_INTRx(usbc_base_addr)		        ((usbc_base_addr) + USBC_REG_o_INTRx		)
#define  USBC_REG_INTTxE(usbc_base_addr)		    ((usbc_base_addr) + USBC_REG_o_INTTxE     	)
#define  USBC_REG_INTRxE(usbc_base_addr)		    ((usbc_base_addr) + USBC_REG_o_INTRxE     	)
#define  USBC_REG_INTUSB(usbc_base_addr)		    ((usbc_base_addr) + USBC_REG_o_INTUSB     	)
#define  USBC_REG_INTUSBE(usbc_base_addr)           ((usbc_base_addr) + USBC_REG_o_INTUSBE    	)
#define  USBC_REG_FRNUM(usbc_base_addr)		        ((usbc_base_addr) + USBC_REG_o_FRNUM      	)
#define  USBC_REG_EPIND(usbc_base_addr)		        ((usbc_base_addr) + USBC_REG_o_EPIND      	)
#define  USBC_REG_TMCTL(usbc_base_addr)		        ((usbc_base_addr) + USBC_REG_o_TMCTL      	)
#define  USBC_REG_TXMAXP(usbc_base_addr)		    ((usbc_base_addr) + USBC_REG_o_TXMAXP     	)

#define  USBC_REG_CSR0(usbc_base_addr)		        ((usbc_base_addr) + USBC_REG_o_CSR0       	)
#define  USBC_REG_TXCSR(usbc_base_addr)		        ((usbc_base_addr) + USBC_REG_o_TXCSR      	)

#define  USBC_REG_RXMAXP(usbc_base_addr)		    ((usbc_base_addr) + USBC_REG_o_RXMAXP     	)
#define  USBC_REG_RXCSR(usbc_base_addr)		        ((usbc_base_addr) + USBC_REG_o_RXCSR      	)

#define  USBC_REG_COUNT0(usbc_base_addr)		    ((usbc_base_addr) + USBC_REG_o_COUNT0     	)
#define  USBC_REG_RXCOUNT(usbc_base_addr)		    ((usbc_base_addr) + USBC_REG_o_RXCOUNT    	)

#define  USBC_REG_EP0TYPE(usbc_base_addr)		    ((usbc_base_addr) + USBC_REG_o_EP0TYPE		)
#define  USBC_REG_TXTYPE(usbc_base_addr)		    ((usbc_base_addr) + USBC_REG_o_TXTYPE     	)

#define  USBC_REG_NAKLIMIT0(usbc_base_addr)	        ((usbc_base_addr) + USBC_REG_o_NAKLIMIT0  	)
#define  USBC_REG_TXINTERVAL(usbc_base_addr)        ((usbc_base_addr) + USBC_REG_o_TXINTERVAL	)

#define  USBC_REG_RXTYPE(usbc_base_addr)		    ((usbc_base_addr) + USBC_REG_o_RXTYPE		)
#define  USBC_REG_RXINTERVAL(usbc_base_addr)	    ((usbc_base_addr) + USBC_REG_o_RXINTERVAL	)
//#define  USBC_REG_CONFIGDATA(usbc_base_addr)	    ((usbc_base_addr) + USBC_REG_o_CONFIGDATA	)
#define  USBC_REG_EPFIFO0(usbc_base_addr)		    ((usbc_base_addr) + USBC_REG_o_EPFIFO0		)
#define  USBC_REG_EPFIFO1(usbc_base_addr)		    ((usbc_base_addr) + USBC_REG_o_EPFIFO1		)
#define  USBC_REG_EPFIFO2(usbc_base_addr)		    ((usbc_base_addr) + USBC_REG_o_EPFIFO2		)
#define  USBC_REG_EPFIFO3(usbc_base_addr)		    ((usbc_base_addr) + USBC_REG_o_EPFIFO3		)
#define  USBC_REG_EPFIFO4(usbc_base_addr)		    ((usbc_base_addr) + USBC_REG_o_EPFIFO4		)
#define  USBC_REG_EPFIFO5(usbc_base_addr)		    ((usbc_base_addr) + USBC_REG_o_EPFIFO5		)
#define  USBC_REG_EPFIFOx(usbc_base_addr, n)	    ((usbc_base_addr) + USBC_REG_o_EPFIFOx(n)	)
#define  USBC_REG_DEVCTL(usbc_base_addr)		    ((usbc_base_addr) + USBC_REG_o_DEVCTL		)
#define  USBC_REG_TXFIFOSZ(usbc_base_addr)	        ((usbc_base_addr) + USBC_REG_o_TXFIFOSZ		)
#define  USBC_REG_RXFIFOSZ(usbc_base_addr)	        ((usbc_base_addr) + USBC_REG_o_RXFIFOSZ		)
#define  USBC_REG_TXFIFOAD(usbc_base_addr)	        ((usbc_base_addr) + USBC_REG_o_TXFIFOAD		)
#define  USBC_REG_RXFIFOAD(usbc_base_addr)	        ((usbc_base_addr) + USBC_REG_o_RXFIFOAD		)
#define  USBC_REG_VEND0(usbc_base_addr)		        ((usbc_base_addr) + USBC_REG_o_VEND0		)
#define  USBC_REG_VEND1(usbc_base_addr)		        ((usbc_base_addr) + USBC_REG_o_VEND1		)
#define  USBC_REG_EPINFO(usbc_base_addr)		    ((usbc_base_addr) + USBC_REG_o_EPINFO		)
#define  USBC_REG_RAMINFO(usbc_base_addr)		    ((usbc_base_addr) + USBC_REG_o_RAMINFO		)
#define  USBC_REG_LINKINFO(usbc_base_addr)	        ((usbc_base_addr) + USBC_REG_o_LINKINFO		)
#define  USBC_REG_VPLEN(usbc_base_addr)		        ((usbc_base_addr) + USBC_REG_o_VPLEN		)
#define  USBC_REG_HSEOF(usbc_base_addr)		        ((usbc_base_addr) + USBC_REG_o_HSEOF		)
#define  USBC_REG_FSEOF(usbc_base_addr)		        ((usbc_base_addr) + USBC_REG_o_FSEOF		)
#define  USBC_REG_LSEOF(usbc_base_addr)		        ((usbc_base_addr) + USBC_REG_o_LSEOF		)

#define  USBC_REG_FADDR0(usbc_base_addr)            ((usbc_base_addr) + USBC_REG_o_FADDR0		)
#define  USBC_REG_HADDR0(usbc_base_addr)            ((usbc_base_addr) + USBC_REG_o_HADDR0		)
#define  USBC_REG_HPORT0(usbc_base_addr)            ((usbc_base_addr) + USBC_REG_o_HPORT0		)

#define  USBC_REG_TXFADDRx(usbc_base_addr, n)		((usbc_base_addr) + USBC_REG_o_TXFADDRx		)
#define  USBC_REG_TXHADDRx(usbc_base_addr, n)		((usbc_base_addr) + USBC_REG_o_TXHADDRx		)
#define  USBC_REG_TXHPORTx(usbc_base_addr, n)		((usbc_base_addr) + USBC_REG_o_TXHPORTx		)
#define  USBC_REG_RXFADDRx(usbc_base_addr, n)		((usbc_base_addr) + USBC_REG_o_RXFADDRx		)
#define  USBC_REG_RXHADDRx(usbc_base_addr, n)		((usbc_base_addr) + USBC_REG_o_RXHADDRx		)
#define  USBC_REG_RXHPORTx(usbc_base_addr, n)		((usbc_base_addr) + USBC_REG_o_RXHPORTx		)

#define  USBC_REG_RPCOUNTx(usbc_base_addr, n)	    ((usbc_base_addr) + USBC_REG_o_RPCOUNT		)

#define  USBC_REG_ISCR(usbc_base_addr)	    	    ((usbc_base_addr) + USBC_REG_o_ISCR			)
#define  USBC_REG_PHYCTL(usbc_base_addr)	        ((usbc_base_addr) + USBC_REG_o_PHYCTL		)
#define  USBC_REG_PHYBIST(usbc_base_addr)	        ((usbc_base_addr) + USBC_REG_o_PHYBIST		)
#define  USBC_REG_PHYTUNE(usbc_base_addr)           ((usbc_base_addr) + USBC_REG_o_PHYTUNE		)
#define  USBC_REG_PMU_IRQ(usbc_base_addr)           ((usbc_base_addr) + USBC_REG_o_PMU_IRQ		)
#define  USBC_REG_CSR(usbc_base_addr)           ((usbc_base_addr) + USBC_REG_o_CSR)
//-----------------------------------------------------------------------
//   bit position
//-----------------------------------------------------------------------

/* USB Power Control for Host only  */
#define  USBC_BP_POWER_H_HIGH_SPEED_EN			5
#define  USBC_BP_POWER_H_HIGH_SPEED_FLAG		4
#define  USBC_BP_POWER_H_RESET					3
#define  USBC_BP_POWER_H_RESUME					2
#define  USBC_BP_POWER_H_SUSPEND				1
#define  USBC_BP_POWER_H_SUEPEND_EN				0

/* USB Power Control for device only  */
#define  USBC_BP_POWER_D_ISO_UPDATE_EN			7
#define  USBC_BP_POWER_D_SOFT_CONNECT			6
#define  USBC_BP_POWER_D_HIGH_SPEED_EN			5
#define  USBC_BP_POWER_D_HIGH_SPEED_FLAG		4
#define  USBC_BP_POWER_D_RESET_FLAG				3
#define  USBC_BP_POWER_D_RESUME					2
#define  USBC_BP_POWER_D_SUSPEND				1
#define  USBC_BP_POWER_D_ENABLE_SUSPENDM		0

/* interrupt flags for ep0 and the Tx ep1~4 */
#define  USBC_BP_INTTx_FLAG_EP5				    5
#define  USBC_BP_INTTx_FLAG_EP4				    4
#define  USBC_BP_INTTx_FLAG_EP3				    3
#define  USBC_BP_INTTx_FLAG_EP2		    		2
#define  USBC_BP_INTTx_FLAG_EP1  				1
#define  USBC_BP_INTTx_FLAG_EP0					0

/* interrupt flags for Rx ep1~4 */
#define  USBC_BP_INTRx_FLAG_EP5				    5
#define  USBC_BP_INTRx_FLAG_EP4				    4
#define  USBC_BP_INTRx_FLAG_EP3				    3
#define  USBC_BP_INTRx_FLAG_EP2				    2
#define  USBC_BP_INTRx_FLAG_EP1				    1

/* interrupt enable for Tx ep0~4 */
#define  USBC_BP_INTTxE_EN_EP5				    5
#define  USBC_BP_INTTxE_EN_EP4				    4
#define  USBC_BP_INTTxE_EN_EP3				    3
#define  USBC_BP_INTTxE_EN_EP2				    2
#define  USBC_BP_INTTxE_EN_EP1				    1
#define  USBC_BP_INTTxE_EN_EP0					0

/* interrupt enable for Rx ep1~4 */
#define  USBC_BP_INTRxE_EN_EP5				    5
#define  USBC_BP_INTRxE_EN_EP4				    4
#define  USBC_BP_INTRxE_EN_EP3				    3
#define  USBC_BP_INTRxE_EN_EP2			    	2
#define  USBC_BP_INTRxE_EN_EP1  		    	1

/* USB interrupt */
#define  USBC_BP_INTUSB_VBUS_ERROR				7
#define  USBC_BP_INTUSB_SESSION_REQ				6
#define  USBC_BP_INTUSB_DISCONNECT				5
#define  USBC_BP_INTUSB_CONNECT					4
#define  USBC_BP_INTUSB_SOF						3
#define  USBC_BP_INTUSB_RESET					2
#define  USBC_BP_INTUSB_RESUME					1
#define  USBC_BP_INTUSB_SUSPEND					0

/* USB interrupt enable */
#define  USBC_BP_INTUSBE_EN_VBUS_ERROR			7
#define  USBC_BP_INTUSBE_EN_SESSION_REQ			6
#define  USBC_BP_INTUSBE_EN_DISCONNECT			5
#define  USBC_BP_INTUSBE_EN_CONNECT				4
#define  USBC_BP_INTUSBE_EN_SOF					3
#define  USBC_BP_INTUSBE_EN_RESET				2
#define  USBC_BP_INTUSBE_EN_RESUME				1
#define  USBC_BP_INTUSBE_EN_SUSPEND				0

/* Test Mode Control */
#define  USBC_BP_TMCTL_FORCE_HOST               7
#define  USBC_BP_TMCTL_FIFO_ACCESS              6
#define  USBC_BP_TMCTL_FORCE_FS                 5
#define  USBC_BP_TMCTL_FORCE_HS                 4
#define  USBC_BP_TMCTL_TEST_PACKET              3
#define  USBC_BP_TMCTL_TEST_K                   2
#define  USBC_BP_TMCTL_TEST_J                   1
#define  USBC_BP_TMCTL_TEST_SE0_NAK             0

/* Tx Max packet */
#define  USBC_BP_TXMAXP_PACKET_COUNT            11
#define  USBC_BP_TXMAXP_MAXIMUM_PAYLOAD         0

/* Control and Status Register for ep0 for Host only */
#define  USBC_BP_CSR0_H_DisPing 				11
#define  USBC_BP_CSR0_H_FlushFIFO				8
#define  USBC_BP_CSR0_H_NAK_Timeout				7
#define  USBC_BP_CSR0_H_StatusPkt				6
#define  USBC_BP_CSR0_H_ReqPkt					5
#define  USBC_BP_CSR0_H_Error					4
#define  USBC_BP_CSR0_H_SetupPkt				3
#define  USBC_BP_CSR0_H_RxStall					2
#define  USBC_BP_CSR0_H_TxPkRdy					1
#define  USBC_BP_CSR0_H_RxPkRdy					0

/* Control and Status Register for ep0 for device only */
#define  USBC_BP_CSR0_D_FLUSH_FIFO				8
#define  USBC_BP_CSR0_D_SERVICED_SETUP_END		7
#define  USBC_BP_CSR0_D_SERVICED_RX_PKT_READY   6
#define  USBC_BP_CSR0_D_SEND_STALL				5
#define  USBC_BP_CSR0_D_SETUP_END				4
#define  USBC_BP_CSR0_D_DATA_END				3
#define  USBC_BP_CSR0_D_SENT_STALL				2
#define  USBC_BP_CSR0_D_TX_PKT_READY			1
#define  USBC_BP_CSR0_D_RX_PKT_READY			0

/* Tx ep Control and Status Register for Host only */
#define  USBC_BP_TXCSR_H_AUTOSET				15
#define  USBC_BP_TXCSR_H_RESERVED				14
#define  USBC_BP_TXCSR_H_MODE				    13
#define  USBC_BP_TXCSR_H_DMA_REQ_EN			 	12
#define  USBC_BP_TXCSR_H_FORCE_DATA_TOGGLE		11
#define  USBC_BP_TXCSR_H_DMA_REQ_MODE			10
#define  USBC_BP_TXCSR_H_NAK_TIMEOUT			7
#define  USBC_BP_TXCSR_H_CLEAR_DATA_TOGGLE		6
#define  USBC_BP_TXCSR_H_TX_STALL				5
#define  USBC_BP_TXCSR_H_FLUSH_FIFO				3
#define  USBC_BP_TXCSR_H_ERROR					2
#define  USBC_BP_TXCSR_H_FIFO_NOT_EMPTY 		1
#define  USBC_BP_TXCSR_H_TX_READY				0

/* Tx ep Control and Status Register for Device only */
#define  USBC_BP_TXCSR_D_AUTOSET				15
#define  USBC_BP_TXCSR_D_ISO					14
#define  USBC_BP_TXCSR_D_MODE					13
#define  USBC_BP_TXCSR_D_DMA_REQ_EN				12
#define  USBC_BP_TXCSR_D_FORCE_DATA_TOGGLE		11
#define  USBC_BP_TXCSR_D_DMA_REQ_MODE			10
#define  USBC_BP_TXCSR_D_INCOMPLETE				7
#define  USBC_BP_TXCSR_D_CLEAR_DATA_TOGGLE		6
#define  USBC_BP_TXCSR_D_SENT_STALL				5
#define  USBC_BP_TXCSR_D_SEND_STALL				4
#define  USBC_BP_TXCSR_D_FLUSH_FIFO				3
#define  USBC_BP_TXCSR_D_UNDER_RUN				2
#define  USBC_BP_TXCSR_D_FIFO_NOT_EMPTY 		1
#define  USBC_BP_TXCSR_D_TX_READY				0

/* Rx Max Packet */
#define  USBC_BP_RXMAXP_PACKET_COUNT            11
#define  USBC_BP_RXMAXP_MAXIMUM_PAYLOAD         0

/* Rx ep Control and Status Register for Host only */
#define  USBC_BP_RXCSR_H_AUTO_CLEAR			    15
#define  USBC_BP_RXCSR_H_AUTO_REQ			    14
#define  USBC_BP_RXCSR_H_DMA_REQ_EN			    13
#define  USBC_BP_RXCSR_H_PID_ERROR			    12
#define  USBC_BP_RXCSR_H_DMA_REQ_MODE		    11

#define  USBC_BP_RXCSR_H_INCOMPLETE			    8
#define  USBC_BP_RXCSR_H_CLEAR_DATA_TOGGLE	    7
#define  USBC_BP_RXCSR_H_RX_STALL			    6
#define  USBC_BP_RXCSR_H_REQ_PACKET			    5
#define  USBC_BP_RXCSR_H_FLUSH_FIFO			    4
#define  USBC_BP_RXCSR_H_NAK_TIMEOUT		    3
#define  USBC_BP_RXCSR_H_ERROR				    2
#define  USBC_BP_RXCSR_H_FIFO_FULL			    1
#define  USBC_BP_RXCSR_H_RX_PKT_READY		    0

/* Rx ep Control and Status Register for Device only */
#define  USBC_BP_RXCSR_D_AUTO_CLEAR			    15
#define  USBC_BP_RXCSR_D_ISO				    14
#define  USBC_BP_RXCSR_D_DMA_REQ_EN			    13
#define  USBC_BP_RXCSR_D_DISABLE_NYET		    12
#define  USBC_BP_RXCSR_D_DMA_REQ_MODE		    11

#define  USBC_BP_RXCSR_D_INCOMPLETE			    8
#define  USBC_BP_RXCSR_D_CLEAR_DATA_TOGGLE	    7
#define  USBC_BP_RXCSR_D_SENT_STALL			    6
#define  USBC_BP_RXCSR_D_SEND_STALL			    5
#define  USBC_BP_RXCSR_D_FLUSH_FIFO			    4
#define  USBC_BP_RXCSR_D_DATA_ERROR			    3
#define  USBC_BP_RXCSR_D_OVERRUN			    2
#define  USBC_BP_RXCSR_D_FIFO_FULL			    1
#define  USBC_BP_RXCSR_D_RX_PKT_READY		    0

/* Tx Type Register for host only */
#define  USBC_BP_TXTYPE_SPEED	                6              //new
#define  USBC_BP_TXTYPE_PROROCOL	            4
#define  USBC_BP_TXTYPE_TARGET_EP_NUM           0

/* Rx Type Register for host only */
#define  USBC_BP_RXTYPE_SPEED		            6              //new
#define  USBC_BP_RXTYPE_PROROCOL	            4
#define  USBC_BP_RXTYPE_TARGET_EP_NUM           0

/* Core Configueation */
#define  USBC_BP_CONFIGDATA_MPRXE               7
#define  USBC_BP_CONFIGDATA_MPTXE               6
#define  USBC_BP_CONFIGDATA_BIGENDIAN		    5
#define  USBC_BP_CONFIGDATA_HBRXE			    4
#define  USBC_BP_CONFIGDATA_HBTXE			    3
#define  USBC_BP_CONFIGDATA_DYNFIFO_SIZING	    2
#define  USBC_BP_CONFIGDATA_SOFTCONE		    1
#define  USBC_BP_CONFIGDATA_UTMI_DATAWIDTH	    0

/* OTG Device Control */
#define  USBC_BP_DEVCTL_B_DEVICE			    7
#define  USBC_BP_DEVCTL_FS_DEV				    6
#define  USBC_BP_DEVCTL_LS_DEV				    5

#define  USBC_BP_DEVCTL_VBUS				    3
#define  USBC_BP_DEVCTL_HOST_MODE			    2
#define  USBC_BP_DEVCTL_HOST_REQ			    1
#define  USBC_BP_DEVCTL_SESSION				    0

/* Tx EP FIFO size control */
#define  USBC_BP_TXFIFOSZ_DPB				    4
#define  USBC_BP_TXFIFOSZ_SZ				    0

/* Rx EP FIFO size control */
#define  USBC_BP_RXFIFOSZ_DPB				    4
#define  USBC_BP_RXFIFOSZ_SZ				    0

/* vendor0 */
#define  USBC_BP_VEND0_DRQ_SEL				    1
#define  USBC_BP_VEND0_BUS_SEL				    0

/* hub address */
#define  USBC_BP_HADDR_MULTI_TT					7

/* Interface Status and Control */
#define  USBC_BP_ISCR_VBUS_VALID_FROM_DATA		30
#define  USBC_BP_ISCR_VBUS_VALID_FROM_VBUS		29
#define  USBC_BP_ISCR_EXT_ID_STATUS				28
#define  USBC_BP_ISCR_EXT_DM_STATUS				27
#define  USBC_BP_ISCR_EXT_DP_STATUS				26
#define  USBC_BP_ISCR_MERGED_VBUS_STATUS		25
#define  USBC_BP_ISCR_MERGED_ID_STATUS			24

#define  USBC_BP_ISCR_ID_PULLUP_EN				17
#define  USBC_BP_ISCR_DPDM_PULLUP_EN			16
#define  USBC_BP_ISCR_FORCE_ID					14
#define  USBC_BP_ISCR_FORCE_VBUS_VALID			12
#define  USBC_BP_ISCR_VBUS_VALID_SRC			10

#define  USBC_BP_ISCR_HOSC_EN                 	7
#define  USBC_BP_ISCR_VBUS_CHANGE_DETECT      	6
#define  USBC_BP_ISCR_ID_CHANGE_DETECT        	5
#define  USBC_BP_ISCR_DPDM_CHANGE_DETECT      	4
#define  USBC_BP_ISCR_IRQ_ENABLE              	3
#define  USBC_BP_ISCR_VBUS_CHANGE_DETECT_EN   	2
#define  USBC_BP_ISCR_ID_CHANGE_DETECT_EN     	1
#define  USBC_BP_ISCR_DPDM_CHANGE_DETECT_EN   	0


#define SUNXI_EHCI_AHB_ICHR8_EN		(1 << 10)
#define SUNXI_EHCI_AHB_INCR4_BURST_EN	(1 << 9)
#define SUNXI_EHCI_AHB_INCRX_ALIGN_EN	(1 << 8)
#define SUNXI_EHCI_ULPI_BYPASS_EN	(1 << 0)
//-----------------------------------------------------------------------
//   �Զ���
//-----------------------------------------------------------------------

/* usb��Դ���� */
#define  USBC_MAX_CTL_NUM                   1
#define  USBC_MAX_EP_NUM                    3   //ep0~2, ep�ĸ���
#define  USBC_MAX_FIFO_SIZE                 (2 * 1024)

/* usb OTG mode */
#define  USBC_OTG_HOST                      0
#define  USBC_OTG_DEVICE                    1

/* usb device type */
#define  USBC_DEVICE_HSDEV                  0
#define  USBC_DEVICE_FSDEV                  1
#define  USBC_DEVICE_LSDEV                  2

/*  usb transfer type  */
#define  USBC_TS_TYPE_IDLE                  0
#define  USBC_TS_TYPE_CTRL                  1
#define  USBC_TS_TYPE_ISO                   2
#define  USBC_TS_TYPE_INT                   3
#define  USBC_TS_TYPE_BULK                  4

/*  usb transfer mode  */
#define  USBC_TS_MODE_UNKOWN                0
#define  USBC_TS_MODE_LS                    1
#define  USBC_TS_MODE_FS                    2
#define  USBC_TS_MODE_HS                    3

/* usb Vbus status */
#define  USBC_VBUS_STATUS_BELOW_SESSIONEND                 0
#define  USBC_VBUS_STATUS_ABOVE_SESSIONEND_BELOW_AVALID    1
#define  USBC_VBUS_STATUS_ABOVE_AVALID_BELOW_VBUSVALID     2
#define  USBC_VBUS_STATUS_ABOVE_VBUSVALID                  3

/* usb io type */
#define  USBC_IO_TYPE_PIO    		        0
#define  USBC_IO_TYPE_DMA    		        1

/* usb ep type */
#define  USBC_EP_TYPE_IDLE    		        0
#define  USBC_EP_TYPE_EP0    		        1
#define  USBC_EP_TYPE_TX     		        2
#define  USBC_EP_TYPE_RX     		        3

/* usb id type */
#define  USBC_ID_TYPE_DISABLE      	        0
#define  USBC_ID_TYPE_HOST         	        1
#define  USBC_ID_TYPE_DEVICE       	        2

/* usb vbus valid type */
#define  USBC_VBUS_TYPE_DISABLE    	        0
#define  USBC_VBUS_TYPE_LOW       	        1
#define  USBC_VBUS_TYPE_HIGH       	        2

/* usb a valid source */
#define  USBC_A_VALID_SOURCE_UTMI_AVALID	0
#define  USBC_A_VALID_SOURCE_UTMI_VBUS    	1

/* usb device switch */
#define  USBC_DEVICE_SWITCH_OFF             0
#define  USBC_DEVICE_SWITCH_ON              1

/* usb fifo config mode */
#define  USBC_FIFO_MODE_4K                  0
#define  USBC_FIFO_MODE_8K                  1

/*
 **************************************************
 *  usb interrupt mask
 *
 **************************************************
 */

/* interrupt flags for ep0 and the Tx ep1~4 */
#define  USBC_INTTx_FLAG_EP5				    (1 << USBC_BP_INTTx_FLAG_EP5)
#define  USBC_INTTx_FLAG_EP4				    (1 << USBC_BP_INTTx_FLAG_EP4)
#define  USBC_INTTx_FLAG_EP3				    (1 << USBC_BP_INTTx_FLAG_EP3)
#define  USBC_INTTx_FLAG_EP2		    		(1 << USBC_BP_INTTx_FLAG_EP2)
#define  USBC_INTTx_FLAG_EP1  				    (1 << USBC_BP_INTTx_FLAG_EP1)
#define  USBC_INTTx_FLAG_EP0					(1 << USBC_BP_INTTx_FLAG_EP0)

/* interrupt flags for Rx ep1~4 */
#define  USBC_INTRx_FLAG_EP5				    (1 << USBC_BP_INTRx_FLAG_EP5)
#define  USBC_INTRx_FLAG_EP4				    (1 << USBC_BP_INTRx_FLAG_EP4)
#define  USBC_INTRx_FLAG_EP3				    (1 << USBC_BP_INTRx_FLAG_EP3)
#define  USBC_INTRx_FLAG_EP2				    (1 << USBC_BP_INTRx_FLAG_EP2)
#define  USBC_INTRx_FLAG_EP1				    (1 << USBC_BP_INTRx_FLAG_EP1)

/* USB interrupt */
#define  USBC_INTUSB_VBUS_ERROR				    (1 << USBC_BP_INTUSB_VBUS_ERROR)
#define  USBC_INTUSB_SESSION_REQ				(1 << USBC_BP_INTUSB_SESSION_REQ)
#define  USBC_INTUSB_DISCONNECT				    (1 << USBC_BP_INTUSB_DISCONNECT)
#define  USBC_INTUSB_CONNECT					(1 << USBC_BP_INTUSB_CONNECT)
#define  USBC_INTUSB_SOF						(1 << USBC_BP_INTUSB_SOF)
#define  USBC_INTUSB_RESET					    (1 << USBC_BP_INTUSB_RESET)
#define  USBC_INTUSB_RESUME					    (1 << USBC_BP_INTUSB_RESUME)
#define  USBC_INTUSB_SUSPEND					(1 << USBC_BP_INTUSB_SUSPEND)

#define USB_CSRL0_NAKTO         0x00000080  // NAK Timeout
#define USB_CSRL0_SETENDC       0x00000080  // Setup End Clear
#define USB_CSRL0_STATUS        0x00000040  // STATUS Packet
#define USB_CSRL0_RXRDYC        0x00000040  // RXRDY Clear
#define USB_CSRL0_REQPKT        0x00000020  // Request Packet
#define USB_CSRL0_STALL         0x00000020  // Send Stall
#define USB_CSRL0_SETEND        0x00000010  // Setup End
#define USB_CSRL0_ERROR         0x00000010  // Error
#define USB_CSRL0_DATAEND       0x00000008  // Data End
#define USB_CSRL0_SETUP         0x00000008  // Setup Packet
#define USB_CSRL0_STALLED       0x00000004  // Endpoint Stalled
#define USB_CSRL0_TXRDY         0x00000002  // Transmit Packet Ready
#define USB_CSRL0_RXRDY         0x00000001  // Receive Packet Ready

#define USB_RXFIFOSZ_DPB        0x00000010  // Double Packet Buffer Support
#define USB_RXFIFOSZ_SIZE_M     0x0000000F  // Max Packet Size

#define USB_TXFIFOSZ_DPB        0x00000010  // Double Packet Buffer Support
#define USB_TXFIFOSZ_SIZE_M     0x0000000F  // Max Packet Size

#endif
