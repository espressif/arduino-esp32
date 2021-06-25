/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Sylvain Munaut <tnt@246tNt.com>
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

#include "tusb_option.h"

#if (TUSB_OPT_DEVICE_ENABLED && CFG_TUD_DFU_RUNTIME)

#include "device/usbd.h"
#include "device/usbd_pvt.h"

#include "dfu_rt_device.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// USBD Driver API
//--------------------------------------------------------------------+
void dfu_rtd_init(void)
{
}

void dfu_rtd_reset(uint8_t rhport)
{
    (void) rhport;
}

uint16_t dfu_rtd_open(uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t max_len)
{
  (void) rhport;
  (void) max_len;

  // Ensure this is DFU Runtime
  TU_VERIFY((itf_desc->bInterfaceSubClass == TUD_DFU_APP_SUBCLASS) &&
            (itf_desc->bInterfaceProtocol == DFU_PROTOCOL_RT), 0);

  uint8_t const * p_desc = tu_desc_next( itf_desc );
  uint16_t drv_len = sizeof(tusb_desc_interface_t);

  if ( TUSB_DESC_FUNCTIONAL == tu_desc_type(p_desc) )
  {
    drv_len += tu_desc_len(p_desc);
    p_desc   = tu_desc_next(p_desc);
  }

  return drv_len;
}

// Invoked when a control transfer occurred on an interface of this class
// Driver response accordingly to the request and the transfer stage (setup/data/ack)
// return false to stall control endpoint (e.g unsupported request)
bool dfu_rtd_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request)
{
  // nothing to do with DATA or ACK stage
  if ( stage != CONTROL_STAGE_SETUP ) return true;

  TU_VERIFY(request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_INTERFACE);

  // dfu-util will try to claim the interface with SET_INTERFACE request before sending DFU request
  if ( TUSB_REQ_TYPE_STANDARD == request->bmRequestType_bit.type &&
       TUSB_REQ_SET_INTERFACE == request->bRequest )
  {
    tud_control_status(rhport, request);
    return true;
  }

  // Handle class request only from here
  TU_VERIFY(request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS);

  switch (request->bRequest)
  {
    case DFU_REQUEST_DETACH:
    {
      TU_LOG2("  DFU RT Request: DETACH\r\n");
      tud_control_status(rhport, request);
      tud_dfu_runtime_reboot_to_dfu_cb();
    }
    break;

    case DFU_REQUEST_GETSTATUS:
    {
      TU_LOG2("  DFU RT Request: GETSTATUS\r\n");
      dfu_status_req_payload_t resp;
      // Status = OK, Poll timeout is ignored during RT, State = APP_IDLE, IString = 0
      memset(&resp, 0x00, sizeof(dfu_status_req_payload_t));
      tud_control_xfer(rhport, request, &resp, sizeof(dfu_status_req_payload_t));
    }
    break;

    default:
    {
      TU_LOG2("  DFU RT Unexpected Request: %d\r\n", request->bRequest);
      return false; // stall unsupported request
    }
  }

  return true;
}

#endif
