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

#if CFG_TUSB_MCU == OPT_MCU_RP2040

#include <stdlib.h>
#include "rp2040_usb.h"

// Direction strings for debug
const char *ep_dir_string[] = {
        "out",
        "in",
};

static inline void _hw_endpoint_lock_update(struct hw_endpoint *ep, int delta) {
    // todo add critsec as necessary to prevent issues between worker and IRQ...
    //  note that this is perhaps as simple as disabling IRQs because it would make
    //  sense to have worker and IRQ on same core, however I think using critsec is about equivalent.
}

#if TUSB_OPT_HOST_ENABLED
static inline void _hw_endpoint_update_last_buf(struct hw_endpoint *ep)
{
    ep->last_buf = (ep->len + ep->transfer_size == ep->total_len);
}
#endif

void rp2040_usb_init(void)
{
    // Reset usb controller
    reset_block(RESETS_RESET_USBCTRL_BITS);
    unreset_block_wait(RESETS_RESET_USBCTRL_BITS);

    // Clear any previous state just in case
    memset(usb_hw, 0, sizeof(*usb_hw));
    memset(usb_dpram, 0, sizeof(*usb_dpram));

    // Mux the controller to the onboard usb phy
    usb_hw->muxing    = USB_USB_MUXING_TO_PHY_BITS    | USB_USB_MUXING_SOFTCON_BITS;

    // Force VBUS detect so the device thinks it is plugged into a host
    // TODO support VBUs detect
    usb_hw->pwr       = USB_USB_PWR_VBUS_DETECT_BITS  | USB_USB_PWR_VBUS_DETECT_OVERRIDE_EN_BITS;
}

void hw_endpoint_reset_transfer(struct hw_endpoint *ep)
{
    ep->stalled = false;
    ep->active = false;
#if TUSB_OPT_HOST_ENABLED
    ep->sent_setup = false;
#endif
    ep->total_len = 0;
    ep->len = 0;
    ep->transfer_size = 0;
    ep->user_buf = 0;
}

void _hw_endpoint_buffer_control_update32(struct hw_endpoint *ep, uint32_t and_mask, uint32_t or_mask) {
    uint32_t value = 0;
    if (and_mask) {
        value = *ep->buffer_control & and_mask;
    }
    if (or_mask) {
        value |= or_mask;
        if (or_mask & USB_BUF_CTRL_AVAIL) {
            if (*ep->buffer_control & USB_BUF_CTRL_AVAIL) {
                panic("ep %d %s was already available", tu_edpt_number(ep->ep_addr), ep_dir_string[tu_edpt_dir(ep->ep_addr)]);
            }
            *ep->buffer_control = value & ~USB_BUF_CTRL_AVAIL;
            // 12 cycle delay.. (should be good for 48*12Mhz = 576Mhz)
            // Don't need delay in host mode as host is in charge
#if !TUSB_OPT_HOST_ENABLED
            __asm volatile (
                    "b 1f\n"
                    "1: b 1f\n"
                    "1: b 1f\n"
                    "1: b 1f\n"
                    "1: b 1f\n"
                    "1: b 1f\n"
                    "1:\n"
                    : : : "memory");
#endif
        }
    }
    *ep->buffer_control = value;
}

void _hw_endpoint_start_next_buffer(struct hw_endpoint *ep)
{
    // Prepare buffer control register value
    uint32_t val = ep->transfer_size | USB_BUF_CTRL_AVAIL;

    if (!ep->rx)
    {
        // Copy data from user buffer to hw buffer
        memcpy(ep->hw_data_buf, &ep->user_buf[ep->len], ep->transfer_size);
        // Mark as full
        val |= USB_BUF_CTRL_FULL;
    }

    // PID
    val |= ep->next_pid ? USB_BUF_CTRL_DATA1_PID : USB_BUF_CTRL_DATA0_PID;

#if TUSB_OPT_DEVICE_ENABLED
    ep->next_pid ^= 1u;

#else
    // For Host (also device but since we dictate the endpoint size, following scenario does not occur)
    // Next PID depends on the number of packet in case wMaxPacketSize < 64 (e.g Interrupt Endpoint 8, or 12)
    // Special case with control status stage where PID is always DATA1
    if ( ep->transfer_size == 0 )
    {
      // ZLP also toggle data
      ep->next_pid ^= 1u;
    }else
    {
      uint32_t packet_count = 1 + ((ep->transfer_size - 1) / ep->wMaxPacketSize);

      if ( packet_count & 0x01 )
      {
        ep->next_pid ^= 1u;
      }
    }
#endif


#if TUSB_OPT_HOST_ENABLED
    // Is this the last buffer? Only really matters for host mode. Will trigger
    // the trans complete irq but also stop it polling. We only really care about
    // trans complete for setup packets being sent
    if (ep->last_buf)
    {
        pico_trace("Last buf (%d bytes left)\n", ep->transfer_size);
        val |= USB_BUF_CTRL_LAST;
    }
#endif

    // Finally, write to buffer_control which will trigger the transfer
    // the next time the controller polls this dpram address
    _hw_endpoint_buffer_control_set_value32(ep, val);
    pico_trace("buffer control (0x%p) <- 0x%x\n", ep->buffer_control, val);
    //print_bufctrl16(val);
}


void _hw_endpoint_xfer_start(struct hw_endpoint *ep, uint8_t *buffer, uint16_t total_len)
{
    _hw_endpoint_lock_update(ep, 1);
    pico_trace("Start transfer of total len %d on ep %d %s\n", total_len, tu_edpt_number(ep->ep_addr), ep_dir_string[tu_edpt_dir(ep->ep_addr)]);
    if (ep->active)
    {
        // TODO: Is this acceptable for interrupt packets?
        pico_warn("WARN: starting new transfer on already active ep %d %s\n", tu_edpt_number(ep->ep_addr), ep_dir_string[tu_edpt_dir(ep->ep_addr)]);

        hw_endpoint_reset_transfer(ep);
    }

    // Fill in info now that we're kicking off the hw
    ep->total_len = total_len;
    ep->len = 0;

    // Limit by packet size but not less 64 (i.e low speed 8 bytes EP0)
    ep->transfer_size = tu_min16(total_len, tu_max16(64, ep->wMaxPacketSize));

    ep->active = true;
    ep->user_buf = buffer;
#if TUSB_OPT_HOST_ENABLED
    // Recalculate if this is the last buffer
    _hw_endpoint_update_last_buf(ep);
    ep->buf_sel = 0;
#endif

    _hw_endpoint_start_next_buffer(ep);
    _hw_endpoint_lock_update(ep, -1);
}

void _hw_endpoint_xfer_sync(struct hw_endpoint *ep)
{
    // Update hw endpoint struct with info from hardware
    // after a buff status interrupt

    uint32_t buf_ctrl = _hw_endpoint_buffer_control_get_value32(ep);

#if TUSB_OPT_HOST_ENABLED
    // RP2040-E4
    // tag::host_buf_sel_fix[]
    // TODO need changes to support double buffering
    if (ep->buf_sel == 1)
    {
        // Host can erroneously write status to top half of buf_ctrl register
        buf_ctrl = buf_ctrl >> 16;

        // update buf1 -> buf0 to prevent panic with "already available"
        *ep->buffer_control = buf_ctrl;
    }
    // Flip buf sel for host
    ep->buf_sel ^= 1u;
    // end::host_buf_sel_fix[]
#endif

    // Get tranferred bytes after adjusted buf sel
    uint16_t const transferred_bytes = buf_ctrl & USB_BUF_CTRL_LEN_MASK;

    // We are continuing a transfer here. If we are TX, we have successfullly
    // sent some data can increase the length we have sent
    if (!ep->rx)
    {
        assert(!(buf_ctrl & USB_BUF_CTRL_FULL));
        pico_trace("tx %d bytes (buf_ctrl 0x%08x)\n", transferred_bytes, buf_ctrl);
        ep->len += transferred_bytes;
    }
    else
    {
        // If we are OUT we have recieved some data, so can increase the length
        // we have recieved AFTER we have copied it to the user buffer at the appropriate
        // offset
        pico_trace("rx %d bytes (buf_ctrl 0x%08x)\n", transferred_bytes, buf_ctrl);
        assert(buf_ctrl & USB_BUF_CTRL_FULL);
        memcpy(&ep->user_buf[ep->len], ep->hw_data_buf, transferred_bytes);
        ep->len += transferred_bytes;
    }

    // Sometimes the host will send less data than we expect...
    // If this is a short out transfer update the total length of the transfer
    // to be the current length
    if ((ep->rx) && (transferred_bytes < ep->wMaxPacketSize))
    {
        pico_trace("Short rx transfer\n");
        // Reduce total length as this is last packet
        ep->total_len = ep->len;
    }
}

// Returns true if transfer is complete
bool _hw_endpoint_xfer_continue(struct hw_endpoint *ep)
{
    _hw_endpoint_lock_update(ep, 1);
    // Part way through a transfer
    if (!ep->active)
    {
        panic("Can't continue xfer on inactive ep %d %s", tu_edpt_number(ep->ep_addr), ep_dir_string);
    }

    // Update EP struct from hardware state
    _hw_endpoint_xfer_sync(ep);

    // Now we have synced our state with the hardware. Is there more data to transfer?
    // Limit by packet size but not less 64 (i.e low speed 8 bytes EP0)
    uint16_t remaining_bytes = ep->total_len - ep->len;
    ep->transfer_size = tu_min16(remaining_bytes, tu_max16(64, ep->wMaxPacketSize));
#if TUSB_OPT_HOST_ENABLED
    _hw_endpoint_update_last_buf(ep);
#endif

    // Can happen because of programmer error so check for it
    if (ep->len > ep->total_len)
    {
        panic("Transferred more data than expected");
    }

    // If we are done then notify tinyusb
    if (ep->len == ep->total_len)
    {
        pico_trace("Completed transfer of %d bytes on ep %d %s\n",
                   ep->len, tu_edpt_number(ep->ep_addr), ep_dir_string[tu_edpt_dir(ep->ep_addr)]);
        // Notify caller we are done so it can notify the tinyusb stack
        _hw_endpoint_lock_update(ep, -1);
        return true;
    }
    else
    {
        _hw_endpoint_start_next_buffer(ep);
    }

    _hw_endpoint_lock_update(ep, -1);
    // More work to do
    return false;
}

void _hw_endpoint_xfer(struct hw_endpoint *ep, uint8_t *buffer, uint16_t total_len, bool start)
{
    // Trace
    pico_trace("hw_endpoint_xfer ep %d %s", tu_edpt_number(ep->ep_addr), ep_dir_string[tu_edpt_dir(ep->ep_addr)]);
    pico_trace(" total_len %d, start=%d\n", total_len, start);

    assert(ep->configured);


    if (start)
    {
        _hw_endpoint_xfer_start(ep, buffer, total_len);
    }
    else
    {
        _hw_endpoint_xfer_continue(ep);
    }
}

#endif
