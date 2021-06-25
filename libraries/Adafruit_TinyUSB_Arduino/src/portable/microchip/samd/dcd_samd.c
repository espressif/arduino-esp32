/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Scott Shawcroft for Adafruit Industries
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

#if TUSB_OPT_DEVICE_ENABLED && \
    (CFG_TUSB_MCU == OPT_MCU_SAMD11 || CFG_TUSB_MCU == OPT_MCU_SAMD21 || \
     CFG_TUSB_MCU == OPT_MCU_SAMD51 || CFG_TUSB_MCU == OPT_MCU_SAME5X || \
     CFG_TUSB_MCU == OPT_MCU_SAML22 || CFG_TUSB_MCU == OPT_MCU_SAML21)

#include "sam.h"
#include "device/dcd.h"

/*------------------------------------------------------------------*/
/* MACRO TYPEDEF CONSTANT ENUM
 *------------------------------------------------------------------*/
static TU_ATTR_ALIGNED(4) UsbDeviceDescBank sram_registers[8][2];

// Setup packet is only 8 bytes in length. However under certain scenario,
// USB DMA controller may decide to overwrite/overflow the buffer  with
// 2 extra bytes of CRC. From datasheet's "Management of SETUP Transactions" section
//    If the number of received data bytes is the maximum data payload specified by
//    PCKSIZE.SIZE minus one, only the first CRC data is written to the data buffer.
//    If the number of received data is equal or less than the data payload specified
//    by PCKSIZE.SIZE minus two, both CRC data bytes are written to the data buffer.
// Therefore we will need to increase it to 10 bytes here.
static TU_ATTR_ALIGNED(4) uint8_t _setup_packet[8+2];

// ready for receiving SETUP packet
static inline void prepare_setup(void)
{
  // Only make sure the EP0 OUT buffer is ready
  sram_registers[0][0].ADDR.reg = (uint32_t) _setup_packet;
  sram_registers[0][0].PCKSIZE.bit.MULTI_PACKET_SIZE = sizeof(tusb_control_request_t);
  sram_registers[0][0].PCKSIZE.bit.BYTE_COUNT = 0;
}

// Setup the control endpoint 0.
static void bus_reset(void)
{
  // Max size of packets is 64 bytes.
  UsbDeviceDescBank* bank_out = &sram_registers[0][TUSB_DIR_OUT];
  bank_out->PCKSIZE.bit.SIZE = 0x3;
  UsbDeviceDescBank* bank_in = &sram_registers[0][TUSB_DIR_IN];
  bank_in->PCKSIZE.bit.SIZE = 0x3;

  UsbDeviceEndpoint* ep = &USB->DEVICE.DeviceEndpoint[0];
  ep->EPCFG.reg = USB_DEVICE_EPCFG_EPTYPE0(0x1) | USB_DEVICE_EPCFG_EPTYPE1(0x1);
  ep->EPINTENSET.reg = USB_DEVICE_EPINTENSET_TRCPT0 | USB_DEVICE_EPINTENSET_TRCPT1 | USB_DEVICE_EPINTENSET_RXSTP;

  // Prepare for setup packet
  prepare_setup();
}

/*------------------------------------------------------------------*/
/* Controller API
 *------------------------------------------------------------------*/
void dcd_init (uint8_t rhport)
{
  (void) rhport;

  // Reset to get in a clean state.
  USB->DEVICE.CTRLA.bit.SWRST = true;
  while (USB->DEVICE.SYNCBUSY.bit.SWRST == 0) {}
  while (USB->DEVICE.SYNCBUSY.bit.SWRST == 1) {}

  USB->DEVICE.PADCAL.bit.TRANSP = (*((uint32_t*) USB_FUSES_TRANSP_ADDR) & USB_FUSES_TRANSP_Msk) >> USB_FUSES_TRANSP_Pos;
  USB->DEVICE.PADCAL.bit.TRANSN = (*((uint32_t*) USB_FUSES_TRANSN_ADDR) & USB_FUSES_TRANSN_Msk) >> USB_FUSES_TRANSN_Pos;
  USB->DEVICE.PADCAL.bit.TRIM   = (*((uint32_t*) USB_FUSES_TRIM_ADDR) & USB_FUSES_TRIM_Msk) >> USB_FUSES_TRIM_Pos;

  USB->DEVICE.QOSCTRL.bit.CQOS = 3; // High Quality
  USB->DEVICE.QOSCTRL.bit.DQOS = 3; // High Quality

  // Configure registers
  USB->DEVICE.DESCADD.reg = (uint32_t) &sram_registers;
  USB->DEVICE.CTRLB.reg = USB_DEVICE_CTRLB_SPDCONF_FS;
  USB->DEVICE.CTRLA.reg = USB_CTRLA_MODE_DEVICE | USB_CTRLA_ENABLE | USB_CTRLA_RUNSTDBY;
  while (USB->DEVICE.SYNCBUSY.bit.ENABLE == 1) {}

  USB->DEVICE.INTFLAG.reg |= USB->DEVICE.INTFLAG.reg; // clear pending
  USB->DEVICE.INTENSET.reg = /* USB_DEVICE_INTENSET_SOF | */ USB_DEVICE_INTENSET_EORST;
}

#if CFG_TUSB_MCU == OPT_MCU_SAMD51 || CFG_TUSB_MCU == OPT_MCU_SAME5X

void dcd_int_enable(uint8_t rhport)
{
  (void) rhport;
  NVIC_EnableIRQ(USB_0_IRQn);
  NVIC_EnableIRQ(USB_1_IRQn);
  NVIC_EnableIRQ(USB_2_IRQn);
  NVIC_EnableIRQ(USB_3_IRQn);
}

void dcd_int_disable(uint8_t rhport)
{
  (void) rhport;
  NVIC_DisableIRQ(USB_3_IRQn);
  NVIC_DisableIRQ(USB_2_IRQn);
  NVIC_DisableIRQ(USB_1_IRQn);
  NVIC_DisableIRQ(USB_0_IRQn);
}

#elif CFG_TUSB_MCU == OPT_MCU_SAMD11 || CFG_TUSB_MCU == OPT_MCU_SAMD21 || \
      CFG_TUSB_MCU == OPT_MCU_SAML22 || CFG_TUSB_MCU == OPT_MCU_SAML21

void dcd_int_enable(uint8_t rhport)
{
  (void) rhport;
  NVIC_EnableIRQ(USB_IRQn);
}

void dcd_int_disable(uint8_t rhport)
{
  (void) rhport;
  NVIC_DisableIRQ(USB_IRQn);
}

#else

#error "No implementation available for dcd_int_enable / dcd_int_disable"

#endif

void dcd_set_address (uint8_t rhport, uint8_t dev_addr)
{
  (void) dev_addr;

  // Response with zlp status
  dcd_edpt_xfer(rhport, 0x80, NULL, 0);

  // DCD can only set address after status for this request is complete
  // do it at dcd_edpt0_status_complete()

  // Enable SUSPEND interrupt since the bus signal D+/D- are stable now.
  USB->DEVICE.INTFLAG.reg = USB_DEVICE_INTENCLR_SUSPEND; // clear pending
  USB->DEVICE.INTENSET.reg = USB_DEVICE_INTENSET_SUSPEND;
}

void dcd_remote_wakeup(uint8_t rhport)
{
  (void) rhport;
  USB->DEVICE.CTRLB.bit.UPRSM = 1;
}

// disconnect by disabling internal pull-up resistor on D+/D-
void dcd_disconnect(uint8_t rhport)
{
  (void) rhport;
  USB->DEVICE.CTRLB.reg |= USB_DEVICE_CTRLB_DETACH;
}

// connect by enabling internal pull-up resistor on D+/D-
void dcd_connect(uint8_t rhport)
{
  (void) rhport;
   USB->DEVICE.CTRLB.reg &= ~USB_DEVICE_CTRLB_DETACH;
}

/*------------------------------------------------------------------*/
/* DCD Endpoint port
 *------------------------------------------------------------------*/

// Invoked when a control transfer's status stage is complete.
// May help DCD to prepare for next control transfer, this API is optional.
void dcd_edpt0_status_complete(uint8_t rhport, tusb_control_request_t const * request)
{
  (void) rhport;

  if (request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_DEVICE &&
      request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD &&
      request->bRequest == TUSB_REQ_SET_ADDRESS )
  {
    uint8_t const dev_addr = (uint8_t) request->wValue;
    USB->DEVICE.DADD.reg = USB_DEVICE_DADD_DADD(dev_addr) | USB_DEVICE_DADD_ADDEN;
  }

  // Just finished status stage, prepare for next setup packet
  // Note: we may already prepare setup when queueing the control status.
  // but it has no harm to do it again here
  prepare_setup();
}

bool dcd_edpt_open (uint8_t rhport, tusb_desc_endpoint_t const * desc_edpt)
{
  (void) rhport;

  uint8_t const epnum = tu_edpt_number(desc_edpt->bEndpointAddress);
  uint8_t const dir   = tu_edpt_dir(desc_edpt->bEndpointAddress);

  UsbDeviceDescBank* bank = &sram_registers[epnum][dir];
  uint32_t size_value = 0;
  while (size_value < 7) {
    if (1 << (size_value + 3) == desc_edpt->wMaxPacketSize.size) {
      break;
    }
    size_value++;
  }

  // unsupported endpoint size
  if ( size_value == 7 && desc_edpt->wMaxPacketSize.size != 1023 ) return false;

  bank->PCKSIZE.bit.SIZE = size_value;

  UsbDeviceEndpoint* ep = &USB->DEVICE.DeviceEndpoint[epnum];

  if ( dir == TUSB_DIR_OUT )
  {
    ep->EPCFG.bit.EPTYPE0 = desc_edpt->bmAttributes.xfer + 1;
    ep->EPINTENSET.bit.TRCPT0 = true;
  }else
  {
    ep->EPCFG.bit.EPTYPE1 = desc_edpt->bmAttributes.xfer + 1;
    ep->EPINTENSET.bit.TRCPT1 = true;
  }

  return true;
}

bool dcd_edpt_xfer (uint8_t rhport, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes)
{
  (void) rhport;

  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  UsbDeviceDescBank* bank = &sram_registers[epnum][dir];
  UsbDeviceEndpoint* ep = &USB->DEVICE.DeviceEndpoint[epnum];

  bank->ADDR.reg = (uint32_t) buffer;

  // A SETUP token can occur immediately after an ZLP Status.
  // So make sure we have a valid buffer for setup packet.
  //   Status = ZLP EP0 with direction opposite to one in the dir bit of current setup
  if ( (epnum == 0) && (buffer == NULL) && (total_bytes == 0) && (dir != tu_edpt_dir(_setup_packet[0])) ) {
    prepare_setup();
  }

  if ( dir == TUSB_DIR_OUT )
  {
    bank->PCKSIZE.bit.MULTI_PACKET_SIZE = total_bytes;
    bank->PCKSIZE.bit.BYTE_COUNT = 0;
    ep->EPSTATUSCLR.reg |= USB_DEVICE_EPSTATUSCLR_BK0RDY;
    ep->EPINTFLAG.reg |= USB_DEVICE_EPINTFLAG_TRFAIL0;
  } else
  {
    bank->PCKSIZE.bit.MULTI_PACKET_SIZE = 0;
    bank->PCKSIZE.bit.BYTE_COUNT = total_bytes;
    ep->EPSTATUSSET.reg |= USB_DEVICE_EPSTATUSSET_BK1RDY;
    ep->EPINTFLAG.reg |= USB_DEVICE_EPINTFLAG_TRFAIL1;
  }

  return true;
}

void dcd_edpt_stall (uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;

  uint8_t const epnum = tu_edpt_number(ep_addr);
  UsbDeviceEndpoint* ep = &USB->DEVICE.DeviceEndpoint[epnum];

  if (tu_edpt_dir(ep_addr) == TUSB_DIR_IN) {
    ep->EPSTATUSSET.reg = USB_DEVICE_EPSTATUSSET_STALLRQ1;
  } else {
    ep->EPSTATUSSET.reg = USB_DEVICE_EPSTATUSSET_STALLRQ0;
  }
}

void dcd_edpt_clear_stall (uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;

  uint8_t const epnum = tu_edpt_number(ep_addr);
  UsbDeviceEndpoint* ep = &USB->DEVICE.DeviceEndpoint[epnum];

  if (tu_edpt_dir(ep_addr) == TUSB_DIR_IN) {
    ep->EPSTATUSCLR.reg = USB_DEVICE_EPSTATUSCLR_STALLRQ1 | USB_DEVICE_EPSTATUSCLR_DTGLIN;
  } else {
    ep->EPSTATUSCLR.reg = USB_DEVICE_EPSTATUSCLR_STALLRQ0 | USB_DEVICE_EPSTATUSCLR_DTGLOUT;
  }
}

//--------------------------------------------------------------------+
// Interrupt Handler
//--------------------------------------------------------------------+
void maybe_transfer_complete(void) {
  uint32_t epints = USB->DEVICE.EPINTSMRY.reg;

  for (uint8_t epnum = 0; epnum < USB_EPT_NUM; epnum++) {
    if ((epints & (1 << epnum)) == 0) {
      continue;
    }

    UsbDeviceEndpoint* ep = &USB->DEVICE.DeviceEndpoint[epnum];
    uint32_t epintflag = ep->EPINTFLAG.reg;

    // Handle IN completions
    if ((epintflag & USB_DEVICE_EPINTFLAG_TRCPT1) != 0) {
      UsbDeviceDescBank* bank = &sram_registers[epnum][TUSB_DIR_IN];
      uint16_t const total_transfer_size = bank->PCKSIZE.bit.BYTE_COUNT;

      dcd_event_xfer_complete(0, epnum | TUSB_DIR_IN_MASK, total_transfer_size, XFER_RESULT_SUCCESS, true);

      ep->EPINTFLAG.reg = USB_DEVICE_EPINTFLAG_TRCPT1;
    }

    // Handle OUT completions
    if ((epintflag & USB_DEVICE_EPINTFLAG_TRCPT0) != 0) {
      UsbDeviceDescBank* bank = &sram_registers[epnum][TUSB_DIR_OUT];
      uint16_t const total_transfer_size = bank->PCKSIZE.bit.BYTE_COUNT;

      dcd_event_xfer_complete(0, epnum, total_transfer_size, XFER_RESULT_SUCCESS, true);

      ep->EPINTFLAG.reg = USB_DEVICE_EPINTFLAG_TRCPT0;
    }
  }
}


void dcd_int_handler (uint8_t rhport)
{
  (void) rhport;

  uint32_t int_status = USB->DEVICE.INTFLAG.reg & USB->DEVICE.INTENSET.reg;

  // Start of Frame
  if ( int_status & USB_DEVICE_INTFLAG_SOF )
  {
    USB->DEVICE.INTFLAG.reg = USB_DEVICE_INTFLAG_SOF;
    dcd_event_bus_signal(0, DCD_EVENT_SOF, true);
  }

  // SAMD doesn't distinguish between Suspend and Disconnect state.
  // Both condition will cause SUSPEND interrupt triggered.
  // To prevent being triggered when D+/D- are not stable, SUSPEND interrupt is only
  // enabled when we received SET_ADDRESS request and cleared on Bus Reset
  if ( int_status & USB_DEVICE_INTFLAG_SUSPEND )
  {
    USB->DEVICE.INTFLAG.reg = USB_DEVICE_INTFLAG_SUSPEND;

    // Enable wakeup interrupt
    USB->DEVICE.INTFLAG.reg = USB_DEVICE_INTFLAG_WAKEUP; // clear pending
    USB->DEVICE.INTENSET.reg = USB_DEVICE_INTFLAG_WAKEUP;

    dcd_event_bus_signal(0, DCD_EVENT_SUSPEND, true);
  }

  // Wakeup interrupt is only enabled when we got suspended.
  // Wakeup interrupt will disable itself
  if ( int_status & USB_DEVICE_INTFLAG_WAKEUP )
  {
    USB->DEVICE.INTFLAG.reg = USB_DEVICE_INTFLAG_WAKEUP;

    // disable wakeup interrupt itself
    USB->DEVICE.INTENCLR.reg = USB_DEVICE_INTFLAG_WAKEUP;
    dcd_event_bus_signal(0, DCD_EVENT_RESUME, true);
  }

  // Enable of Reset
  if ( int_status & USB_DEVICE_INTFLAG_EORST )
  {
    USB->DEVICE.INTFLAG.reg = USB_DEVICE_INTFLAG_EORST;

    // Disable both suspend and wakeup interrupt
    USB->DEVICE.INTENCLR.reg = USB_DEVICE_INTFLAG_WAKEUP | USB_DEVICE_INTFLAG_SUSPEND;

    bus_reset();
    dcd_event_bus_reset(0, TUSB_SPEED_FULL, true);
  }

  // Handle SETUP packet
  if (USB->DEVICE.DeviceEndpoint[0].EPINTFLAG.bit.RXSTP)
  {
    // This copies the data elsewhere so we can reuse the buffer.
    dcd_event_setup_received(0, _setup_packet, true);

    // Although Setup packet only set RXSTP bit,
    // TRCPT0 bit could already be set by previous ZLP OUT Status (not handled until now).
    // Since control status complete event is optional, we can just clear TRCPT0 and skip the status event
    USB->DEVICE.DeviceEndpoint[0].EPINTFLAG.reg = USB_DEVICE_EPINTFLAG_RXSTP | USB_DEVICE_EPINTFLAG_TRCPT0;
  }

  // Handle complete transfer
  maybe_transfer_complete();
}

#endif
