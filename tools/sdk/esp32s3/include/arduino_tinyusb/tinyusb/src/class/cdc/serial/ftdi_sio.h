/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 Ha Thach (thach@tinyusb.org) for Adafruit Industries
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
 */

#ifndef TUSB_FTDI_SIO_H
#define TUSB_FTDI_SIO_H

// VID/PID for matching FTDI devices
#define TU_FTDI_VID 0x0403
#define TU_FTDI_PID_LIST \
  0x6001, 0x6006, 0x6010, 0x6011, 0x6014, 0x6015, 0x8372, 0xFBFA, \
  0xcd18

// Commands
#define FTDI_SIO_RESET             	0    /* Reset the port */
#define FTDI_SIO_MODEM_CTRL        	1    /* Set the modem control register */
#define FTDI_SIO_SET_FLOW_CTRL     	2    /* Set flow control register */
#define FTDI_SIO_SET_BAUD_RATE     	3    /* Set baud rate */
#define FTDI_SIO_SET_DATA          	4    /* Set the data characteristics of the port */
#define FTDI_SIO_GET_MODEM_STATUS  	5    /* Retrieve current value of modem status register */
#define FTDI_SIO_SET_EVENT_CHAR    	6    /* Set the event character */
#define FTDI_SIO_SET_ERROR_CHAR    	7    /* Set the error character */
#define FTDI_SIO_SET_LATENCY_TIMER 	9    /* Set the latency timer */
#define FTDI_SIO_GET_LATENCY_TIMER 	0x0a /* Get the latency timer */
#define FTDI_SIO_SET_BITMODE       	0x0b /* Set bitbang mode */
#define FTDI_SIO_READ_PINS         	0x0c /* Read immediate value of pins */
#define FTDI_SIO_READ_EEPROM       	0x90 /* Read EEPROM */

/* FTDI_SIO_RESET */
#define FTDI_SIO_RESET_SIO 0
#define FTDI_SIO_RESET_PURGE_RX 1
#define FTDI_SIO_RESET_PURGE_TX 2

/*
 * BmRequestType:  0100 0000B
 * bRequest:       FTDI_SIO_RESET
 * wValue:         Control Value
 *                   0 = Reset SIO
 *                   1 = Purge RX buffer
 *                   2 = Purge TX buffer
 * wIndex:         Port
 * wLength:        0
 * Data:           None
 *
 * The Reset SIO command has this effect:
 *
 *    Sets flow control set to 'none'
 *    Event char = $0D
 *    Event trigger = disabled
 *    Purge RX buffer
 *    Purge TX buffer
 *    Clear DTR
 *    Clear RTS
 *    baud and data format not reset
 *
 * The Purge RX and TX buffer commands affect nothing except the buffers
 *
   */

/* FTDI_SIO_MODEM_CTRL */
/*
 * BmRequestType:   0100 0000B
 * bRequest:        FTDI_SIO_MODEM_CTRL
 * wValue:          ControlValue (see below)
 * wIndex:          Port
 * wLength:         0
 * Data:            None
 *
 * NOTE: If the device is in RTS/CTS flow control, the RTS set by this
 * command will be IGNORED without an error being returned
 * Also - you can not set DTR and RTS with one control message
 */

#define FTDI_SIO_SET_DTR_MASK 0x1
#define FTDI_SIO_SET_DTR_HIGH ((FTDI_SIO_SET_DTR_MASK << 8) | 1)
#define FTDI_SIO_SET_DTR_LOW  ((FTDI_SIO_SET_DTR_MASK << 8) | 0)
#define FTDI_SIO_SET_RTS_MASK 0x2
#define FTDI_SIO_SET_RTS_HIGH ((FTDI_SIO_SET_RTS_MASK << 8) | 2)
#define FTDI_SIO_SET_RTS_LOW  ((FTDI_SIO_SET_RTS_MASK << 8) | 0)

/*
 * ControlValue
 * B0    DTR state
 *          0 = reset
 *          1 = set
 * B1    RTS state
 *          0 = reset
 *          1 = set
 * B2..7 Reserved
 * B8    DTR state enable
 *          0 = ignore
 *          1 = use DTR state
 * B9    RTS state enable
 *          0 = ignore
 *          1 = use RTS state
 * B10..15 Reserved
 */

/* FTDI_SIO_SET_FLOW_CTRL */
#define FTDI_SIO_DISABLE_FLOW_CTRL 0x0
#define FTDI_SIO_RTS_CTS_HS (0x1 << 8)
#define FTDI_SIO_DTR_DSR_HS (0x2 << 8)
#define FTDI_SIO_XON_XOFF_HS (0x4 << 8)

/*
 *   BmRequestType:  0100 0000b
 *   bRequest:       FTDI_SIO_SET_FLOW_CTRL
 *   wValue:         Xoff/Xon
 *   wIndex:         Protocol/Port - hIndex is protocol / lIndex is port
 *   wLength:        0
 *   Data:           None
 *
 * hIndex protocol is:
 *   B0 Output handshaking using RTS/CTS
 *       0 = disabled
 *       1 = enabled
 *   B1 Output handshaking using DTR/DSR
 *       0 = disabled
 *       1 = enabled
 *   B2 Xon/Xoff handshaking
 *       0 = disabled
 *       1 = enabled
 *
 * A value of zero in the hIndex field disables handshaking
 *
 * If Xon/Xoff handshaking is specified, the hValue field should contain the
 * XOFF character and the lValue field contains the XON character.
 */

/* FTDI_SIO_SET_BAUD_RATE */
/*
 * BmRequestType:  0100 0000B
 * bRequest:       FTDI_SIO_SET_BAUDRATE
 * wValue:         BaudDivisor value - see below
 * wIndex:         Port
 * wLength:        0
 * Data:           None
 * The BaudDivisor values are calculated as follows (too complicated):
 */

/* FTDI_SIO_SET_DATA */
#define FTDI_SIO_SET_DATA_PARITY_NONE	(0x0 << 8)
#define FTDI_SIO_SET_DATA_PARITY_ODD	(0x1 << 8)
#define FTDI_SIO_SET_DATA_PARITY_EVEN	(0x2 << 8)
#define FTDI_SIO_SET_DATA_PARITY_MARK	(0x3 << 8)
#define FTDI_SIO_SET_DATA_PARITY_SPACE	(0x4 << 8)
#define FTDI_SIO_SET_DATA_STOP_BITS_1	(0x0 << 11)
#define FTDI_SIO_SET_DATA_STOP_BITS_15	(0x1 << 11)
#define FTDI_SIO_SET_DATA_STOP_BITS_2	(0x2 << 11)
#define FTDI_SIO_SET_BREAK		(0x1 << 14)

/*
 * BmRequestType:  0100 0000B
 * bRequest:       FTDI_SIO_SET_DATA
 * wValue:         Data characteristics (see below)
 * wIndex:         Port
 * wLength:        0
 * Data:           No
 *
 * Data characteristics
 *
 *   B0..7   Number of data bits
 *   B8..10  Parity
 *           0 = None
 *           1 = Odd
 *           2 = Even
 *           3 = Mark
 *           4 = Space
 *   B11..13 Stop Bits
 *           0 = 1
 *           1 = 1.5
 *           2 = 2
 *   B14
 *           1 = TX ON (break)
 *           0 = TX OFF (normal state)
 *   B15 Reserved
 *
 */

/*
* DATA FORMAT
*
* IN Endpoint
*
* The device reserves the first two bytes of data on this endpoint to contain
* the current values of the modem and line status registers. In the absence of
* data, the device generates a message consisting of these two status bytes
  * every 40 ms
  *
  * Byte 0: Modem Status
*
* Offset	Description
* B0	Reserved - must be 1
* B1	Reserved - must be 0
* B2	Reserved - must be 0
* B3	Reserved - must be 0
* B4	Clear to Send (CTS)
* B5	Data Set Ready (DSR)
* B6	Ring Indicator (RI)
* B7	Receive Line Signal Detect (RLSD)
*
* Byte 1: Line Status
*
* Offset	Description
* B0	Data Ready (DR)
* B1	Overrun Error (OE)
* B2	Parity Error (PE)
* B3	Framing Error (FE)
* B4	Break Interrupt (BI)
* B5	Transmitter Holding Register (THRE)
* B6	Transmitter Empty (TEMT)
* B7	Error in RCVR FIFO
*
*/
#define FTDI_RS0_CTS	(1 << 4)
#define FTDI_RS0_DSR	(1 << 5)
#define FTDI_RS0_RI	(1 << 6)
#define FTDI_RS0_RLSD	(1 << 7)

#define FTDI_RS_DR	1
#define FTDI_RS_OE	(1<<1)
#define FTDI_RS_PE	(1<<2)
#define FTDI_RS_FE	(1<<3)
#define FTDI_RS_BI	(1<<4)
#define FTDI_RS_THRE	(1<<5)
#define FTDI_RS_TEMT	(1<<6)
#define FTDI_RS_FIFO	(1<<7)

#endif //TUSB_FTDI_SIO_H
