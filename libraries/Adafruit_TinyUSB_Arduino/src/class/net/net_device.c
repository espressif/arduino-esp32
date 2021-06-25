/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Peter Lawrence
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

#include "tusb_option.h"

#if ( TUSB_OPT_DEVICE_ENABLED && CFG_TUD_NET )

#include "device/usbd.h"
#include "device/usbd_pvt.h"

#include "net_device.h"
#include "rndis_protocol.h"

void rndis_class_set_handler(uint8_t *data, int size); /* found in ./misc/networking/rndis_reports.c */

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
typedef struct
{
  uint8_t itf_num;      // Index number of Management Interface, +1 for Data Interface
  uint8_t itf_data_alt; // Alternate setting of Data Interface. 0 : inactive, 1 : active

  uint8_t ep_notif;
  uint8_t ep_in;
  uint8_t ep_out;

  bool ecm_mode;

  // Endpoint descriptor use to open/close when receving SetInterface
  // TODO since configuration descriptor may not be long-lived memory, we should
  // keep a copy of endpoint attribute instead
  uint8_t const * ecm_desc_epdata;

} netd_interface_t;

#define CFG_TUD_NET_PACKET_PREFIX_LEN sizeof(rndis_data_packet_t)
#define CFG_TUD_NET_PACKET_SUFFIX_LEN 0

CFG_TUSB_MEM_SECTION CFG_TUSB_MEM_ALIGN static uint8_t received[CFG_TUD_NET_PACKET_PREFIX_LEN + CFG_TUD_NET_MTU + CFG_TUD_NET_PACKET_PREFIX_LEN];
CFG_TUSB_MEM_SECTION CFG_TUSB_MEM_ALIGN static uint8_t transmitted[CFG_TUD_NET_PACKET_PREFIX_LEN + CFG_TUD_NET_MTU + CFG_TUD_NET_PACKET_PREFIX_LEN];

struct ecm_notify_struct
{
  tusb_control_request_t header;
  uint32_t downlink, uplink;
};

static const struct ecm_notify_struct ecm_notify_nc =
{
  .header = {
    .bmRequestType = 0xA1,
    .bRequest = 0 /* NETWORK_CONNECTION aka NetworkConnection */,
    .wValue = 1 /* Connected */,
    .wLength = 0,
  },
};

static const struct ecm_notify_struct ecm_notify_csc =
{
  .header = {
    .bmRequestType = 0xA1,
    .bRequest = 0x2A /* CONNECTION_SPEED_CHANGE aka ConnectionSpeedChange */,
    .wLength = 8,
  },
  .downlink = 9728000,
  .uplink = 9728000,
};

// TODO remove CFG_TUSB_MEM_SECTION, control internal buffer is already in this special section
CFG_TUSB_MEM_SECTION CFG_TUSB_MEM_ALIGN static union
{
  uint8_t rndis_buf[120];
  struct ecm_notify_struct ecm_buf;
} notify;

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
// TODO remove CFG_TUSB_MEM_SECTION
CFG_TUSB_MEM_SECTION static netd_interface_t _netd_itf;

static bool can_xmit;

void tud_network_recv_renew(void)
{
  usbd_edpt_xfer(TUD_OPT_RHPORT, _netd_itf.ep_out, received, sizeof(received));
}

static void do_in_xfer(uint8_t *buf, uint16_t len)
{
  can_xmit = false;
  usbd_edpt_xfer(TUD_OPT_RHPORT, _netd_itf.ep_in, buf, len);
}

void netd_report(uint8_t *buf, uint16_t len)
{
  usbd_edpt_xfer(TUD_OPT_RHPORT, _netd_itf.ep_notif, buf, len);
}

//--------------------------------------------------------------------+
// USBD Driver API
//--------------------------------------------------------------------+
void netd_init(void)
{
  tu_memclr(&_netd_itf, sizeof(_netd_itf));
}

void netd_reset(uint8_t rhport)
{
  (void) rhport;

  netd_init();
}

uint16_t netd_open(uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t max_len)
{
  bool const is_rndis = (TUD_RNDIS_ITF_CLASS    == itf_desc->bInterfaceClass    &&
                         TUD_RNDIS_ITF_SUBCLASS == itf_desc->bInterfaceSubClass &&
                         TUD_RNDIS_ITF_PROTOCOL == itf_desc->bInterfaceProtocol);

  bool const is_ecm = (TUSB_CLASS_CDC                           == itf_desc->bInterfaceClass &&
                       CDC_COMM_SUBCLASS_ETHERNET_CONTROL_MODEL == itf_desc->bInterfaceSubClass &&
                       0x00                                     == itf_desc->bInterfaceProtocol);

  TU_VERIFY(is_rndis || is_ecm, 0);

  // confirm interface hasn't already been allocated
  TU_ASSERT(0 == _netd_itf.ep_notif, 0);

  // sanity check the descriptor
  _netd_itf.ecm_mode = is_ecm;

  //------------- Management Interface -------------//
  _netd_itf.itf_num = itf_desc->bInterfaceNumber;

  uint16_t drv_len = sizeof(tusb_desc_interface_t);
  uint8_t const * p_desc = tu_desc_next( itf_desc );

  // Communication Functional Descriptors
  while ( TUSB_DESC_CS_INTERFACE == tu_desc_type(p_desc) && drv_len <= max_len )
  {
    drv_len += tu_desc_len(p_desc);
    p_desc   = tu_desc_next(p_desc);
  }

  // notification endpoint (if any)
  if ( TUSB_DESC_ENDPOINT == tu_desc_type(p_desc) )
  {
    TU_ASSERT( usbd_edpt_open(rhport, (tusb_desc_endpoint_t const *) p_desc), 0 );

    _netd_itf.ep_notif = ((tusb_desc_endpoint_t const *) p_desc)->bEndpointAddress;

    drv_len += tu_desc_len(p_desc);
    p_desc   = tu_desc_next(p_desc);
  }

  //------------- Data Interface -------------//
  // - RNDIS Data followed immediately by a pair of endpoints
  // - CDC-ECM data interface has 2 alternate settings
  //   - 0 : zero endpoints for inactive (default)
  //   - 1 : IN & OUT endpoints for active networking
  TU_ASSERT(TUSB_DESC_INTERFACE == tu_desc_type(p_desc), 0);

  do
  {
    tusb_desc_interface_t const * data_itf_desc = (tusb_desc_interface_t const *) p_desc;
    TU_ASSERT(TUSB_CLASS_CDC_DATA == data_itf_desc->bInterfaceClass, 0);

    drv_len += tu_desc_len(p_desc);
    p_desc   = tu_desc_next(p_desc);
  }while( _netd_itf.ecm_mode && (TUSB_DESC_INTERFACE == tu_desc_type(p_desc)) && (drv_len <= max_len) );

  // Pair of endpoints
  TU_ASSERT(TUSB_DESC_ENDPOINT == tu_desc_type(p_desc), 0);

  if ( _netd_itf.ecm_mode )
  {
    // ECM by default is in-active, save the endpoint attribute
    // to open later when received setInterface
    _netd_itf.ecm_desc_epdata = p_desc;
  }else
  {
    // Open endpoint pair for RNDIS
    TU_ASSERT( usbd_open_edpt_pair(rhport, p_desc, 2, TUSB_XFER_BULK, &_netd_itf.ep_out, &_netd_itf.ep_in), 0 );

    tud_network_init_cb();

    // we are ready to transmit a packet
    can_xmit = true;

    // prepare for incoming packets
    tud_network_recv_renew();
  }

  drv_len += 2*sizeof(tusb_desc_endpoint_t);

  return drv_len;
}

static void ecm_report(bool nc)
{
  notify.ecm_buf = (nc) ? ecm_notify_nc : ecm_notify_csc;
  notify.ecm_buf.header.wIndex = _netd_itf.itf_num;
  netd_report((uint8_t *)&notify.ecm_buf, (nc) ? sizeof(notify.ecm_buf.header) : sizeof(notify.ecm_buf));
}

// Invoked when a control transfer occurred on an interface of this class
// Driver response accordingly to the request and the transfer stage (setup/data/ack)
// return false to stall control endpoint (e.g unsupported request)
bool netd_control_xfer_cb (uint8_t rhport, uint8_t stage, tusb_control_request_t const * request)
{
  if ( stage == CONTROL_STAGE_SETUP )
  {
    switch ( request->bmRequestType_bit.type )
    {
      case TUSB_REQ_TYPE_STANDARD:
        switch ( request->bRequest )
        {
          case TUSB_REQ_GET_INTERFACE:
          {
            uint8_t const req_itfnum = (uint8_t) request->wIndex;
            TU_VERIFY(_netd_itf.itf_num+1 == req_itfnum);

            tud_control_xfer(rhport, request, &_netd_itf.itf_data_alt, 1);
          }
          break;

          case TUSB_REQ_SET_INTERFACE:
          {
            uint8_t const req_itfnum = (uint8_t) request->wIndex;
            uint8_t const req_alt    = (uint8_t) request->wValue;

            // Only valid for Data Interface with Alternate is either 0 or 1
            TU_VERIFY(_netd_itf.itf_num+1 == req_itfnum && req_alt < 2);

            // ACM-ECM only: qequest to enable/disable network activities
            TU_VERIFY(_netd_itf.ecm_mode);

            _netd_itf.itf_data_alt = req_alt;

            if ( _netd_itf.itf_data_alt )
            {
              // TODO since we don't actually close endpoint
              // hack here to not re-open it
              if ( _netd_itf.ep_in == 0 && _netd_itf.ep_out == 0 )
              {
                TU_ASSERT(_netd_itf.ecm_desc_epdata);
                TU_ASSERT( usbd_open_edpt_pair(rhport, _netd_itf.ecm_desc_epdata, 2, TUSB_XFER_BULK, &_netd_itf.ep_out, &_netd_itf.ep_in) );

                // TODO should be merge with RNDIS's after endpoint opened
                // Also should have opposite callback for application to disable network !!
                tud_network_init_cb();
                can_xmit = true; // we are ready to transmit a packet
                tud_network_recv_renew(); // prepare for incoming packets
              }
            }else
            {
              // TODO close the endpoint pair
              // For now pretend that we did, this should have no harm since host won't try to
              // communicate with the endpoints again
              // _netd_itf.ep_in = _netd_itf.ep_out = 0
            }

            tud_control_status(rhport, request);
          }
          break;

          // unsupported request
          default: return false;
        }
      break;

      case TUSB_REQ_TYPE_CLASS:
        TU_VERIFY (_netd_itf.itf_num == request->wIndex);

        if (_netd_itf.ecm_mode)
        {
          /* the only required CDC-ECM Management Element Request is SetEthernetPacketFilter */
          if (0x43 /* SET_ETHERNET_PACKET_FILTER */ == request->bRequest)
          {
            tud_control_xfer(rhport, request, NULL, 0);
            ecm_report(true);
          }
        }
        else
        {
          if (request->bmRequestType_bit.direction == TUSB_DIR_IN)
          {
            rndis_generic_msg_t *rndis_msg = (rndis_generic_msg_t *) ((void*) notify.rndis_buf);
            uint32_t msglen = tu_le32toh(rndis_msg->MessageLength);
            TU_ASSERT(msglen <= sizeof(notify.rndis_buf));
            tud_control_xfer(rhport, request, notify.rndis_buf, msglen);
          }
          else
          {
            tud_control_xfer(rhport, request, notify.rndis_buf, sizeof(notify.rndis_buf));
          }
        }
      break;

      // unsupported request
      default: return false;
    }
  }
  else if ( stage == CONTROL_STAGE_DATA )
  {
    // Handle RNDIS class control OUT only
    if (request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS &&
        request->bmRequestType_bit.direction == TUSB_DIR_OUT   &&
        _netd_itf.itf_num == request->wIndex)
    {
      if ( !_netd_itf.ecm_mode )
      {
        rndis_class_set_handler(notify.rndis_buf, request->wLength);
      }
    }
  }

  return true;
}

static void handle_incoming_packet(uint32_t len)
{
  uint8_t *pnt = received;
  uint32_t size = 0;

  if (_netd_itf.ecm_mode)
  {
    size = len;
  }
  else
  {
    rndis_data_packet_t *r = (rndis_data_packet_t *) ((void*) pnt);
    if (len >= sizeof(rndis_data_packet_t))
      if ( (r->MessageType == REMOTE_NDIS_PACKET_MSG) && (r->MessageLength <= len))
        if ( (r->DataOffset + offsetof(rndis_data_packet_t, DataOffset) + r->DataLength) <= len)
        {
          pnt = &received[r->DataOffset + offsetof(rndis_data_packet_t, DataOffset)];
          size = r->DataLength;
        }
  }

  if (!tud_network_recv_cb(pnt, size))
  {
    /* if a buffer was never handled by user code, we must renew on the user's behalf */
    tud_network_recv_renew();
  }
}

bool netd_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes)
{
  (void) rhport;
  (void) result;

  /* new packet received */
  if ( ep_addr == _netd_itf.ep_out )
  {
    handle_incoming_packet(xferred_bytes);
  }

  /* data transmission finished */
  if ( ep_addr == _netd_itf.ep_in )
  {
    /* TinyUSB requires the class driver to implement ZLP (since ZLP usage is class-specific) */

    if ( xferred_bytes && (0 == (xferred_bytes % CFG_TUD_NET_ENDPOINT_SIZE)) )
    {
      do_in_xfer(NULL, 0); /* a ZLP is needed */
    }
    else
    {
      /* we're finally finished */
      can_xmit = true;
    }
  }

  if ( _netd_itf.ecm_mode && (ep_addr == _netd_itf.ep_notif) )
  {
    if (sizeof(notify.ecm_buf.header) == xferred_bytes) ecm_report(false);
  }

  return true;
}

bool tud_network_can_xmit(void)
{
  return can_xmit;
}

void tud_network_xmit(void *ref, uint16_t arg)
{
  uint8_t *data;
  uint16_t len;

  if (!can_xmit)
    return;

  len = (_netd_itf.ecm_mode) ? 0 : CFG_TUD_NET_PACKET_PREFIX_LEN;
  data = transmitted + len;

  len += tud_network_xmit_cb(data, ref, arg);

  if (!_netd_itf.ecm_mode)
  {
    rndis_data_packet_t *hdr = (rndis_data_packet_t *) ((void*) transmitted);
    memset(hdr, 0, sizeof(rndis_data_packet_t));
    hdr->MessageType = REMOTE_NDIS_PACKET_MSG;
    hdr->MessageLength = len;
    hdr->DataOffset = sizeof(rndis_data_packet_t) - offsetof(rndis_data_packet_t, DataOffset);
    hdr->DataLength = len - sizeof(rndis_data_packet_t);
  }

  do_in_xfer(transmitted, len);
}

#endif
