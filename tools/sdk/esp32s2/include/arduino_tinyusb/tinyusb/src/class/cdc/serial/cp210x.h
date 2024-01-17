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

#ifndef TUSB_CP210X_H
#define TUSB_CP210X_H

// Protocol details can be found at AN571: CP210x Virtual COM Port Interface
// https://www.silabs.com/documents/public/application-notes/AN571.pdf

#define TU_CP210X_VID 0x10C4

/* Config request codes */
#define CP210X_IFC_ENABLE      0x00
#define CP210X_SET_BAUDDIV     0x01
#define CP210X_GET_BAUDDIV     0x02
#define CP210X_SET_LINE_CTL    0x03 // Set parity, data bits, stop bits
#define CP210X_GET_LINE_CTL    0x04
#define CP210X_SET_BREAK       0x05
#define CP210X_IMM_CHAR        0x06
#define CP210X_SET_MHS         0x07 // Set DTR, RTS
#define CP210X_GET_MDMSTS      0x08 // Get modem status (DTR, RTS, CTS, DSR, RI, DCD)
#define CP210X_SET_XON         0x09
#define CP210X_SET_XOFF        0x0A
#define CP210X_SET_EVENTMASK   0x0B
#define CP210X_GET_EVENTMASK   0x0C
#define CP210X_SET_CHAR        0x0D
#define CP210X_GET_CHARS       0x0E
#define CP210X_GET_PROPS       0x0F
#define CP210X_GET_COMM_STATUS 0x10
#define CP210X_RESET           0x11
#define CP210X_PURGE           0x12
#define CP210X_SET_FLOW        0x13
#define CP210X_GET_FLOW        0x14
#define CP210X_EMBED_EVENTS    0x15
#define CP210X_GET_EVENTSTATE  0x16
#define CP210X_SET_CHARS       0x19
#define CP210X_GET_BAUDRATE    0x1D
#define CP210X_SET_BAUDRATE    0x1E
#define CP210X_VENDOR_SPECIFIC 0xFF // GPIO, Recipient must be Device

#endif //TUSB_CP210X_H
