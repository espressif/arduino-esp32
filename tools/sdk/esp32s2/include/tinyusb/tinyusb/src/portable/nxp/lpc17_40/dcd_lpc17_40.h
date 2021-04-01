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

#ifndef _TUSB_DCD_LPC175X_6X_H_
#define _TUSB_DCD_LPC175X_6X_H_

#include "common/tusb_common.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// Register Interface
//--------------------------------------------------------------------+

//------------- USB Interrupt USBIntSt -------------//
//enum {
//  DCD_USB_REQ_LOW_PRIO_MASK   = TU_BIT(0),
//  DCD_USB_REQ_HIGH_PRIO_MASK  = TU_BIT(1),
//  DCD_USB_REQ_DMA_MASK        = TU_BIT(2),
//  DCD_USB_REQ_NEED_CLOCK_MASK = TU_BIT(8),
//  DCD_USB_REQ_ENABLE_MASK     = TU_BIT(31)
//};

//------------- Device Interrupt USBDevInt -------------//
enum {
  DEV_INT_FRAME_MASK              = TU_BIT(0),
  DEV_INT_ENDPOINT_FAST_MASK      = TU_BIT(1),
  DEV_INT_ENDPOINT_SLOW_MASK      = TU_BIT(2),
  DEV_INT_DEVICE_STATUS_MASK      = TU_BIT(3),
  DEV_INT_COMMAND_CODE_EMPTY_MASK = TU_BIT(4),
  DEV_INT_COMMAND_DATA_FULL_MASK  = TU_BIT(5),
  DEV_INT_RX_ENDPOINT_PACKET_MASK = TU_BIT(6),
  DEV_INT_TX_ENDPOINT_PACKET_MASK = TU_BIT(7),
  DEV_INT_ENDPOINT_REALIZED_MASK  = TU_BIT(8),
  DEV_INT_ERROR_MASK              = TU_BIT(9)
};

//------------- DMA Interrupt USBDMAInt-------------//
enum {
  DMA_INT_END_OF_XFER_MASK    = TU_BIT(0),
  DMA_INT_NEW_DD_REQUEST_MASK = TU_BIT(1),
  DMA_INT_ERROR_MASK          = TU_BIT(2)
};

//------------- USBCtrl -------------//
enum {
  USBCTRL_READ_ENABLE_MASK  = TU_BIT(0),
  USBCTRL_WRITE_ENABLE_MASK = TU_BIT(1),
};

//------------- USBRxPLen -------------//
enum {
  USBRXPLEN_PACKET_LENGTH_MASK = (TU_BIT(10)-1),
  USBRXPLEN_DATA_VALID_MASK    = TU_BIT(10),
  USBRXPLEN_PACKET_READY_MASK  = TU_BIT(11),
};

//------------- SIE Command Code -------------//
typedef enum
{
  SIE_CMDPHASE_WRITE   = 1,
  SIE_CMDPHASE_READ    = 2,
  SIE_CMDPHASE_COMMAND = 5
} sie_cmdphase_t;

enum {
  // device commands
  SIE_CMDCODE_SET_ADDRESS                     = 0xd0,
  SIE_CMDCODE_CONFIGURE_DEVICE                = 0xd8,
  SIE_CMDCODE_SET_MODE                        = 0xf3,
  SIE_CMDCODE_READ_FRAME_NUMBER               = 0xf5,
  SIE_CMDCODE_READ_TEST_REGISTER              = 0xfd,
  SIE_CMDCODE_DEVICE_STATUS                   = 0xfe,
  SIE_CMDCODE_GET_ERROR                       = 0xff,
  SIE_CMDCODE_READ_ERROR_STATUS               = 0xfb,

  // endpoint commands
  SIE_CMDCODE_ENDPOINT_SELECT                 = 0x00, // + endpoint index
  SIE_CMDCODE_ENDPOINT_SELECT_CLEAR_INTERRUPT = 0x40, // + endpoint index, should use USBEpIntClr instead
  SIE_CMDCODE_ENDPOINT_SET_STATUS             = 0x40, // + endpoint index
  SIE_CMDCODE_BUFFER_CLEAR                    = 0xf2,
  SIE_CMDCODE_BUFFER_VALIDATE                 = 0xfa
};

//------------- SIE Device Status (get/set from SIE_CMDCODE_DEVICE_STATUS) -------------//
enum {
  SIE_DEV_STATUS_CONNECT_STATUS_MASK = TU_BIT(0),
  SIE_DEV_STATUS_CONNECT_CHANGE_MASK = TU_BIT(1),
  SIE_DEV_STATUS_SUSPEND_MASK        = TU_BIT(2),
  SIE_DEV_STATUS_SUSPEND_CHANGE_MASK = TU_BIT(3),
  SIE_DEV_STATUS_RESET_MASK          = TU_BIT(4)
};

//------------- SIE Select Endpoint Command -------------//
enum {
  SIE_SELECT_ENDPOINT_FULL_EMPTY_MASK         = TU_BIT(0), // 0: empty, 1 full. IN endpoint checks empty, OUT endpoint check full
  SIE_SELECT_ENDPOINT_STALL_MASK              = TU_BIT(1),
  SIE_SELECT_ENDPOINT_SETUP_RECEIVED_MASK     = TU_BIT(2), // clear by SIE_CMDCODE_ENDPOINT_SELECT_CLEAR_INTERRUPT
  SIE_SELECT_ENDPOINT_PACKET_OVERWRITTEN_MASK = TU_BIT(3), // previous packet is overwritten by a SETUP packet
  SIE_SELECT_ENDPOINT_NAK_MASK                = TU_BIT(4), // last packet response is NAK (auto clear by an ACK)
  SIE_SELECT_ENDPOINT_BUFFER1_FULL_MASK       = TU_BIT(5),
  SIE_SELECT_ENDPOINT_BUFFER2_FULL_MASK       = TU_BIT(6)
};

typedef enum
{
  SIE_SET_ENDPOINT_STALLED_MASK           = TU_BIT(0),
  SIE_SET_ENDPOINT_DISABLED_MASK          = TU_BIT(5),
  SIE_SET_ENDPOINT_RATE_FEEDBACK_MASK     = TU_BIT(6),
  SIE_SET_ENDPOINT_CONDITION_STALLED_MASK = TU_BIT(7),
}sie_endpoint_set_status_mask_t;

//------------- DMA Descriptor Status -------------//
enum {
  DD_STATUS_NOT_SERVICED = 0,
  DD_STATUS_BEING_SERVICED,
  DD_STATUS_NORMAL,
  DD_STATUS_DATA_UNDERUN, // short packet
  DD_STATUS_DATA_OVERRUN,
  DD_STATUS_SYSTEM_ERROR
};

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_DCD_LPC175X_6X_H_ */
