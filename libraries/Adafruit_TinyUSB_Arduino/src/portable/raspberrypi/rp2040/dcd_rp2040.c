/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
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

#if TUSB_OPT_DEVICE_ENABLED && CFG_TUSB_MCU == OPT_MCU_RP2040

#include "pico.h"
#include "rp2040_usb.h"

#if TUD_OPT_RP2040_USB_DEVICE_ENUMERATION_FIX
#include "pico/fix/rp2040_usb_device_enumeration.h"
#endif

#include "device/dcd.h"

/*------------------------------------------------------------------*/
/* Low level controller
 *------------------------------------------------------------------*/

#define usb_hw_set hw_set_alias(usb_hw)
#define usb_hw_clear hw_clear_alias(usb_hw)

// Init these in dcd_init
static uint8_t *next_buffer_ptr;

// USB_MAX_ENDPOINTS Endpoints, direction TUSB_DIR_OUT for out and TUSB_DIR_IN for in.
static struct hw_endpoint hw_endpoints[USB_MAX_ENDPOINTS][2] = {0};

static inline struct hw_endpoint *hw_endpoint_get_by_num(uint8_t num, tusb_dir_t dir)
{
    return &hw_endpoints[num][dir];
}

static struct hw_endpoint *hw_endpoint_get_by_addr(uint8_t ep_addr)
{
    uint8_t num = tu_edpt_number(ep_addr);
    tusb_dir_t dir = tu_edpt_dir(ep_addr);
    return hw_endpoint_get_by_num(num, dir);
}

static void _hw_endpoint_alloc(struct hw_endpoint *ep)
{
    uint16_t size = tu_min16(64, ep->wMaxPacketSize);

    // Assumes single buffered for now
    ep->hw_data_buf = next_buffer_ptr;
    next_buffer_ptr += size;
    // Bits 0-5 are ignored by the controller so make sure these are 0
    if ((uintptr_t)next_buffer_ptr & 0b111111u)
    {
        // Round up to the next 64
        uint32_t fixptr = (uintptr_t)next_buffer_ptr;
        fixptr &= ~0b111111u;
        fixptr += 64;
        pico_info("Rounding non 64 byte boundary buffer up from %x to %x\n", (uintptr_t)next_buffer_ptr, fixptr);
        next_buffer_ptr = (uint8_t*)fixptr;
    }
    assert(((uintptr_t)next_buffer_ptr & 0b111111u) == 0);
    uint dpram_offset = hw_data_offset(ep->hw_data_buf);
    assert(hw_data_offset(next_buffer_ptr) <= USB_DPRAM_MAX);

    pico_info("Alloced %d bytes at offset 0x%x (0x%p) for ep %d %s\n",
                size,
                dpram_offset,
                ep->hw_data_buf,
                ep->num,
                ep_dir_string[ep->in]);

    // Fill in endpoint control register with buffer offset
    uint32_t reg =  EP_CTRL_ENABLE_BITS
                  | EP_CTRL_INTERRUPT_PER_BUFFER
                  | (ep->transfer_type << EP_CTRL_BUFFER_TYPE_LSB)
                  | dpram_offset;

    *ep->endpoint_control = reg;
}

static void _hw_endpoint_init(struct hw_endpoint *ep, uint8_t ep_addr, uint16_t wMaxPacketSize, uint8_t transfer_type)
{
    const uint8_t num = tu_edpt_number(ep_addr);
    const tusb_dir_t dir = tu_edpt_dir(ep_addr);
    ep->ep_addr = ep_addr;
    // For device, IN is a tx transfer and OUT is an rx transfer
    ep->rx = (dir == TUSB_DIR_OUT);
    // Response to a setup packet on EP0 starts with pid of 1
    ep->next_pid = num == 0 ? 1u : 0u;

    // Add some checks around the max packet size
    if (transfer_type == TUSB_XFER_ISOCHRONOUS)
    {
        if (wMaxPacketSize > USB_MAX_ISO_PACKET_SIZE)
        {
            panic("Isochronous wMaxPacketSize %d too large", wMaxPacketSize);
        }
    }
    else
    {
        if (wMaxPacketSize > USB_MAX_PACKET_SIZE)
        {
            panic("Isochronous wMaxPacketSize %d too large", wMaxPacketSize);
        }
    }

    ep->wMaxPacketSize = wMaxPacketSize;
    ep->transfer_type = transfer_type;

    // Every endpoint has a buffer control register in dpram
    if (dir == TUSB_DIR_IN)
    {
        ep->buffer_control = &usb_dpram->ep_buf_ctrl[num].in;
    }
    else
    {
        ep->buffer_control = &usb_dpram->ep_buf_ctrl[num].out;
    }

    // Clear existing buffer control state
    *ep->buffer_control = 0;

    if (num == 0)
    {
        // EP0 has no endpoint control register because
        // the buffer offsets are fixed
        ep->endpoint_control = NULL;

        // Buffer offset is fixed
        ep->hw_data_buf = (uint8_t*)&usb_dpram->ep0_buf_a[0];
    }
    else
    {
        // Set the endpoint control register (starts at EP1, hence num-1)
        if (dir == TUSB_DIR_IN)
        {
            ep->endpoint_control = &usb_dpram->ep_ctrl[num-1].in;
        }
        else
        {
            ep->endpoint_control = &usb_dpram->ep_ctrl[num-1].out;
        }

        // Now if it hasn't already been done
        //alloc a buffer and fill in endpoint control register
        if(!(ep->configured))
        {
            _hw_endpoint_alloc(ep);
        }
    }

    ep->configured = true;
}

#if 0 // todo unused
static void _hw_endpoint_close(struct hw_endpoint *ep)
{
    // Clear hardware registers and then zero the struct
    // Clears endpoint enable
    *ep->endpoint_control = 0;
    // Clears buffer available, etc
    *ep->buffer_control = 0;
    // Clear any endpoint state
    memset(ep, 0, sizeof(struct hw_endpoint));
}

static void hw_endpoint_close(uint8_t ep_addr)
{
    struct hw_endpoint *ep = hw_endpoint_get_by_addr(ep_addr);
    _hw_endpoint_close(ep);
}
#endif

static void hw_endpoint_init(uint8_t ep_addr, uint16_t wMaxPacketSize, uint8_t bmAttributes)
{
    struct hw_endpoint *ep = hw_endpoint_get_by_addr(ep_addr);
    _hw_endpoint_init(ep, ep_addr, wMaxPacketSize, bmAttributes);
}

static void hw_endpoint_xfer(uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes, bool start)
{
    struct hw_endpoint *ep = hw_endpoint_get_by_addr(ep_addr);
    _hw_endpoint_xfer(ep, buffer, total_bytes, start);
}

static void hw_handle_buff_status(void)
{
    uint32_t remaining_buffers = usb_hw->buf_status;
    pico_trace("buf_status 0x%08x\n", remaining_buffers);
    uint bit = 1u;
    for (uint i = 0; remaining_buffers && i < USB_MAX_ENDPOINTS * 2; i++)
    {
        if (remaining_buffers & bit)
        {
            uint __unused which = (usb_hw->buf_cpu_should_handle & bit) ? 1 : 0;
            // Should be single buffered
            assert(which == 0);
            // clear this in advance
            usb_hw_clear->buf_status = bit;
            // IN transfer for even i, OUT transfer for odd i
            struct hw_endpoint *ep = hw_endpoint_get_by_num(i >> 1u, !(i & 1u));
            // Continue xfer
            bool done = _hw_endpoint_xfer_continue(ep);
            if (done)
            {
                // Notify
                dcd_event_xfer_complete(0, ep->ep_addr, ep->len, XFER_RESULT_SUCCESS, true);
                hw_endpoint_reset_transfer(ep);
            }
            remaining_buffers &= ~bit;
        }
        bit <<= 1u;
    }
}

static void reset_ep0(void)
{
    // If we have finished this transfer on EP0 set pid back to 1 for next
    // setup transfer. Also clear a stall in case
    uint8_t addrs[] = {0x0, 0x80};
    for (uint i = 0 ; i < TU_ARRAY_SIZE(addrs); i++)
    {
        struct hw_endpoint *ep = hw_endpoint_get_by_addr(addrs[i]);
        ep->next_pid = 1u;
        ep->stalled  = 0;
    }
}

static void ep0_0len_status(void)
{
    // Send 0len complete response on EP0 IN
    reset_ep0();
    hw_endpoint_xfer(0x80, NULL, 0, true);
}

static void _hw_endpoint_stall(struct hw_endpoint *ep)
{
    assert(!ep->stalled);
    if (tu_edpt_number(ep->ep_addr) == 0)
    {
        // A stall on EP0 has to be armed so it can be cleared on the next setup packet
        usb_hw_set->ep_stall_arm = (tu_edpt_dir(ep->ep_addr) == TUSB_DIR_IN) ? USB_EP_STALL_ARM_EP0_IN_BITS : USB_EP_STALL_ARM_EP0_OUT_BITS;
    }
    _hw_endpoint_buffer_control_set_mask32(ep, USB_BUF_CTRL_STALL);
    ep->stalled = true;
}

static void hw_endpoint_stall(uint8_t ep_addr)
{
    struct hw_endpoint *ep = hw_endpoint_get_by_addr(ep_addr);
    _hw_endpoint_stall(ep);
}

static void _hw_endpoint_clear_stall(struct hw_endpoint *ep)
{
    if (tu_edpt_number(ep->ep_addr) == 0)
    {
        // Probably already been cleared but no harm
        usb_hw_clear->ep_stall_arm = (tu_edpt_dir(ep->ep_addr) == TUSB_DIR_IN) ? USB_EP_STALL_ARM_EP0_IN_BITS : USB_EP_STALL_ARM_EP0_OUT_BITS;
    }
    _hw_endpoint_buffer_control_clear_mask32(ep, USB_BUF_CTRL_STALL);
    ep->stalled = false;
}

static void hw_endpoint_clear_stall(uint8_t ep_addr)
{
    struct hw_endpoint *ep = hw_endpoint_get_by_addr(ep_addr);
    _hw_endpoint_clear_stall(ep);
}

static void dcd_rp2040_irq(void)
{
    uint32_t status = usb_hw->ints;
    uint32_t handled = 0;

    if (status & USB_INTS_SETUP_REQ_BITS)
    {
        handled |= USB_INTS_SETUP_REQ_BITS;
        uint8_t const *setup = (uint8_t const *)&usb_dpram->setup_packet;
        // Clear stall bits and reset pid
        reset_ep0();
        // Pass setup packet to tiny usb
        dcd_event_setup_received(0, setup, true);
        usb_hw_clear->sie_status = USB_SIE_STATUS_SETUP_REC_BITS;
    }

    if (status & USB_INTS_BUFF_STATUS_BITS)
    {
        handled |= USB_INTS_BUFF_STATUS_BITS;
        hw_handle_buff_status();
    }

    // SE0 for 2 us or more, usually together with Bus Reset
    if (status & USB_INTS_DEV_CONN_DIS_BITS)
    {
        handled |= USB_INTS_DEV_CONN_DIS_BITS;

        if ( usb_hw->sie_status & USB_SIE_STATUS_CONNECTED_BITS )
        {
          // Connected: nothing to do
        }else
        {
          // Disconnected
          dcd_event_bus_signal(0, DCD_EVENT_UNPLUGGED, true);
        }

        usb_hw_clear->sie_status = USB_SIE_STATUS_CONNECTED_BITS;
    }

    // SE0 for 2.5 us or more
    if (status & USB_INTS_BUS_RESET_BITS)
    {
        pico_trace("BUS RESET\n");
        usb_hw->dev_addr_ctrl = 0;
        handled |= USB_INTS_BUS_RESET_BITS;
        dcd_event_bus_reset(0, TUSB_SPEED_FULL, true);
        usb_hw_clear->sie_status = USB_SIE_STATUS_BUS_RESET_BITS;

#if TUD_OPT_RP2040_USB_DEVICE_ENUMERATION_FIX
        // Only run enumeration walk-around if pull up is enabled
        if ( usb_hw->sie_ctrl & USB_SIE_CTRL_PULLUP_EN_BITS )
        {
          rp2040_usb_device_enumeration_fix();
        }
#endif
    }

#if 0
    // TODO Enable SUSPEND & RESUME interrupt and test later on with/without VBUS detection

    /* Note from pico datasheet 4.1.2.6.4 (v1.2)
     * If you enable the suspend interrupt, it is likely you will see a suspend interrupt when
     * the device is first connected but the bus is idle. The bus can be idle for a few ms before
     * the host begins sending start of frame packets. You will also see a suspend interrupt
     * when the device is disconnected if you do not have a VBUS detect circuit connected. This is
     * because without VBUS detection, it is impossible to tell the difference between
     * being disconnected and suspended.
     */
    if (status & USB_INTS_DEV_SUSPEND_BITS)
    {
        handled |= USB_INTS_DEV_SUSPEND_BITS;
        dcd_event_bus_signal(0, DCD_EVENT_SUSPEND, true);
        usb_hw_clear->sie_status = USB_SIE_STATUS_SUSPENDED_BITS;
    }

    if (status & USB_INTS_DEV_RESUME_FROM_HOST_BITS)
    {
        handled |= USB_INTS_DEV_RESUME_FROM_HOST_BITS;
        dcd_event_bus_signal(0, DCD_EVENT_RESUME, true);
        usb_hw_clear->sie_status = USB_SIE_STATUS_RESUME_BITS;
    }
#endif

    if (status ^ handled)
    {
        panic("Unhandled IRQ 0x%x\n", (uint) (status ^ handled));
    }
}

#define USB_INTS_ERROR_BITS ( \
    USB_INTS_ERROR_DATA_SEQ_BITS      |  \
    USB_INTS_ERROR_BIT_STUFF_BITS     |  \
    USB_INTS_ERROR_CRC_BITS           |  \
    USB_INTS_ERROR_RX_OVERFLOW_BITS   |  \
    USB_INTS_ERROR_RX_TIMEOUT_BITS)

/*------------------------------------------------------------------*/
/* Controller API
 *------------------------------------------------------------------*/
void dcd_init (uint8_t rhport)
{
    pico_trace("dcd_init %d\n", rhport);
    assert(rhport == 0);

    // Reset hardware to default state
    rp2040_usb_init();

    irq_set_exclusive_handler(USBCTRL_IRQ, dcd_rp2040_irq);
    memset(hw_endpoints, 0, sizeof(hw_endpoints));
    next_buffer_ptr = &usb_dpram->epx_data[0];

    // EP0 always exists so init it now
    // EP0 OUT
    hw_endpoint_init(0x0, 64, 0);
    // EP0 IN
    hw_endpoint_init(0x80, 64, 0);

    // Initializes the USB peripheral for device mode and enables it.
    // Don't need to enable the pull up here. Force VBUS
    usb_hw->main_ctrl = USB_MAIN_CTRL_CONTROLLER_EN_BITS;

    // Enable individual controller IRQS here. Processor interrupt enable will be used
    // for the global interrupt enable...
    // TODO Enable SUSPEND & RESUME interrupt
    usb_hw->sie_ctrl = USB_SIE_CTRL_EP0_INT_1BUF_BITS; 
    usb_hw->inte     = USB_INTS_BUFF_STATUS_BITS | USB_INTS_BUS_RESET_BITS | USB_INTS_SETUP_REQ_BITS |
                       USB_INTS_DEV_CONN_DIS_BITS /* | USB_INTS_DEV_SUSPEND_BITS | USB_INTS_DEV_RESUME_FROM_HOST_BITS */  ;

    dcd_connect(rhport);
}

void dcd_int_enable(uint8_t rhport)
{
    assert(rhport == 0);
    irq_set_enabled(USBCTRL_IRQ, true);
}

void dcd_int_disable(uint8_t rhport)
{
    assert(rhport == 0);
    irq_set_enabled(USBCTRL_IRQ, false);
}

void dcd_set_address (uint8_t rhport, uint8_t dev_addr)
{
    pico_trace("dcd_set_address %d %d\n", rhport, dev_addr);
    assert(rhport == 0);

    // Can't set device address in hardware until status xfer has complete
    ep0_0len_status();
}

void dcd_remote_wakeup(uint8_t rhport)
{
    pico_info("dcd_remote_wakeup %d\n", rhport);
    assert(rhport == 0);
    usb_hw_set->sie_ctrl = USB_SIE_CTRL_RESUME_BITS;
}

// disconnect by disabling internal pull-up resistor on D+/D-
void dcd_disconnect(uint8_t rhport)
{
    pico_info("dcd_disconnect %d\n", rhport);
    assert(rhport == 0);
    usb_hw_clear->sie_ctrl = USB_SIE_CTRL_PULLUP_EN_BITS;
}

// connect by enabling internal pull-up resistor on D+/D-
void dcd_connect(uint8_t rhport)
{
    pico_info("dcd_connect %d\n", rhport);
    assert(rhport == 0);
    usb_hw_set->sie_ctrl = USB_SIE_CTRL_PULLUP_EN_BITS;
}

/*------------------------------------------------------------------*/
/* DCD Endpoint port
 *------------------------------------------------------------------*/

void dcd_edpt0_status_complete(uint8_t rhport, tusb_control_request_t const * request)
{
    pico_trace("dcd_edpt0_status_complete %d\n", rhport);
    assert(rhport == 0);

    if (request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_DEVICE &&
        request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD &&
        request->bRequest == TUSB_REQ_SET_ADDRESS)
    {
        pico_trace("Set HW address %d\n", assigned_address);
        usb_hw->dev_addr_ctrl = (uint8_t) request->wValue;
    }

    reset_ep0();
}

bool dcd_edpt_open (uint8_t rhport, tusb_desc_endpoint_t const * desc_edpt)
{
    pico_info("dcd_edpt_open %d %02x\n", rhport, desc_edpt->bEndpointAddress);
    assert(rhport == 0);
    hw_endpoint_init(desc_edpt->bEndpointAddress, desc_edpt->wMaxPacketSize.size, desc_edpt->bmAttributes.xfer);
    return true;
}

bool dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes)
{
    assert(rhport == 0);
    // True means start new xfer
    hw_endpoint_xfer(ep_addr, buffer, total_bytes, true);
    return true;
}

void dcd_edpt_stall (uint8_t rhport, uint8_t ep_addr)
{
    pico_trace("dcd_edpt_stall %d %02x\n", rhport, ep_addr);
    assert(rhport == 0);
    hw_endpoint_stall(ep_addr);
}

void dcd_edpt_clear_stall (uint8_t rhport, uint8_t ep_addr)
{
    pico_trace("dcd_edpt_clear_stall %d %02x\n", rhport, ep_addr);
    assert(rhport == 0);
    hw_endpoint_clear_stall(ep_addr);
}


void dcd_edpt_close (uint8_t rhport, uint8_t ep_addr)
{
    // usbd.c says: In progress transfers on this EP may be delivered after this call
    pico_trace("dcd_edpt_close %d %02x\n", rhport, ep_addr);

}

void dcd_int_handler(uint8_t rhport)
{
    (void) rhport;
    dcd_rp2040_irq();
}

#endif
