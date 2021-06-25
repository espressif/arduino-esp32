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

#if TUSB_OPT_HOST_ENABLED && CFG_TUSB_MCU == OPT_MCU_RP2040

#include "pico.h"
#include "rp2040_usb.h"

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "osal/osal.h"

#include "host/hcd.h"
#include "host/usbh.h"
#include "host/usbh_hcd.h"

#define ROOT_PORT 0

//--------------------------------------------------------------------+
// Low level rp2040 controller functions
//--------------------------------------------------------------------+

#ifndef PICO_USB_HOST_INTERRUPT_ENDPOINTS
#define PICO_USB_HOST_INTERRUPT_ENDPOINTS (USB_MAX_ENDPOINTS - 1)
#endif
static_assert(PICO_USB_HOST_INTERRUPT_ENDPOINTS <= USB_MAX_ENDPOINTS, "");

// Host mode uses one shared endpoint register for non-interrupt endpoint
struct hw_endpoint eps[1 + PICO_USB_HOST_INTERRUPT_ENDPOINTS];
#define epx (eps[0])

#define usb_hw_set hw_set_alias(usb_hw)
#define usb_hw_clear hw_clear_alias(usb_hw)

// Used for hcd pipe busy.
// todo still a bit wasteful
// top bit set if valid
uint8_t dev_ep_map[CFG_TUSB_HOST_DEVICE_MAX][1 + PICO_USB_HOST_INTERRUPT_ENDPOINTS][2];

// Flags we set by default in sie_ctrl (we add other bits on top)
static uint32_t sie_ctrl_base = USB_SIE_CTRL_SOF_EN_BITS | 
                                USB_SIE_CTRL_KEEP_ALIVE_EN_BITS | 
                                USB_SIE_CTRL_PULLDOWN_EN_BITS | 
                                USB_SIE_CTRL_EP0_INT_1BUF_BITS;

static struct hw_endpoint *get_dev_ep(uint8_t dev_addr, uint8_t ep_addr)
{
    uint8_t num = tu_edpt_number(ep_addr);
    if (num == 0) {
        return &epx;
    }
    uint8_t in = (ep_addr & TUSB_DIR_IN_MASK) ? 1 : 0;
    uint mapping = dev_ep_map[dev_addr-1][num][in];
    pico_trace("Get dev addr %d ep %d = %d\n", dev_addr, ep_addr, mapping);
    return mapping >= 128 ? eps + (mapping & 0x7fu) : NULL;
}

static void set_dev_ep(uint8_t dev_addr, uint8_t ep_addr, struct hw_endpoint *ep)
{
    uint8_t num = tu_edpt_number(ep_addr);
    uint8_t in = (ep_addr & TUSB_DIR_IN_MASK) ? 1 : 0;
    uint32_t index = ep - eps;
    hard_assert(index < TU_ARRAY_SIZE(eps));
    // todo revisit why dev_addr can be 0 here
    if (dev_addr) {
        dev_ep_map[dev_addr-1][num][in] = 128u | index;
    }
    pico_trace("Set dev addr %d ep %d = %d\n", dev_addr, ep_addr, index);
}

static inline uint8_t dev_speed(void)
{
    return (usb_hw->sie_status & USB_SIE_STATUS_SPEED_BITS) >> USB_SIE_STATUS_SPEED_LSB;
}

static bool need_pre(uint8_t dev_addr)
{
    // If this device is different to the speed of the root device
    // (i.e. is a low speed device on a full speed hub) then need pre
    return hcd_port_speed_get(0) != tuh_device_get_speed(dev_addr);
}

static void hw_xfer_complete(struct hw_endpoint *ep, xfer_result_t xfer_result)
{
    // Mark transfer as done before we tell the tinyusb stack
    uint8_t dev_addr = ep->dev_addr;
    uint8_t ep_addr = ep->ep_addr;
    uint total_len = ep->total_len;
    hw_endpoint_reset_transfer(ep);
    hcd_event_xfer_complete(dev_addr, ep_addr, total_len, xfer_result, true);
}

static void _handle_buff_status_bit(uint bit, struct hw_endpoint *ep)
{
    usb_hw_clear->buf_status = bit;
    bool done = _hw_endpoint_xfer_continue(ep);
    if (done)
    {
        hw_xfer_complete(ep, XFER_RESULT_SUCCESS);
    }
}

static void hw_handle_buff_status(void)
{
    uint32_t remaining_buffers = usb_hw->buf_status;
    pico_trace("buf_status 0x%08x\n", remaining_buffers);

    // Check EPX first
    uint bit = 0b1;
    if (remaining_buffers & bit)
    {
        remaining_buffers &= ~bit;
        struct hw_endpoint *ep = &epx;
        _handle_buff_status_bit(bit, ep);
    }

    // Check interrupt endpoints
    for (uint i = 1; i <= USB_HOST_INTERRUPT_ENDPOINTS && remaining_buffers; i++)
    {
        // EPX is bit 0
        // IEP1 is bit 2
        // IEP2 is bit 4
        // IEP3 is bit 6
        // etc
        bit = 1 << (i*2);

        if (remaining_buffers & bit)
        {
            remaining_buffers &= ~bit;
            _handle_buff_status_bit(bit, &eps[i]);
        }
    }

    if (remaining_buffers)
    {
        panic("Unhandled buffer %d\n", remaining_buffers);
    }
}

static void hw_trans_complete(void)
{
    struct hw_endpoint *ep = &epx;
    assert(ep->active);

    if (ep->sent_setup)
    {
        pico_trace("Sent setup packet\n");
        hw_xfer_complete(ep, XFER_RESULT_SUCCESS);
    }
    else
    {
        // Don't care. Will handle this in buff status
        return;
    }
}

static void hcd_rp2040_irq(void)
{
    uint32_t status = usb_hw->ints;
    uint32_t handled = 0;

    if (status & USB_INTS_HOST_CONN_DIS_BITS)
    {
        handled |= USB_INTS_HOST_CONN_DIS_BITS;
        
        if (dev_speed())
        {
            hcd_event_device_attach(ROOT_PORT, true);
        }
        else
        {
            hcd_event_device_remove(ROOT_PORT, true);
        }

        // Clear speed change interrupt
        usb_hw_clear->sie_status = USB_SIE_STATUS_SPEED_BITS;
    }

    if (status & USB_INTS_TRANS_COMPLETE_BITS)
    {
        handled |= USB_INTS_TRANS_COMPLETE_BITS;
        usb_hw_clear->sie_status = USB_SIE_STATUS_TRANS_COMPLETE_BITS;
        hw_trans_complete();
    }

    if (status & USB_INTS_BUFF_STATUS_BITS)
    {
        handled |= USB_INTS_BUFF_STATUS_BITS;
        // print_bufctrl32(*epx.buffer_control);
        hw_handle_buff_status();
    }

    if (status & USB_INTS_STALL_BITS)
    {
        // We have rx'd a stall from the device
        pico_trace("Stall REC\n");
        handled |= USB_INTS_STALL_BITS;
        usb_hw_clear->sie_status = USB_SIE_STATUS_STALL_REC_BITS;
        hw_xfer_complete(&epx, XFER_RESULT_STALLED);
    }

    if (status & USB_INTS_ERROR_RX_TIMEOUT_BITS)
    {
        handled |= USB_INTS_ERROR_RX_TIMEOUT_BITS;
        usb_hw_clear->sie_status = USB_SIE_STATUS_RX_TIMEOUT_BITS;
    }

    if (status & USB_INTS_ERROR_DATA_SEQ_BITS)
    {
        usb_hw_clear->sie_status = USB_SIE_STATUS_DATA_SEQ_ERROR_BITS;
        // print_bufctrl32(*epx.buffer_control);
        panic("Data Seq Error \n");
    }

    if (status ^ handled)
    {
        panic("Unhandled IRQ 0x%x\n", (uint) (status ^ handled));
    }
}

static struct hw_endpoint *_next_free_interrupt_ep(void)
{
    struct hw_endpoint *ep = NULL;
    for (uint i = 1; i < TU_ARRAY_SIZE(eps); i++)
    {
        ep = &eps[i];
        if (!ep->configured)
        {
            // Will be configured by _hw_endpoint_init / _hw_endpoint_allocate
            ep->interrupt_num = i - 1;
            return ep;
        }
    }
    return ep;
}

static struct hw_endpoint *_hw_endpoint_allocate(uint8_t transfer_type)
{
    struct hw_endpoint *ep = NULL;
    if (transfer_type == TUSB_XFER_INTERRUPT)
    {
        ep = _next_free_interrupt_ep();
        pico_info("Allocate interrupt ep %d\n", ep->interrupt_num);
        assert(ep);
        ep->buffer_control = &usbh_dpram->int_ep_buffer_ctrl[ep->interrupt_num].ctrl;
        ep->endpoint_control = &usbh_dpram->int_ep_ctrl[ep->interrupt_num].ctrl;
        // 0x180 for epx
        // 0x1c0 for intep0
        // 0x200 for intep1
        // etc
        ep->hw_data_buf = &usbh_dpram->epx_data[64 * (ep->interrupt_num + 1)];
    }
    else
    {
        ep = &epx;
        ep->buffer_control = &usbh_dpram->epx_buf_ctrl;
        ep->endpoint_control = &usbh_dpram->epx_ctrl;
        ep->hw_data_buf = &usbh_dpram->epx_data[0];
    }
    return ep;
}

static void _hw_endpoint_init(struct hw_endpoint *ep, uint8_t dev_addr, uint8_t ep_addr, uint wMaxPacketSize, uint8_t transfer_type, uint8_t bmInterval)
{
    // Already has data buffer, endpoint control, and buffer control allocated at this point
    assert(ep->endpoint_control);
    assert(ep->buffer_control);
    assert(ep->hw_data_buf);

    uint8_t const num = tu_edpt_number(ep_addr);
    tusb_dir_t const dir = tu_edpt_dir(ep_addr);

    ep->ep_addr = ep_addr;
    ep->dev_addr = dev_addr;

    // For host, IN to host == RX, anything else rx == false
    ep->rx = (dir == TUSB_DIR_IN);

    // Response to a setup packet on EP0 starts with pid of 1
    ep->next_pid = num == 0 ? 1u : 0u;
    ep->wMaxPacketSize = wMaxPacketSize;
    ep->transfer_type = transfer_type;

    pico_trace("hw_endpoint_init dev %d ep %d %s xfer %d\n", ep->dev_addr, tu_edpt_number(ep->ep_addr), ep_dir_string[tu_edpt_dir(ep->ep_addr)], ep->transfer_type);
    pico_trace("dev %d ep %d %s setup buffer @ 0x%p\n", ep->dev_addr, tu_edpt_number(ep->ep_addr), ep_dir_string[tu_edpt_dir(ep->ep_addr)], ep->hw_data_buf);
    uint dpram_offset = hw_data_offset(ep->hw_data_buf);
    // Bits 0-5 should be 0
    assert(!(dpram_offset & 0b111111));

    // Fill in endpoint control register with buffer offset
    uint32_t ep_reg =  EP_CTRL_ENABLE_BITS
                  | EP_CTRL_INTERRUPT_PER_BUFFER
                  | (ep->transfer_type << EP_CTRL_BUFFER_TYPE_LSB)
                  | dpram_offset;
    ep_reg |= bmInterval ? (bmInterval - 1) << EP_CTRL_HOST_INTERRUPT_INTERVAL_LSB : 0;
    *ep->endpoint_control = ep_reg;
    pico_trace("endpoint control (0x%p) <- 0x%x\n", ep->endpoint_control, ep_reg);
    ep->configured = true;

    if (bmInterval)
    {
        // This is an interrupt endpoint
        // so need to set up interrupt endpoint address control register with:
        // device address
        // endpoint number / direction
        // preamble
        uint32_t reg = dev_addr | (num << USB_ADDR_ENDP1_ENDPOINT_LSB);
        // Assert the interrupt endpoint is IN_TO_HOST
        assert(dir == TUSB_DIR_IN);

        if (need_pre(dev_addr))
        {
            reg |= USB_ADDR_ENDP1_INTEP_PREAMBLE_BITS;
        }
        usb_hw->int_ep_addr_ctrl[ep->interrupt_num] = reg;

        // Finally, enable interrupt that endpoint
        usb_hw_set->int_ep_ctrl = 1 << (ep->interrupt_num + 1);

        // If it's an interrupt endpoint we need to set up the buffer control
        // register

    }
}

static void hw_endpoint_init(uint8_t dev_addr, const tusb_desc_endpoint_t *ep_desc)
{
    // Allocated differently based on if it's an interrupt endpoint or not
    struct hw_endpoint *ep = _hw_endpoint_allocate(ep_desc->bmAttributes.xfer);
    _hw_endpoint_init(ep,
        dev_addr,
        ep_desc->bEndpointAddress,
        ep_desc->wMaxPacketSize.size,
        ep_desc->bmAttributes.xfer,
        ep_desc->bInterval);
    // Map this struct to ep@device address
    set_dev_ep(dev_addr, ep_desc->bEndpointAddress, ep);
}

//--------------------------------------------------------------------+
// HCD API
//--------------------------------------------------------------------+
bool hcd_init(uint8_t rhport)
{
    pico_trace("hcd_init %d\n", rhport);
    assert(rhport == 0);

    // Reset any previous state
    rp2040_usb_init();

    irq_set_exclusive_handler(USBCTRL_IRQ, hcd_rp2040_irq);

    // clear epx and interrupt eps
    memset(&eps, 0, sizeof(eps));

    // Enable in host mode with SOF / Keep alive on
    usb_hw->main_ctrl = USB_MAIN_CTRL_CONTROLLER_EN_BITS | USB_MAIN_CTRL_HOST_NDEVICE_BITS;
    usb_hw->sie_ctrl = sie_ctrl_base;
    usb_hw->inte = USB_INTE_BUFF_STATUS_BITS      | 
                   USB_INTE_HOST_CONN_DIS_BITS    | 
                   USB_INTE_HOST_RESUME_BITS      | 
                   USB_INTE_STALL_BITS            | 
                   USB_INTE_TRANS_COMPLETE_BITS   |
                   USB_INTE_ERROR_RX_TIMEOUT_BITS |
                   USB_INTE_ERROR_DATA_SEQ_BITS   ;

    return true;
}

void hcd_port_reset(uint8_t rhport)
{
    pico_trace("hcd_port_reset\n");
    assert(rhport == 0);
    // TODO: Nothing to do here yet. Perhaps need to reset some state?
}

bool hcd_port_connect_status(uint8_t rhport)
{
    pico_trace("hcd_port_connect_status\n");
    assert(rhport == 0);
    return usb_hw->sie_status & USB_SIE_STATUS_SPEED_BITS;
}

tusb_speed_t hcd_port_speed_get(uint8_t rhport)
{
    pico_trace("hcd_port_speed_get\n");
    assert(rhport == 0);
    // TODO: Should enumval this register
    switch (dev_speed())
    {
        case 1:
            return TUSB_SPEED_LOW;
        case 2:
            return TUSB_SPEED_FULL;
        default:
            panic("Invalid speed\n");
    }
}

// Close all opened endpoint belong to this device
void hcd_device_close(uint8_t rhport, uint8_t dev_addr)
{
    pico_trace("hcd_device_close %d\n", dev_addr);
}

void hcd_int_enable(uint8_t rhport)
{
    assert(rhport == 0);
    irq_set_enabled(USBCTRL_IRQ, true);
}

void hcd_int_disable(uint8_t rhport)
{
    // todo we should check this is disabling from the correct core; note currently this is never called
    assert(rhport == 0);
    irq_set_enabled(USBCTRL_IRQ, false);
}

bool hcd_edpt_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr, uint8_t * buffer, uint16_t buflen)
{
    pico_info("hcd_edpt_xfer dev_addr %d, ep_addr 0x%x, len %d\n", dev_addr, ep_addr, buflen);
    
    // Get appropriate ep. Either EPX or interrupt endpoint
    struct hw_endpoint *ep = get_dev_ep(dev_addr, ep_addr);
    assert(ep);

    if (ep_addr != ep->ep_addr)
    {
        // Direction has flipped so re init it but with same properties
        // TODO treat IN and OUT as invidual endpoints
        _hw_endpoint_init(ep, dev_addr, ep_addr, ep->wMaxPacketSize, ep->transfer_type, 0);
    }

    // True indicates this is the start of the transfer
    _hw_endpoint_xfer(ep, buffer, buflen, true);

    // If a normal transfer (non-interrupt) then initiate using
    // sie ctrl registers. Otherwise interrupt ep registers should
    // already be configured
    if (ep == &epx) {
        // That has set up buffer control, endpoint control etc
        // for host we have to initiate the transfer
        usb_hw->dev_addr_ctrl = dev_addr | (tu_edpt_number(ep_addr) << USB_ADDR_ENDP_ENDPOINT_LSB);
        uint32_t flags = USB_SIE_CTRL_START_TRANS_BITS | sie_ctrl_base;
        flags |= ep->rx ? USB_SIE_CTRL_RECEIVE_DATA_BITS : USB_SIE_CTRL_SEND_DATA_BITS;
        // Set pre if we are a low speed device on full speed hub
        flags |= need_pre(dev_addr) ? USB_SIE_CTRL_PREAMBLE_EN_BITS : 0;
        usb_hw->sie_ctrl = flags;
    }

    return true;
}

bool hcd_setup_send(uint8_t rhport, uint8_t dev_addr, uint8_t const setup_packet[8])
{
    pico_info("hcd_setup_send dev_addr %d\n", dev_addr);
    
    // Copy data into setup packet buffer
    memcpy((void*)&usbh_dpram->setup_packet[0], setup_packet, 8);

    // Configure EP0 struct with setup info for the trans complete
    struct hw_endpoint *ep = _hw_endpoint_allocate(0);
    // EP0 out
    _hw_endpoint_init(ep, dev_addr, 0x00, ep->wMaxPacketSize, 0, 0);
    assert(ep->configured);
    ep->total_len = 8;
    ep->transfer_size = 8;
    ep->active = true;
    ep->sent_setup = true;

    // Set device address
    usb_hw->dev_addr_ctrl = dev_addr;
    // Set pre if we are a low speed device on full speed hub
    uint32_t flags = sie_ctrl_base | USB_SIE_CTRL_SEND_SETUP_BITS | USB_SIE_CTRL_START_TRANS_BITS;
    flags |= need_pre(dev_addr) ? USB_SIE_CTRL_PREAMBLE_EN_BITS : 0;
    usb_hw->sie_ctrl = flags;
    return true;
}

uint32_t hcd_frame_number(uint8_t rhport)
{
    return usb_hw->sof_rd;
}

bool hcd_edpt_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_endpoint_t const * ep_desc)
{
    pico_trace("hcd_edpt_open dev_addr %d, ep_addr %d\n", dev_addr, ep_desc->bEndpointAddress);
    hw_endpoint_init(dev_addr, ep_desc);
    return true;
}

//bool hcd_edpt_busy(uint8_t dev_addr, uint8_t ep_addr)
//{
//    // EPX is shared, so multiple device addresses and endpoint addresses share that
//    // so if any transfer is active on epx, we are busy. Interrupt endpoints have their own
//    // EPX so ep->active will only be busy if there is a pending transfer on that interrupt endpoint
//    // on that device
//    pico_trace("hcd_edpt_busy dev addr %d ep_addr 0x%x\n", dev_addr, ep_addr);
//    struct hw_endpoint *ep = get_dev_ep(dev_addr, ep_addr);
//    assert(ep);
//    bool busy = ep->active;
//    pico_trace("busy == %d\n", busy);
//    return busy;
//}

bool hcd_edpt_clear_stall(uint8_t dev_addr, uint8_t ep_addr)
{
    panic("hcd_clear_stall");
    return true;
}

#endif
