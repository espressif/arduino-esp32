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

#ifndef _TUSB_CDC_HOST_H_
#define _TUSB_CDC_HOST_H_

#include "cdc.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// Class Driver Configuration
//--------------------------------------------------------------------+

// Set Line Control state on enumeration/mounted: DTR ( bit 0), RTS (bit 1)
#ifndef CFG_TUH_CDC_LINE_CONTROL_ON_ENUM
#define CFG_TUH_CDC_LINE_CONTROL_ON_ENUM    0
#endif

// Set Line Coding on enumeration/mounted, value for cdc_line_coding_t
//#ifndef CFG_TUH_CDC_LINE_CODING_ON_ENUM
//#define CFG_TUH_CDC_LINE_CODING_ON_ENUM   { 115200, CDC_LINE_CODING_STOP_BITS_1, CDC_LINE_CODING_PARITY_NONE, 8 }
//#endif

// RX FIFO size
#ifndef CFG_TUH_CDC_RX_BUFSIZE
#define CFG_TUH_CDC_RX_BUFSIZE USBH_EPSIZE_BULK_MAX
#endif

// RX Endpoint size
#ifndef CFG_TUH_CDC_RX_EPSIZE
#define CFG_TUH_CDC_RX_EPSIZE  USBH_EPSIZE_BULK_MAX
#endif

// TX FIFO size
#ifndef CFG_TUH_CDC_TX_BUFSIZE
#define CFG_TUH_CDC_TX_BUFSIZE USBH_EPSIZE_BULK_MAX
#endif

// TX Endpoint size
#ifndef CFG_TUH_CDC_TX_EPSIZE
#define CFG_TUH_CDC_TX_EPSIZE  USBH_EPSIZE_BULK_MAX
#endif

//--------------------------------------------------------------------+
// Application API
//--------------------------------------------------------------------+

// Get Interface index from device address + interface number
// return TUSB_INDEX_INVALID_8 (0xFF) if not found
uint8_t tuh_cdc_itf_get_index(uint8_t daddr, uint8_t itf_num);

// Get Interface information
// return true if index is correct and interface is currently mounted
bool tuh_cdc_itf_get_info(uint8_t idx, tuh_itf_info_t* info);

// Check if a interface is mounted
bool tuh_cdc_mounted(uint8_t idx);

// Get current DTR status
bool tuh_cdc_get_dtr(uint8_t idx);

// Get current RTS status
bool tuh_cdc_get_rts(uint8_t idx);

// Check if interface is connected (DTR active)
TU_ATTR_ALWAYS_INLINE static inline bool tuh_cdc_connected(uint8_t idx)
{
  return tuh_cdc_get_dtr(idx);
}

// Get local (saved/cached) version of line coding.
// This function should return correct values if tuh_cdc_set_line_coding() / tuh_cdc_get_line_coding()
// are invoked previously or CFG_TUH_CDC_LINE_CODING_ON_ENUM is defined.
// NOTE: This function does not make any USB transfer request to device.
bool tuh_cdc_get_local_line_coding(uint8_t idx, cdc_line_coding_t* line_coding);

//--------------------------------------------------------------------+
// Write API
//--------------------------------------------------------------------+

// Get the number of bytes available for writing
uint32_t tuh_cdc_write_available(uint8_t idx);

// Write to cdc interface
uint32_t tuh_cdc_write(uint8_t idx, void const* buffer, uint32_t bufsize);

// Force sending data if possible, return number of forced bytes
uint32_t tuh_cdc_write_flush(uint8_t idx);

// Clear the transmit FIFO
bool tuh_cdc_write_clear(uint8_t idx);

//--------------------------------------------------------------------+
// Read API
//--------------------------------------------------------------------+

// Get the number of bytes available for reading
uint32_t tuh_cdc_read_available(uint8_t idx);

// Read from cdc interface
uint32_t tuh_cdc_read (uint8_t idx, void* buffer, uint32_t bufsize);

// Get a byte from RX FIFO without removing it
bool tuh_cdc_peek(uint8_t idx, uint8_t* ch);

// Clear the received FIFO
bool tuh_cdc_read_clear (uint8_t idx);

//--------------------------------------------------------------------+
// Control Endpoint (Request) API
// Each Function will make a USB control transfer request to/from device
// - If complete_cb is provided, the function will return immediately and invoke
// the callback when request is complete.
// - If complete_cb is NULL, the function will block until request is complete.
//   - In this case, user_data should be pointed to xfer_result_t to hold the transfer result.
//   - The function will return true if transfer is successful, false otherwise.
//--------------------------------------------------------------------+

// Request to Set Control Line State: DTR (bit 0), RTS (bit 1)
bool tuh_cdc_set_control_line_state(uint8_t idx, uint16_t line_state, tuh_xfer_cb_t complete_cb, uintptr_t user_data);

// Request to set baudrate
bool tuh_cdc_set_baudrate(uint8_t idx, uint32_t baudrate, tuh_xfer_cb_t complete_cb, uintptr_t user_data);

// Request to set data format
bool tuh_cdc_set_data_format(uint8_t idx, uint8_t stop_bits, uint8_t parity, uint8_t data_bits, tuh_xfer_cb_t complete_cb, uintptr_t user_data);

// Request to Set Line Coding = baudrate + data format
// Note: only implemented by ACM and CH34x, not supported by FTDI and CP210x yet
bool tuh_cdc_set_line_coding(uint8_t idx, cdc_line_coding_t const* line_coding, tuh_xfer_cb_t complete_cb, uintptr_t user_data);

// Request to Get Line Coding (ACM only)
// Should only use if tuh_cdc_set_line_coding() / tuh_cdc_get_line_coding() never got invoked and
// CFG_TUH_CDC_LINE_CODING_ON_ENUM is not defined
// bool tuh_cdc_get_line_coding(uint8_t idx, cdc_line_coding_t* coding);

// Connect by set both DTR, RTS
TU_ATTR_ALWAYS_INLINE static inline
bool tuh_cdc_connect(uint8_t idx, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  return tuh_cdc_set_control_line_state(idx, CDC_CONTROL_LINE_STATE_DTR | CDC_CONTROL_LINE_STATE_RTS, complete_cb, user_data);
}

// Disconnect by clear both DTR, RTS
TU_ATTR_ALWAYS_INLINE static inline
bool tuh_cdc_disconnect(uint8_t idx, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  return tuh_cdc_set_control_line_state(idx, 0x00, complete_cb, user_data);
}

//--------------------------------------------------------------------+
// CDC APPLICATION CALLBACKS
//--------------------------------------------------------------------+

// Invoked when a device with CDC interface is mounted
// idx is index of cdc interface in the internal pool.
TU_ATTR_WEAK extern void tuh_cdc_mount_cb(uint8_t idx);

// Invoked when a device with CDC interface is unmounted
TU_ATTR_WEAK extern void tuh_cdc_umount_cb(uint8_t idx);

// Invoked when received new data
TU_ATTR_WEAK extern void tuh_cdc_rx_cb(uint8_t idx);

// Invoked when a TX is complete and therefore space becomes available in TX buffer
TU_ATTR_WEAK extern void tuh_cdc_tx_complete_cb(uint8_t idx);

//--------------------------------------------------------------------+
// Internal Class Driver API
//--------------------------------------------------------------------+
bool cdch_init       (void);
bool cdch_deinit     (void);
bool cdch_open       (uint8_t rhport, uint8_t dev_addr, tusb_desc_interface_t const *itf_desc, uint16_t max_len);
bool cdch_set_config (uint8_t dev_addr, uint8_t itf_num);
bool cdch_xfer_cb    (uint8_t dev_addr, uint8_t ep_addr, xfer_result_t event, uint32_t xferred_bytes);
void cdch_close      (uint8_t dev_addr);

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_CDC_HOST_H_ */
