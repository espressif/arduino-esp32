/*
 * usbtmc.c
 *
 *  Created on: Sep 9, 2019
 *      Author: nconrad
 */

/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Nathan Conrad
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

/*
 * This library is not fully reentrant, though it is reentrant from the view
 * of either the application layer or the USB stack. Due to its locking,
 * it is not safe to call its functions from interrupts.
 *
 * The one exception is that its functions may not be called from the application
 * until the USB stack is initialized. This should not be a problem since the
 * device shouldn't be sending messages until it receives a request from the
 * host.
 */


/*
 * In the case of single-CPU "no OS", this task is never preempted other than by
 * interrupts, and the USBTMC code isn't called by interrupts, so all is OK. For "no OS",
 * the mutex structure's main effect is to disable the USB interrupts.
 * With an OS, this class driver uses the OSAL to perform locking. The code uses a single lock
 * and does not call outside of this class with a lock held, so deadlocks won't happen.
 */

//Limitations:
// "vendor-specific" commands are not handled.
// Dealing with "termchar" must be handled by the application layer,
//    though additional error checking is does in this module.
// talkOnly and listenOnly are NOT supported. They're not permitted
// in USB488, anyway.

/* Supported:
 *
 * Notification pulse
 * Trigger
 * Read status byte (both by interrupt endpoint and control message)
 *
 */


// TODO:
// USBTMC 3.2.2 error conditions not strictly followed
// No local lock-out, REN, or GTL.
// Clear message available status byte at the correct time? (488 4.3.1.3)


#include "tusb_option.h"

#if (TUSB_OPT_DEVICE_ENABLED && CFG_TUD_USBTMC)

#include "device/usbd.h"
#include "device/usbd_pvt.h"

#include "usbtmc_device.h"

#ifdef xDEBUG
#include "uart_util.h"
static char logMsg[150];
#endif

/*
 * The state machine does not allow simultaneous reading and writing. This is
 * consistent with USBTMC.
 */

typedef enum
{
  STATE_CLOSED,  // Endpoints have not yet been opened since USB reset
  STATE_NAK,     // Bulk-out endpoint is in NAK state.
  STATE_IDLE,    // Bulk-out endpoint is waiting for CMD.
  STATE_RCV,     // Bulk-out is receiving DEV_DEP message
  STATE_TX_REQUESTED,
  STATE_TX_INITIATED,
  STATE_TX_SHORTED,
  STATE_CLEARING,
  STATE_ABORTING_BULK_IN,
  STATE_ABORTING_BULK_IN_SHORTED, // aborting, and short packet has been queued for transmission
  STATE_ABORTING_BULK_IN_ABORTED, // aborting, and short packet has been transmitted
  STATE_ABORTING_BULK_OUT,
  STATE_NUM_STATES
} usbtmcd_state_enum;

#if (CFG_TUD_USBTMC_ENABLE_488)
  typedef usbtmc_response_capabilities_488_t usbtmc_capabilities_specific_t;
#else
  typedef usbtmc_response_capabilities_t usbtmc_capabilities_specific_t;
#endif


typedef struct
{
  volatile usbtmcd_state_enum state;

  uint8_t itf_id;
  uint8_t rhport;
  uint8_t ep_bulk_in;
  uint8_t ep_bulk_out;
  uint8_t ep_int_in;
  // IN buffer is only used for first packet, not the remainder
  // in order to deal with prepending header
  CFG_TUSB_MEM_ALIGN uint8_t ep_bulk_in_buf[USBTMCD_MAX_PACKET_SIZE];
  // OUT buffer receives one packet at a time
  CFG_TUSB_MEM_ALIGN uint8_t ep_bulk_out_buf[USBTMCD_MAX_PACKET_SIZE];
  uint32_t transfer_size_remaining; // also used for requested length for bulk IN.
  uint32_t transfer_size_sent;      // To keep track of data bytes that have been queued in FIFO (not header bytes)

  uint8_t lastBulkOutTag; // used for aborts (mostly)
  uint8_t lastBulkInTag; // used for aborts (mostly)

  uint8_t const * devInBuffer; // pointer to application-layer used for transmissions

  usbtmc_capabilities_specific_t const * capabilities;
} usbtmc_interface_state_t;

CFG_TUSB_MEM_SECTION static usbtmc_interface_state_t usbtmc_state =
{
    .itf_id = 0xFF,
};

// We need all headers to fit in a single packet in this implementation.
TU_VERIFY_STATIC(USBTMCD_MAX_PACKET_SIZE >= 32u,"USBTMC dev EP packet size too small");
TU_VERIFY_STATIC(
    (sizeof(usbtmc_state.ep_bulk_in_buf) % USBTMCD_MAX_PACKET_SIZE) == 0,
    "packet buffer must be a multiple of the packet size");

static bool handle_devMsgOutStart(uint8_t rhport, void *data, size_t len);
static bool handle_devMsgOut(uint8_t rhport, void *data, size_t len, size_t packetLen);

static uint8_t termChar;
static uint8_t termCharRequested = false;


osal_mutex_def_t usbtmcLockBuffer;
static osal_mutex_t usbtmcLock;

// Our own private lock, mostly for the state variable.
#define criticalEnter() do {osal_mutex_lock(usbtmcLock,OSAL_TIMEOUT_WAIT_FOREVER); } while (0)
#define criticalLeave() do {osal_mutex_unlock(usbtmcLock); } while (0)

bool atomicChangeState(usbtmcd_state_enum expectedState, usbtmcd_state_enum newState)
{
  bool ret = true;
  criticalEnter();
  usbtmcd_state_enum oldState = usbtmc_state.state;
  if (oldState == expectedState)
  {
    usbtmc_state.state = newState;
  }
  else
  {
    ret = false;
  }
  criticalLeave();
  return ret;
}

// called from app
// We keep a reference to the buffer, so it MUST not change until the app is
// notified that the transfer is complete.
// length of data is specified in the hdr.

// We can't just send the whole thing at once because we need to concatanate the
// header with the data.
bool tud_usbtmc_transmit_dev_msg_data(
    const void * data, size_t len,
    bool endOfMessage,
    bool usingTermChar)
{
  const unsigned int txBufLen = sizeof(usbtmc_state.ep_bulk_in_buf);

#ifndef NDEBUG
  TU_ASSERT(len > 0u);
  TU_ASSERT(len <= usbtmc_state.transfer_size_remaining);
  TU_ASSERT(usbtmc_state.transfer_size_sent == 0u);
  if(usingTermChar)
  {
    TU_ASSERT(usbtmc_state.capabilities->bmDevCapabilities.canEndBulkInOnTermChar);
    TU_ASSERT(termCharRequested);
    TU_ASSERT(((uint8_t*)data)[len-1u] == termChar);
  }
#endif

  TU_VERIFY(usbtmc_state.state == STATE_TX_REQUESTED);
  usbtmc_msg_dev_dep_msg_in_header_t *hdr = (usbtmc_msg_dev_dep_msg_in_header_t*)usbtmc_state.ep_bulk_in_buf;
  tu_varclr(hdr);
  hdr->header.MsgID = USBTMC_MSGID_DEV_DEP_MSG_IN;
  hdr->header.bTag = usbtmc_state.lastBulkInTag;
  hdr->header.bTagInverse = (uint8_t)~(usbtmc_state.lastBulkInTag);
  hdr->TransferSize = len;
  hdr->bmTransferAttributes.EOM = endOfMessage;
  hdr->bmTransferAttributes.UsingTermChar = usingTermChar;

  // Copy in the header
  const size_t headerLen = sizeof(*hdr);
  const size_t dataLen = ((headerLen + hdr->TransferSize) <= txBufLen) ?
                            len : (txBufLen - headerLen);
  const size_t packetLen = headerLen + dataLen;

  memcpy((uint8_t*)(usbtmc_state.ep_bulk_in_buf) + headerLen, data, dataLen);
  usbtmc_state.transfer_size_remaining = len - dataLen;
  usbtmc_state.transfer_size_sent = dataLen;
  usbtmc_state.devInBuffer = (uint8_t*)data + (dataLen);

  bool stateChanged =
      atomicChangeState(STATE_TX_REQUESTED, (packetLen >= txBufLen) ? STATE_TX_INITIATED : STATE_TX_SHORTED);
  TU_VERIFY(stateChanged);
  TU_VERIFY(usbd_edpt_xfer(usbtmc_state.rhport, usbtmc_state.ep_bulk_in, usbtmc_state.ep_bulk_in_buf, (uint16_t)packetLen));
  return true;
}

void usbtmcd_init_cb(void)
{
  usbtmc_state.capabilities = tud_usbtmc_get_capabilities_cb();
#ifndef NDEBUG
# if CFG_TUD_USBTMC_ENABLE_488
    if(usbtmc_state.capabilities->bmIntfcCapabilities488.supportsTrigger)
      TU_ASSERT(&tud_usbtmc_msg_trigger_cb != NULL,);
      // Per USB488 spec: table 8
      TU_ASSERT(!usbtmc_state.capabilities->bmIntfcCapabilities.listenOnly,);
      TU_ASSERT(!usbtmc_state.capabilities->bmIntfcCapabilities.talkOnly,);
# endif
    if(usbtmc_state.capabilities->bmIntfcCapabilities.supportsIndicatorPulse)
      TU_ASSERT(&tud_usbtmc_indicator_pulse_cb != NULL,);
#endif

    usbtmcLock = osal_mutex_create(&usbtmcLockBuffer);
}

uint16_t usbtmcd_open_cb(uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t max_len)
{
  (void)rhport;

  uint16_t drv_len;
  uint8_t const * p_desc;
  uint8_t found_endpoints = 0;

  TU_VERIFY(itf_desc->bInterfaceClass    == TUD_USBTMC_APP_CLASS   , 0);
  TU_VERIFY(itf_desc->bInterfaceSubClass == TUD_USBTMC_APP_SUBCLASS, 0);

#ifndef NDEBUG
  // Only 2 or 3 endpoints are allowed for USBTMC.
  TU_ASSERT((itf_desc->bNumEndpoints == 2) || (itf_desc->bNumEndpoints ==3), 0);
#endif

  TU_ASSERT(usbtmc_state.state == STATE_CLOSED, 0);

  // Interface
  drv_len = 0u;
  p_desc = (uint8_t const *) itf_desc;

  usbtmc_state.itf_id = itf_desc->bInterfaceNumber;
  usbtmc_state.rhport = rhport;

  while (found_endpoints < itf_desc->bNumEndpoints && drv_len <= max_len)
  {
    if ( TUSB_DESC_ENDPOINT == p_desc[DESC_OFFSET_TYPE])
    {
      tusb_desc_endpoint_t const *ep_desc = (tusb_desc_endpoint_t const *)p_desc;
      switch(ep_desc->bmAttributes.xfer) {
        case TUSB_XFER_BULK:
          TU_ASSERT(ep_desc->wMaxPacketSize.size == USBTMCD_MAX_PACKET_SIZE, 0);
          if (tu_edpt_dir(ep_desc->bEndpointAddress) == TUSB_DIR_IN)
          {
            usbtmc_state.ep_bulk_in = ep_desc->bEndpointAddress;
          } else {
            usbtmc_state.ep_bulk_out = ep_desc->bEndpointAddress;
          }

          break;
        case TUSB_XFER_INTERRUPT:
#ifndef NDEBUG
          TU_ASSERT(tu_edpt_dir(ep_desc->bEndpointAddress) == TUSB_DIR_IN, 0);
          TU_ASSERT(usbtmc_state.ep_int_in == 0, 0);
#endif
          usbtmc_state.ep_int_in = ep_desc->bEndpointAddress;
          break;
        default:
          TU_ASSERT(false, 0);
      }
      TU_ASSERT( usbd_edpt_open(rhport, ep_desc), 0);
      found_endpoints++;
    }

    drv_len += tu_desc_len(p_desc);
    p_desc   = tu_desc_next(p_desc);
  }

  // bulk endpoints are required, but interrupt IN is optional
#ifndef NDEBUG
  TU_ASSERT(usbtmc_state.ep_bulk_in  != 0, 0);
  TU_ASSERT(usbtmc_state.ep_bulk_out != 0, 0);
  if (itf_desc->bNumEndpoints == 2)
  {
    TU_ASSERT(usbtmc_state.ep_int_in == 0, 0);
  }
  else if (itf_desc->bNumEndpoints == 3)
  {
    TU_ASSERT(usbtmc_state.ep_int_in != 0, 0);
  }
#if (CFG_TUD_USBTMC_ENABLE_488)
  if(usbtmc_state.capabilities->bmIntfcCapabilities488.is488_2 ||
      usbtmc_state.capabilities->bmDevCapabilities488.SR1)
  {
    TU_ASSERT(usbtmc_state.ep_int_in != 0, 0);
  }
#endif
#endif
  atomicChangeState(STATE_CLOSED, STATE_NAK);
  tud_usbtmc_open_cb(itf_desc->iInterface);

  return drv_len;
}
// Tell USBTMC class to set its bulk-in EP to ACK so that it can
// receive USBTMC commands.
// Returns false if it was already in an ACK state or is busy
// processing a command (such as a clear). Returns true if it was
// in the NAK state and successfully transitioned to the ACK wait
// state.
bool tud_usbtmc_start_bus_read()
{
  usbtmcd_state_enum oldState = usbtmc_state.state;
  switch(oldState)
  {
  // These may transition to IDLE
  case STATE_NAK:
  case STATE_ABORTING_BULK_IN_ABORTED:
    TU_VERIFY(atomicChangeState(oldState, STATE_IDLE));
    break;
  // When receiving, let it remain receiving
  case STATE_RCV:
    break;
  default:
    TU_VERIFY(false);
  }
  TU_VERIFY(usbd_edpt_xfer(usbtmc_state.rhport, usbtmc_state.ep_bulk_out, usbtmc_state.ep_bulk_out_buf, 64));
  return true;
}

void usbtmcd_reset_cb(uint8_t rhport)
{
  (void)rhport;
  usbtmc_capabilities_specific_t const * capabilities = tud_usbtmc_get_capabilities_cb();

  criticalEnter();
  tu_varclr(&usbtmc_state);
  usbtmc_state.capabilities = capabilities;
  usbtmc_state.itf_id = 0xFFu;
  criticalLeave();
}

static bool handle_devMsgOutStart(uint8_t rhport, void *data, size_t len)
{
  (void)rhport;
  // return true upon failure, as we can assume error is being handled elsewhere.
  TU_VERIFY(atomicChangeState(STATE_IDLE, STATE_RCV), true);
  usbtmc_state.transfer_size_sent = 0u;

  // must be a header, should have been confirmed before calling here.
  usbtmc_msg_request_dev_dep_out *msg = (usbtmc_msg_request_dev_dep_out*)data;
  usbtmc_state.transfer_size_remaining = msg->TransferSize;
  TU_VERIFY(tud_usbtmc_msgBulkOut_start_cb(msg));

  TU_VERIFY(handle_devMsgOut(rhport, (uint8_t*)data + sizeof(*msg), len - sizeof(*msg), len));
  usbtmc_state.lastBulkOutTag = msg->header.bTag;
  return true;
}

static bool handle_devMsgOut(uint8_t rhport, void *data, size_t len, size_t packetLen)
{
  (void)rhport;
  // return true upon failure, as we can assume error is being handled elsewhere.
  TU_VERIFY(usbtmc_state.state == STATE_RCV,true);

  bool shortPacket = (packetLen < USBTMCD_MAX_PACKET_SIZE);

  // Packet is to be considered complete when we get enough data or at a short packet.
  bool atEnd = false;
  if(len >= usbtmc_state.transfer_size_remaining || shortPacket)
  {
    atEnd = true;
    TU_VERIFY(atomicChangeState(STATE_RCV, STATE_NAK));
  }

  len = tu_min32(len, usbtmc_state.transfer_size_remaining);

  usbtmc_state.transfer_size_remaining -= len;
  usbtmc_state.transfer_size_sent += len;

  // App may (should?) call the wait_for_bus() command at this point
  if(!tud_usbtmc_msg_data_cb(data, len, atEnd))
  {
    // TODO: Go to an error state upon failure other than just stalling the EP?
    return false;
  }


  return true;
}

static bool handle_devMsgIn(void *data, size_t len)
{
  TU_VERIFY(len == sizeof(usbtmc_msg_request_dev_dep_in));
  usbtmc_msg_request_dev_dep_in *msg = (usbtmc_msg_request_dev_dep_in*)data;
  bool stateChanged = atomicChangeState(STATE_IDLE, STATE_TX_REQUESTED);
  TU_VERIFY(stateChanged);
  usbtmc_state.lastBulkInTag = msg->header.bTag;
  usbtmc_state.transfer_size_remaining = msg->TransferSize;
  usbtmc_state.transfer_size_sent = 0u;

  termCharRequested = msg->bmTransferAttributes.TermCharEnabled;
  termChar = msg->TermChar;

  if(termCharRequested)
    TU_VERIFY(usbtmc_state.capabilities->bmDevCapabilities.canEndBulkInOnTermChar);

  TU_VERIFY(tud_usbtmc_msgBulkIn_request_cb(msg));
  return true;
}

bool usbtmcd_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes)
{
  TU_VERIFY(result == XFER_RESULT_SUCCESS);
  //uart_tx_str_sync("TMC XFER CB\r\n");
  if(usbtmc_state.state == STATE_CLEARING) {
    return true; /* I think we can ignore everything here */
  }

  if(ep_addr == usbtmc_state.ep_bulk_out)
  {
    usbtmc_msg_generic_t *msg = NULL;

    switch(usbtmc_state.state)
    {
    case STATE_IDLE:
      TU_VERIFY(xferred_bytes >= sizeof(usbtmc_msg_generic_t));
      msg = (usbtmc_msg_generic_t*)(usbtmc_state.ep_bulk_out_buf);
      uint8_t invInvTag = (uint8_t)~(msg->header.bTagInverse);
      TU_VERIFY(msg->header.bTag == invInvTag);
      TU_VERIFY(msg->header.bTag != 0x00);

      switch(msg->header.MsgID) {
      case USBTMC_MSGID_DEV_DEP_MSG_OUT:
        if(!handle_devMsgOutStart(rhport, msg, xferred_bytes))
        {
          usbd_edpt_stall(rhport, usbtmc_state.ep_bulk_out);
          TU_VERIFY(false);
        }
        break;

      case USBTMC_MSGID_DEV_DEP_MSG_IN:
        TU_VERIFY(handle_devMsgIn(msg, xferred_bytes));
        break;

#if (CFG_TUD_USBTMC_ENABLE_488)
      case USBTMC_MSGID_USB488_TRIGGER:
        // Spec says we halt the EP if we didn't declare we support it.
        TU_VERIFY(usbtmc_state.capabilities->bmIntfcCapabilities488.supportsTrigger);
        TU_VERIFY(tud_usbtmc_msg_trigger_cb(msg));

        break;
#endif
      case USBTMC_MSGID_VENDOR_SPECIFIC_MSG_OUT:
      case USBTMC_MSGID_VENDOR_SPECIFIC_IN:
      default:
        usbd_edpt_stall(rhport, usbtmc_state.ep_bulk_out);
        TU_VERIFY(false);
        return false;
      }
      return true;

    case STATE_RCV:
      if(!handle_devMsgOut(rhport, usbtmc_state.ep_bulk_out_buf, xferred_bytes, xferred_bytes))
      {
        usbd_edpt_stall(rhport, usbtmc_state.ep_bulk_out);
        TU_VERIFY(false);
      }
      return true;

    case STATE_ABORTING_BULK_OUT:
      TU_VERIFY(false);
      return false; // Should be stalled by now, shouldn't have received a packet.
    case STATE_TX_REQUESTED:
    case STATE_TX_INITIATED:
    case STATE_ABORTING_BULK_IN:
    case STATE_ABORTING_BULK_IN_SHORTED:
    case STATE_ABORTING_BULK_IN_ABORTED:
    default:

      TU_VERIFY(false);
    }
  }
  else if(ep_addr == usbtmc_state.ep_bulk_in)
  {
    switch(usbtmc_state.state) {
    case STATE_TX_SHORTED:
      TU_VERIFY(atomicChangeState(STATE_TX_SHORTED, STATE_NAK));
      TU_VERIFY(tud_usbtmc_msgBulkIn_complete_cb());
      break;

    case STATE_TX_INITIATED:
      if(usbtmc_state.transfer_size_remaining >=sizeof(usbtmc_state.ep_bulk_in_buf))
    {
        // FIXME! This removes const below!
        TU_VERIFY( usbd_edpt_xfer(rhport, usbtmc_state.ep_bulk_in,
            (void*)usbtmc_state.devInBuffer,sizeof(usbtmc_state.ep_bulk_in_buf)));
        usbtmc_state.devInBuffer += sizeof(usbtmc_state.ep_bulk_in_buf);
        usbtmc_state.transfer_size_remaining -= sizeof(usbtmc_state.ep_bulk_in_buf);
        usbtmc_state.transfer_size_sent += sizeof(usbtmc_state.ep_bulk_in_buf);
    }
    else // last packet
    {
      size_t packetLen = usbtmc_state.transfer_size_remaining;
      memcpy(usbtmc_state.ep_bulk_in_buf, usbtmc_state.devInBuffer, usbtmc_state.transfer_size_remaining);
        usbtmc_state.transfer_size_sent += sizeof(usbtmc_state.transfer_size_remaining);
      usbtmc_state.transfer_size_remaining = 0;
      usbtmc_state.devInBuffer = NULL;
      TU_VERIFY( usbd_edpt_xfer(rhport, usbtmc_state.ep_bulk_in, usbtmc_state.ep_bulk_in_buf,(uint16_t)packetLen));
        if(((packetLen % USBTMCD_MAX_PACKET_SIZE) != 0) || (packetLen == 0 ))
        {
          usbtmc_state.state = STATE_TX_SHORTED;
        }
      }
      return true;
    case STATE_ABORTING_BULK_IN:
      // need to send short packet  (ZLP?)
      TU_VERIFY( usbd_edpt_xfer(rhport, usbtmc_state.ep_bulk_in, usbtmc_state.ep_bulk_in_buf,(uint16_t)0u));
      usbtmc_state.state = STATE_ABORTING_BULK_IN_SHORTED;
      return true;
    case STATE_ABORTING_BULK_IN_SHORTED:
      /* Done. :)*/
      usbtmc_state.state = STATE_ABORTING_BULK_IN_ABORTED;
    return true;
    default:
      TU_ASSERT(false);
      return false;
    }
  }
  else if (ep_addr == usbtmc_state.ep_int_in) {
    // Good?
    return true;
  }
  return false;
}

// Invoked when a control transfer occurred on an interface of this class
// Driver response accordingly to the request and the transfer stage (setup/data/ack)
// return false to stall control endpoint (e.g unsupported request)
bool usbtmcd_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request)
{
  // nothing to do with DATA and ACK stage
  if ( stage != CONTROL_STAGE_SETUP ) return true;

  uint8_t tmcStatusCode = USBTMC_STATUS_FAILED;
#if (CFG_TUD_USBTMC_ENABLE_488)
  uint8_t bTag;
#endif

  if((request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD) &&
      (request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_ENDPOINT) &&
      (request->bRequest == TUSB_REQ_CLEAR_FEATURE) &&
      (request->wValue == TUSB_REQ_FEATURE_EDPT_HALT))
  {
    uint32_t ep_addr = (request->wIndex);

    if(ep_addr == usbtmc_state.ep_bulk_out)
    {
      criticalEnter();
      usbtmc_state.state = STATE_NAK; // USBD core has placed EP in NAK state for us
      criticalLeave();
      tud_usbtmc_bulkOut_clearFeature_cb();
    }
    else if (ep_addr == usbtmc_state.ep_bulk_in)
    {
      tud_usbtmc_bulkIn_clearFeature_cb();
    }
    else
    {
      return false;
    }
    return true;
  }

  // Otherwise, we only handle class requests.
  if(request->bmRequestType_bit.type != TUSB_REQ_TYPE_CLASS)
  {
    return false;
  }

  // Verification that we own the interface is unneeded since it's been routed to us specifically.

  switch(request->bRequest)
  {
  // USBTMC required requests
  case USBTMC_bREQUEST_INITIATE_ABORT_BULK_OUT:
  {
    usbtmc_initiate_abort_rsp_t rsp = {
        .bTag = usbtmc_state.lastBulkOutTag,
    };
    TU_VERIFY(request->bmRequestType == 0xA2); // in,class,interface
    TU_VERIFY(request->wLength == sizeof(rsp));
    TU_VERIFY(request->wIndex == usbtmc_state.ep_bulk_out);

    // wValue is the requested bTag to abort
    if(usbtmc_state.state != STATE_RCV)
    {
      rsp.USBTMC_status = USBTMC_STATUS_FAILED;
    }
    else if(usbtmc_state.lastBulkOutTag == (request->wValue & 0x7Fu))
    {
      rsp.USBTMC_status = USBTMC_STATUS_TRANSFER_NOT_IN_PROGRESS;
    }
    else
    {
      rsp.USBTMC_status = USBTMC_STATUS_SUCCESS;
      // Check if we've queued a short packet
      criticalEnter();
      usbtmc_state.state = STATE_ABORTING_BULK_OUT;
      criticalLeave();
      TU_VERIFY(tud_usbtmc_initiate_abort_bulk_out_cb(&(rsp.USBTMC_status)));
      usbd_edpt_stall(rhport, usbtmc_state.ep_bulk_out);
    }
    TU_VERIFY(tud_control_xfer(rhport, request, (void*)&rsp,sizeof(rsp)));
    return true;
  }

  case USBTMC_bREQUEST_CHECK_ABORT_BULK_OUT_STATUS:
  {
    usbtmc_check_abort_bulk_rsp_t rsp = {
        .USBTMC_status = USBTMC_STATUS_SUCCESS,
        .NBYTES_RXD_TXD = usbtmc_state.transfer_size_sent
    };
    TU_VERIFY(request->bmRequestType == 0xA2); // in,class,EP
    TU_VERIFY(request->wLength == sizeof(rsp));
    TU_VERIFY(request->wIndex == usbtmc_state.ep_bulk_out);
    TU_VERIFY(tud_usbtmc_check_abort_bulk_out_cb(&rsp));
    TU_VERIFY(tud_control_xfer(rhport, request, (void*)&rsp,sizeof(rsp)));
    return true;
  }

  case USBTMC_bREQUEST_INITIATE_ABORT_BULK_IN:
  {
    usbtmc_initiate_abort_rsp_t rsp = {
        .bTag = usbtmc_state.lastBulkInTag,
    };
    TU_VERIFY(request->bmRequestType == 0xA2); // in,class,interface
    TU_VERIFY(request->wLength == sizeof(rsp));
    TU_VERIFY(request->wIndex == usbtmc_state.ep_bulk_in);
    // wValue is the requested bTag to abort
    if((usbtmc_state.state == STATE_TX_REQUESTED || usbtmc_state.state == STATE_TX_INITIATED) &&
        usbtmc_state.lastBulkInTag == (request->wValue & 0x7Fu))
    {
      rsp.USBTMC_status = USBTMC_STATUS_SUCCESS;
    usbtmc_state.transfer_size_remaining = 0u;
      // Check if we've queued a short packet
      criticalEnter();
      usbtmc_state.state = ((usbtmc_state.transfer_size_sent % USBTMCD_MAX_PACKET_SIZE) == 0) ?
              STATE_ABORTING_BULK_IN : STATE_ABORTING_BULK_IN_SHORTED;
      criticalLeave();
      if(usbtmc_state.transfer_size_sent  == 0)
      {
        // Send short packet, nothing is in the buffer yet
        TU_VERIFY( usbd_edpt_xfer(rhport, usbtmc_state.ep_bulk_in, usbtmc_state.ep_bulk_in_buf,(uint16_t)0u));
        usbtmc_state.state = STATE_ABORTING_BULK_IN_SHORTED;
      }
      TU_VERIFY(tud_usbtmc_initiate_abort_bulk_in_cb(&(rsp.USBTMC_status)));
    }
    else if((usbtmc_state.state == STATE_TX_REQUESTED || usbtmc_state.state == STATE_TX_INITIATED))
    { // FIXME: Unsure how to check  if the OUT endpoint fifo is non-empty....
      rsp.USBTMC_status = USBTMC_STATUS_TRANSFER_NOT_IN_PROGRESS;
    }
    else
    {
      rsp.USBTMC_status = USBTMC_STATUS_FAILED;
    }
    TU_VERIFY(tud_control_xfer(rhport, request, (void*)&rsp,sizeof(rsp)));
    return true;
  }

  case USBTMC_bREQUEST_CHECK_ABORT_BULK_IN_STATUS:
  {
    TU_VERIFY(request->bmRequestType == 0xA2); // in,class,EP
    TU_VERIFY(request->wLength == 8u);

    usbtmc_check_abort_bulk_rsp_t rsp =
    {
        .USBTMC_status = USBTMC_STATUS_FAILED,
        .bmAbortBulkIn =
        {
            .BulkInFifoBytes = (usbtmc_state.state != STATE_ABORTING_BULK_IN_ABORTED)
        },
        .NBYTES_RXD_TXD = usbtmc_state.transfer_size_sent,
    };
    TU_VERIFY(tud_usbtmc_check_abort_bulk_in_cb(&rsp));
    criticalEnter();
    switch(usbtmc_state.state)
    {
    case STATE_ABORTING_BULK_IN_ABORTED:
      rsp.USBTMC_status = USBTMC_STATUS_SUCCESS;
      usbtmc_state.state = STATE_IDLE;
      break;
    case STATE_ABORTING_BULK_IN:
    case STATE_ABORTING_BULK_OUT:
      rsp.USBTMC_status = USBTMC_STATUS_PENDING;
      break;
    default:
      break;
    }
    criticalLeave();
    TU_VERIFY(tud_control_xfer(rhport, request, (void*)&rsp,sizeof(rsp)));

    return true;
  }

  case USBTMC_bREQUEST_INITIATE_CLEAR:
    {
      TU_VERIFY(request->bmRequestType == 0xA1); // in,class,interface
      TU_VERIFY(request->wLength == sizeof(tmcStatusCode));
      // After receiving an INITIATE_CLEAR request, the device must Halt the Bulk-OUT endpoint, queue the
      // control endpoint response shown in Table 31, and clear all input buffers and output buffers.
      usbd_edpt_stall(rhport, usbtmc_state.ep_bulk_out);
      usbtmc_state.transfer_size_remaining = 0;
      criticalEnter();
      usbtmc_state.state = STATE_CLEARING;
      criticalLeave();
      TU_VERIFY(tud_usbtmc_initiate_clear_cb(&tmcStatusCode));
      TU_VERIFY(tud_control_xfer(rhport, request, (void*)&tmcStatusCode,sizeof(tmcStatusCode)));
      return true;
    }

  case USBTMC_bREQUEST_CHECK_CLEAR_STATUS:
    {
      TU_VERIFY(request->bmRequestType == 0xA1); // in,class,interface
      usbtmc_get_clear_status_rsp_t clearStatusRsp = {0};
      TU_VERIFY(request->wLength == sizeof(clearStatusRsp));

      if(usbd_edpt_busy(rhport, usbtmc_state.ep_bulk_in))
      {
        // Stuff stuck in TX buffer?
        clearStatusRsp.bmClear.BulkInFifoBytes = 1;
        clearStatusRsp.USBTMC_status = USBTMC_STATUS_PENDING;
      }
      else
      {
        // Let app check if it's clear
        TU_VERIFY(tud_usbtmc_check_clear_cb(&clearStatusRsp));
      }
      if(clearStatusRsp.USBTMC_status == USBTMC_STATUS_SUCCESS)
      {
        criticalEnter();
        usbtmc_state.state = STATE_IDLE;
        criticalLeave();
      }
      TU_VERIFY(tud_control_xfer(rhport, request, (void*)&clearStatusRsp,sizeof(clearStatusRsp)));
      return true;
    }

  case USBTMC_bREQUEST_GET_CAPABILITIES:
    {
      TU_VERIFY(request->bmRequestType == 0xA1); // in,class,interface
      TU_VERIFY(request->wLength == sizeof(*(usbtmc_state.capabilities)));
      TU_VERIFY(tud_control_xfer(rhport, request, (void*)usbtmc_state.capabilities, sizeof(*usbtmc_state.capabilities)));
      return true;
    }
  // USBTMC Optional Requests

  case USBTMC_bREQUEST_INDICATOR_PULSE: // Optional
    {
      TU_VERIFY(request->bmRequestType == 0xA1); // in,class,interface
      TU_VERIFY(request->wLength == sizeof(tmcStatusCode));
      TU_VERIFY(usbtmc_state.capabilities->bmIntfcCapabilities.supportsIndicatorPulse);
      TU_VERIFY(tud_usbtmc_indicator_pulse_cb(request, &tmcStatusCode));
      TU_VERIFY(tud_control_xfer(rhport, request, (void*)&tmcStatusCode, sizeof(tmcStatusCode)));
      return true;
    }
#if (CFG_TUD_USBTMC_ENABLE_488)

    // USB488 required requests
  case USB488_bREQUEST_READ_STATUS_BYTE:
    {
      usbtmc_read_stb_rsp_488_t rsp;
      TU_VERIFY(request->bmRequestType == 0xA1); // in,class,interface
      TU_VERIFY(request->wLength == sizeof(rsp)); // in,class,interface

      bTag = request->wValue & 0x7F;
      TU_VERIFY(request->bmRequestType == 0xA1);
      TU_VERIFY((request->wValue & (~0x7F)) == 0u); // Other bits are required to be zero
      TU_VERIFY(bTag >= 0x02 && bTag <= 127);
      TU_VERIFY(request->wIndex == usbtmc_state.itf_id);
      TU_VERIFY(request->wLength == 0x0003);
      rsp.bTag = (uint8_t)bTag;
      if(usbtmc_state.ep_int_in != 0)
      {
        rsp.USBTMC_status = USBTMC_STATUS_SUCCESS;
        rsp.statusByte = 0x00; // Use interrupt endpoint, instead.

        usbtmc_read_stb_interrupt_488_t intMsg =
        {
          .bNotify1 = {
              .one = 1,
              .bTag = bTag & 0x7Fu,
          },
          .StatusByte = tud_usbtmc_get_stb_cb(&(rsp.USBTMC_status))
        };
        usbd_edpt_xfer(rhport, usbtmc_state.ep_int_in, (void*)&intMsg, sizeof(intMsg));
      }
      else
      {
        rsp.statusByte = tud_usbtmc_get_stb_cb(&(rsp.USBTMC_status));
      }
      TU_VERIFY(tud_control_xfer(rhport, request, (void*)&rsp, sizeof(rsp)));
      return true;
    }
    // USB488 optional requests
  case USB488_bREQUEST_REN_CONTROL:
  case USB488_bREQUEST_GO_TO_LOCAL:
  case USB488_bREQUEST_LOCAL_LOCKOUT:
    {
      TU_VERIFY(request->bmRequestType == 0xA1); // in,class,interface
      TU_VERIFY(false);
      return false;
    }
#endif

  default:
    TU_VERIFY(false);
    return false;
  }
  TU_VERIFY(false);
}

#endif /* CFG_TUD_TSMC */
