/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Jerzy Kasenberg
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

#ifndef _TUSB_BTH_DEVICE_H_
#define _TUSB_BTH_DEVICE_H_

#include <common/tusb_common.h>
#include <device/usbd.h>

//--------------------------------------------------------------------+
// Class Driver Configuration
//--------------------------------------------------------------------+
#ifndef CFG_TUD_BTH_EVENT_EPSIZE
#define CFG_TUD_BTH_EVENT_EPSIZE     16
#endif
#ifndef CFG_TUD_BTH_DATA_EPSIZE
#define CFG_TUD_BTH_DATA_EPSIZE      64
#endif

typedef struct TU_ATTR_PACKED
{
  uint16_t op_code;
  uint8_t param_length;
  uint8_t param[255];
} bt_hci_cmd_t;

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// Application Callback API (weak is optional)
//--------------------------------------------------------------------+

// Invoked when HCI command was received over USB from Bluetooth host.
// Detailed format is described in Bluetooth core specification Vol 2,
// Part E, 5.4.1.
// Length of the command is from 3 bytes (2 bytes for OpCode,
// 1 byte for parameter total length) to 258.
TU_ATTR_WEAK void tud_bt_hci_cmd_cb(void *hci_cmd, size_t cmd_len);

// Invoked when ACL data was received over USB from Bluetooth host.
// Detailed format is described in Bluetooth core specification Vol 2,
// Part E, 5.4.2.
// Length is from 4 bytes, (12 bits for Handle, 4 bits for flags
// and 16 bits for data total length) to endpoint size.
TU_ATTR_WEAK void tud_bt_acl_data_received_cb(void *acl_data, uint16_t data_len);

// Called when event sent with tud_bt_event_send() was delivered to BT stack.
// Controller can release/reuse buffer with Event packet at this point.
TU_ATTR_WEAK void tud_bt_event_sent_cb(uint16_t sent_bytes);

// Called when ACL data that was sent with tud_bt_acl_data_send()
// was delivered to BT stack.
// Controller can release/reuse buffer with ACL packet at this point.
TU_ATTR_WEAK void tud_bt_acl_data_sent_cb(uint16_t sent_bytes);

// Bluetooth controller calls this function when it wants to send even packet
// as described in Bluetooth core specification Vol 2, Part E, 5.4.4.
// Event has at least 2 bytes, first is Event code second contains parameter
// total length. Controller can release/reuse event memory after
// tud_bt_event_sent_cb() is called.
bool tud_bt_event_send(void *event, uint16_t event_len);

// Bluetooth controller calls this to send ACL data packet
// as described in Bluetooth core specification Vol 2, Part E, 5.4.2
// Minimum length is 4 bytes, (12 bits for Handle, 4 bits for flags
// and 16 bits for data total length). Upper limit is not limited
// to endpoint size since buffer is allocate by controller
// and must not be reused till tud_bt_acl_data_sent_cb() is called.
bool tud_bt_acl_data_send(void *acl_data, uint16_t data_len);

//--------------------------------------------------------------------+
// Internal Class Driver API
//--------------------------------------------------------------------+
void     btd_init             (void);
void     btd_reset            (uint8_t rhport);
uint16_t btd_open             (uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t max_len);
bool     btd_control_request  (uint8_t rhport, tusb_control_request_t const * request);
bool     btd_control_complete (uint8_t rhport, tusb_control_request_t const * request);
bool     btd_xfer_cb          (uint8_t rhport, uint8_t edpt_addr, xfer_result_t result, uint32_t xferred_bytes);

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_BTH_DEVICE_H_ */
