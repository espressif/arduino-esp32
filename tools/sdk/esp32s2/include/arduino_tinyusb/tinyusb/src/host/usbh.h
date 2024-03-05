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

#ifndef _TUSB_USBH_H_
#define _TUSB_USBH_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include "common/tusb_common.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

// forward declaration
struct tuh_xfer_s;
typedef struct tuh_xfer_s tuh_xfer_t;

typedef void (*tuh_xfer_cb_t)(tuh_xfer_t* xfer);

// Note1: layout and order of this will be changed in near future
// it is advised to initialize it using member name
// Note2: not all field is available/meaningful in callback,
// some info is not saved by usbh to save SRAM
struct tuh_xfer_s {
  uint8_t daddr;
  uint8_t ep_addr;
  uint8_t TU_RESERVED;      // reserved
  xfer_result_t result;

  uint32_t actual_len;      // excluding setup packet

  union {
    tusb_control_request_t const* setup; // setup packet pointer if control transfer
    uint32_t buflen;                     // expected length if not control transfer (not available in callback)
  };

  uint8_t* buffer;           // not available in callback if not control transfer
  tuh_xfer_cb_t complete_cb;
  uintptr_t user_data;

  // uint32_t timeout_ms;    // place holder, not supported yet
};

// Subject to change
typedef struct {
  uint8_t daddr;
  tusb_desc_interface_t desc;
} tuh_itf_info_t;

// ConfigID for tuh_config()
enum {
  TUH_CFGID_RPI_PIO_USB_CONFIGURATION = OPT_MCU_RP2040 << 8 // cfg_param: pio_usb_configuration_t
};

//--------------------------------------------------------------------+
// APPLICATION CALLBACK
//--------------------------------------------------------------------+

//TU_ATTR_WEAK uint8_t tuh_attach_cb (tusb_desc_device_t const *desc_device);

// Invoked when a device is mounted (configured)
TU_ATTR_WEAK void tuh_mount_cb (uint8_t daddr);

// Invoked when a device failed to mount during enumeration process
// TU_ATTR_WEAK void tuh_mount_failed_cb (uint8_t daddr);

// Invoked when a device is unmounted (detached)
TU_ATTR_WEAK void tuh_umount_cb(uint8_t daddr);

// Invoked when there is a new usb event, which need to be processed by tuh_task()/tuh_task_ext()
void tuh_event_hook_cb(uint8_t rhport, uint32_t eventid, bool in_isr);

//--------------------------------------------------------------------+
// APPLICATION API
//--------------------------------------------------------------------+

// Configure host stack behavior with dynamic or port-specific parameters.
// Should be called before tuh_init()
// - cfg_id   : configure ID (TBD)
// - cfg_param: configure data, structure depends on the ID
bool tuh_configure(uint8_t rhport, uint32_t cfg_id, const void* cfg_param);

// Init host stack
bool tuh_init(uint8_t rhport);

// Check if host stack is already initialized with any roothub ports
bool tuh_inited(void);

// Task function should be called in main/rtos loop, extended version of tuh_task()
// - timeout_ms: millisecond to wait, zero = no wait, 0xFFFFFFFF = wait forever
// - in_isr: if function is called in ISR
void tuh_task_ext(uint32_t timeout_ms, bool in_isr);

// Task function should be called in main/rtos loop
TU_ATTR_ALWAYS_INLINE static inline
void tuh_task(void) {
  tuh_task_ext(UINT32_MAX, false);
}

// Check if there is pending events need processing by tuh_task()
bool tuh_task_event_ready(void);

#ifndef _TUSB_HCD_H_
extern void hcd_int_handler(uint8_t rhport, bool in_isr);
#endif

// Interrupt handler alias to HCD with in_isr as optional parameter
// - tuh_int_handler(rhport) --> hcd_int_handler(rhport, true)
// - tuh_int_handler(rhport, in_isr) --> hcd_int_handler(rhport, in_isr)
// Note: this is similar to TU_VERIFY(), _GET_3RD_ARG() is defined in tusb_verify.h
#define _tuh_int_handler_1arg(_rhport)            hcd_int_handler(_rhport, true)
#define _tuh_int_hanlder_2arg(_rhport, _in_isr)   hcd_int_handler(_rhport, _in_isr)
#define tuh_int_handler(...)   _GET_3RD_ARG(__VA_ARGS__, _tuh_int_hanlder_2arg, _tuh_int_handler_1arg, _dummy)(__VA_ARGS__)

// Check if roothub port is initialized and active as a host
bool tuh_rhport_is_active(uint8_t rhport);

// Assert/de-assert Bus Reset signal to roothub port. USB specs: it should last 10-50ms
bool tuh_rhport_reset_bus(uint8_t rhport, bool active);

//--------------------------------------------------------------------+
// Device API
//--------------------------------------------------------------------+

// Get VID/PID of device
bool tuh_vid_pid_get(uint8_t daddr, uint16_t* vid, uint16_t* pid);

// Get speed of device
tusb_speed_t tuh_speed_get(uint8_t daddr);

// Check if device is connected and configured
bool tuh_mounted(uint8_t daddr);

// Check if device is suspended
TU_ATTR_ALWAYS_INLINE static inline
bool tuh_suspended(uint8_t daddr) {
  // TODO implement suspend & resume on host
  (void) daddr;
  return false;
}

// Check if device is ready to communicate with
TU_ATTR_ALWAYS_INLINE static inline
bool tuh_ready(uint8_t daddr) {
  return tuh_mounted(daddr) && !tuh_suspended(daddr);
}

//--------------------------------------------------------------------+
// Transfer API
//--------------------------------------------------------------------+

// Submit a control transfer
//  - async: complete callback invoked when finished.
//  - sync : blocking if complete callback is NULL.
bool tuh_control_xfer(tuh_xfer_t* xfer);

// Submit a bulk/interrupt transfer
//  - async: complete callback invoked when finished.
//  - sync : blocking if complete callback is NULL.
bool tuh_edpt_xfer(tuh_xfer_t* xfer);

// Open a non-control endpoint
bool tuh_edpt_open(uint8_t daddr, tusb_desc_endpoint_t const * desc_ep);

// Abort a queued transfer. Note: it can only abort transfer that has not been started
// Return true if a queued transfer is aborted, false if there is no transfer to abort
bool tuh_edpt_abort_xfer(uint8_t daddr, uint8_t ep_addr);

// Set Configuration (control transfer)
// config_num = 0 will un-configure device. Note: config_num = config_descriptor_index + 1
// true on success, false if there is on-going control transfer or incorrect parameters
// if complete_cb == NULL i.e blocking, user_data should be pointed to xfer_reuslt_t*
bool tuh_configuration_set(uint8_t daddr, uint8_t config_num,
                           tuh_xfer_cb_t complete_cb, uintptr_t user_data);

// Set Interface (control transfer)
// true on success, false if there is on-going control transfer or incorrect parameters
// if complete_cb == NULL i.e blocking, user_data should be pointed to xfer_reuslt_t*
bool tuh_interface_set(uint8_t daddr, uint8_t itf_num, uint8_t itf_alt,
                       tuh_xfer_cb_t complete_cb, uintptr_t user_data);

//--------------------------------------------------------------------+
// Descriptors Asynchronous (non-blocking)
//--------------------------------------------------------------------+

// Get an descriptor (control transfer)
// true on success, false if there is on-going control transfer or incorrect parameters
bool tuh_descriptor_get(uint8_t daddr, uint8_t type, uint8_t index, void* buffer, uint16_t len,
                        tuh_xfer_cb_t complete_cb, uintptr_t user_data);

// Get device descriptor (control transfer)
// true on success, false if there is on-going control transfer or incorrect parameters
bool tuh_descriptor_get_device(uint8_t daddr, void* buffer, uint16_t len,
                               tuh_xfer_cb_t complete_cb, uintptr_t user_data);

// Get configuration descriptor (control transfer)
// true on success, false if there is on-going control transfer or incorrect parameters
bool tuh_descriptor_get_configuration(uint8_t daddr, uint8_t index, void* buffer, uint16_t len,
                                      tuh_xfer_cb_t complete_cb, uintptr_t user_data);

// Get HID report descriptor (control transfer)
// true on success, false if there is on-going control transfer or incorrect parameters
bool tuh_descriptor_get_hid_report(uint8_t daddr, uint8_t itf_num, uint8_t desc_type, uint8_t index, void* buffer, uint16_t len,
                                   tuh_xfer_cb_t complete_cb, uintptr_t user_data);

// Get string descriptor (control transfer)
// true on success, false if there is on-going control transfer or incorrect parameters
// Blocking if complete callback is NULL, in this case 'user_data' must contain xfer_result_t variable
bool tuh_descriptor_get_string(uint8_t daddr, uint8_t index, uint16_t language_id, void* buffer, uint16_t len,
                               tuh_xfer_cb_t complete_cb, uintptr_t user_data);

// Get manufacturer string descriptor (control transfer)
// true on success, false if there is on-going control transfer or incorrect parameters
bool tuh_descriptor_get_manufacturer_string(uint8_t daddr, uint16_t language_id, void* buffer, uint16_t len,
                                            tuh_xfer_cb_t complete_cb, uintptr_t user_data);

// Get product string descriptor (control transfer)
// true on success, false if there is on-going control transfer or incorrect parameters
bool tuh_descriptor_get_product_string(uint8_t daddr, uint16_t language_id, void* buffer, uint16_t len,
                                       tuh_xfer_cb_t complete_cb, uintptr_t user_data);

// Get serial string descriptor (control transfer)
// true on success, false if there is on-going control transfer or incorrect parameters
bool tuh_descriptor_get_serial_string(uint8_t daddr, uint16_t language_id, void* buffer, uint16_t len,
                                      tuh_xfer_cb_t complete_cb, uintptr_t user_data);

//--------------------------------------------------------------------+
// Descriptors Synchronous (blocking)
//--------------------------------------------------------------------+

// Sync (blocking) version of tuh_descriptor_get()
// return transfer result
uint8_t tuh_descriptor_get_sync(uint8_t daddr, uint8_t type, uint8_t index, void* buffer, uint16_t len);

// Sync (blocking) version of tuh_descriptor_get_device()
// return transfer result
uint8_t tuh_descriptor_get_device_sync(uint8_t daddr, void* buffer, uint16_t len);

// Sync (blocking) version of tuh_descriptor_get_configuration()
// return transfer result
uint8_t tuh_descriptor_get_configuration_sync(uint8_t daddr, uint8_t index, void* buffer, uint16_t len);

// Sync (blocking) version of tuh_descriptor_get_hid_report()
// return transfer result
uint8_t tuh_descriptor_get_hid_report_sync(uint8_t daddr, uint8_t itf_num, uint8_t desc_type, uint8_t index, void* buffer, uint16_t len);

// Sync (blocking) version of tuh_descriptor_get_string()
// return transfer result
uint8_t tuh_descriptor_get_string_sync(uint8_t daddr, uint8_t index, uint16_t language_id, void* buffer, uint16_t len);

// Sync (blocking) version of tuh_descriptor_get_manufacturer_string()
// return transfer result
uint8_t tuh_descriptor_get_manufacturer_string_sync(uint8_t daddr, uint16_t language_id, void* buffer, uint16_t len);

// Sync (blocking) version of tuh_descriptor_get_product_string()
// return transfer result
uint8_t tuh_descriptor_get_product_string_sync(uint8_t daddr, uint16_t language_id, void* buffer, uint16_t len);

// Sync (blocking) version of tuh_descriptor_get_serial_string()
// return transfer result
uint8_t tuh_descriptor_get_serial_string_sync(uint8_t daddr, uint16_t language_id, void* buffer, uint16_t len);

#ifdef __cplusplus
 }
#endif

#endif
