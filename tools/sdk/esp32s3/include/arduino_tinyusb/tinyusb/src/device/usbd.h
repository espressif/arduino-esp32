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

#ifndef _TUSB_USBD_H_
#define _TUSB_USBD_H_

#include "common/tusb_common.h"

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------+
// Application API
//--------------------------------------------------------------------+

// Init device stack on roothub port
bool tud_init (uint8_t rhport);

// Deinit device stack on roothub port
bool tud_deinit(uint8_t rhport);

// Check if device stack is already initialized
bool tud_inited(void);

// Task function should be called in main/rtos loop, extended version of tud_task()
// - timeout_ms: millisecond to wait, zero = no wait, 0xFFFFFFFF = wait forever
// - in_isr: if function is called in ISR
void tud_task_ext(uint32_t timeout_ms, bool in_isr);

// Task function should be called in main/rtos loop
TU_ATTR_ALWAYS_INLINE static inline
void tud_task (void) {
  tud_task_ext(UINT32_MAX, false);
}

// Check if there is pending events need processing by tud_task()
bool tud_task_event_ready(void);

#ifndef TUSB_DCD_H_
extern void dcd_int_handler(uint8_t rhport);
#endif

// Interrupt handler, name alias to DCD
#define tud_int_handler   dcd_int_handler

// Get current bus speed
tusb_speed_t tud_speed_get(void);

// Check if device is connected (may not mounted/configured yet)
// True if just got out of Bus Reset and received the very first data from host
bool tud_connected(void);

// Check if device is connected and configured
bool tud_mounted(void);

// Check if device is suspended
bool tud_suspended(void);

// Check if device is ready to transfer
TU_ATTR_ALWAYS_INLINE static inline
bool tud_ready(void) {
  return tud_mounted() && !tud_suspended();
}

// Remote wake up host, only if suspended and enabled by host
bool tud_remote_wakeup(void);

// Enable pull-up resistor on D+ D-
// Return false on unsupported MCUs
bool tud_disconnect(void);

// Disable pull-up resistor on D+ D-
// Return false on unsupported MCUs
bool tud_connect(void);

// Enable or disable the Start Of Frame callback support
void tud_sof_cb_enable(bool en);

// Carry out Data and Status stage of control transfer
// - If len = 0, it is equivalent to sending status only
// - If len > wLength : it will be truncated
bool tud_control_xfer(uint8_t rhport, tusb_control_request_t const * request, void* buffer, uint16_t len);

// Send STATUS (zero length) packet
bool tud_control_status(uint8_t rhport, tusb_control_request_t const * request);

//--------------------------------------------------------------------+
// Application Callbacks (WEAK is optional)
//--------------------------------------------------------------------+

// Invoked when received GET DEVICE DESCRIPTOR request
// Application return pointer to descriptor
uint8_t const * tud_descriptor_device_cb(void);

// Invoked when received GET CONFIGURATION DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint8_t const * tud_descriptor_configuration_cb(uint8_t index);

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid);

// Invoked when received GET BOS DESCRIPTOR request
// Application return pointer to descriptor
TU_ATTR_WEAK uint8_t const * tud_descriptor_bos_cb(void);

// Invoked when received GET DEVICE QUALIFIER DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete.
// device_qualifier descriptor describes information about a high-speed capable device that would
// change if the device were operating at the other speed. If not highspeed capable stall this request.
TU_ATTR_WEAK uint8_t const* tud_descriptor_device_qualifier_cb(void);

// Invoked when received GET OTHER SEED CONFIGURATION DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
// Configuration descriptor in the other speed e.g if high speed then this is for full speed and vice versa
TU_ATTR_WEAK uint8_t const* tud_descriptor_other_speed_configuration_cb(uint8_t index);

// Invoked when device is mounted (configured)
TU_ATTR_WEAK void tud_mount_cb(void);

// Invoked when device is unmounted
TU_ATTR_WEAK void tud_umount_cb(void);

// Invoked when usb bus is suspended
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
TU_ATTR_WEAK void tud_suspend_cb(bool remote_wakeup_en);

// Invoked when usb bus is resumed
TU_ATTR_WEAK void tud_resume_cb(void);

// Invoked when there is a new usb event, which need to be processed by tud_task()/tud_task_ext()
void tud_event_hook_cb(uint8_t rhport, uint32_t eventid, bool in_isr);

// Invoked when a new (micro) frame started
void tud_sof_cb(uint32_t frame_count);

// Invoked when received control request with VENDOR TYPE
TU_ATTR_WEAK bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request);

//--------------------------------------------------------------------+
// Binary Device Object Store (BOS) Descriptor Templates
//--------------------------------------------------------------------+

#define TUD_BOS_DESC_LEN      5

// total length, number of device caps
#define TUD_BOS_DESCRIPTOR(_total_len, _caps_num) \
  5, TUSB_DESC_BOS, U16_TO_U8S_LE(_total_len), _caps_num

// Device Capability Platform 128-bit UUID + Data
#define TUD_BOS_PLATFORM_DESCRIPTOR(...) \
  4+TU_ARGS_NUM(__VA_ARGS__), TUSB_DESC_DEVICE_CAPABILITY, DEVICE_CAPABILITY_PLATFORM, 0x00, __VA_ARGS__

//------------- WebUSB BOS Platform -------------//

// Descriptor Length
#define TUD_BOS_WEBUSB_DESC_LEN         24

// Vendor Code, iLandingPage
#define TUD_BOS_WEBUSB_DESCRIPTOR(_vendor_code, _ipage) \
  TUD_BOS_PLATFORM_DESCRIPTOR(TUD_BOS_WEBUSB_UUID, U16_TO_U8S_LE(0x0100), _vendor_code, _ipage)

#define TUD_BOS_WEBUSB_UUID   \
  0x38, 0xB6, 0x08, 0x34, 0xA9, 0x09, 0xA0, 0x47, \
  0x8B, 0xFD, 0xA0, 0x76, 0x88, 0x15, 0xB6, 0x65

//------------- Microsoft OS 2.0 Platform -------------//
#define TUD_BOS_MICROSOFT_OS_DESC_LEN   28

// Total Length of descriptor set, vendor code
#define TUD_BOS_MS_OS_20_DESCRIPTOR(_desc_set_len, _vendor_code) \
  TUD_BOS_PLATFORM_DESCRIPTOR(TUD_BOS_MS_OS_20_UUID, U32_TO_U8S_LE(0x06030000), U16_TO_U8S_LE(_desc_set_len), _vendor_code, 0)

#define TUD_BOS_MS_OS_20_UUID \
    0xDF, 0x60, 0xDD, 0xD8, 0x89, 0x45, 0xC7, 0x4C, \
  0x9C, 0xD2, 0x65, 0x9D, 0x9E, 0x64, 0x8A, 0x9F

//--------------------------------------------------------------------+
// Configuration Descriptor Templates
//--------------------------------------------------------------------+

#define TUD_CONFIG_DESC_LEN   (9)

// Config number, interface count, string index, total length, attribute, power in mA
#define TUD_CONFIG_DESCRIPTOR(config_num, _itfcount, _stridx, _total_len, _attribute, _power_ma) \
  9, TUSB_DESC_CONFIGURATION, U16_TO_U8S_LE(_total_len), _itfcount, config_num, _stridx, TU_BIT(7) | _attribute, (_power_ma)/2

//--------------------------------------------------------------------+
// CDC Descriptor Templates
//--------------------------------------------------------------------+

// Length of template descriptor: 66 bytes
#define TUD_CDC_DESC_LEN  (8+9+5+5+4+5+7+9+7+7)

// CDC Descriptor Template
// Interface number, string index, EP notification address and size, EP data address (out, in) and size.
#define TUD_CDC_DESCRIPTOR(_itfnum, _stridx, _ep_notif, _ep_notif_size, _epout, _epin, _epsize) \
  /* Interface Associate */\
  8, TUSB_DESC_INTERFACE_ASSOCIATION, _itfnum, 2, TUSB_CLASS_CDC, CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL, CDC_COMM_PROTOCOL_NONE, 0,\
  /* CDC Control Interface */\
  9, TUSB_DESC_INTERFACE, _itfnum, 0, 1, TUSB_CLASS_CDC, CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL, CDC_COMM_PROTOCOL_NONE, _stridx,\
  /* CDC Header */\
  5, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_HEADER, U16_TO_U8S_LE(0x0120),\
  /* CDC Call */\
  5, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_CALL_MANAGEMENT, 0, (uint8_t)((_itfnum) + 1),\
  /* CDC ACM: support line request + send break */\
  4, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_ABSTRACT_CONTROL_MANAGEMENT, 6,\
  /* CDC Union */\
  5, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_UNION, _itfnum, (uint8_t)((_itfnum) + 1),\
  /* Endpoint Notification */\
  7, TUSB_DESC_ENDPOINT, _ep_notif, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(_ep_notif_size), 16,\
  /* CDC Data Interface */\
  9, TUSB_DESC_INTERFACE, (uint8_t)((_itfnum)+1), 0, 2, TUSB_CLASS_CDC_DATA, 0, 0, 0,\
  /* Endpoint Out */\
  7, TUSB_DESC_ENDPOINT, _epout, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0,\
  /* Endpoint In */\
  7, TUSB_DESC_ENDPOINT, _epin, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0

//--------------------------------------------------------------------+
// MSC Descriptor Templates
//--------------------------------------------------------------------+

// Length of template descriptor: 23 bytes
#define TUD_MSC_DESC_LEN    (9 + 7 + 7)

// Interface number, string index, EP Out & EP In address, EP size
#define TUD_MSC_DESCRIPTOR(_itfnum, _stridx, _epout, _epin, _epsize) \
  /* Interface */\
  9, TUSB_DESC_INTERFACE, _itfnum, 0, 2, TUSB_CLASS_MSC, MSC_SUBCLASS_SCSI, MSC_PROTOCOL_BOT, _stridx,\
  /* Endpoint Out */\
  7, TUSB_DESC_ENDPOINT, _epout, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0,\
  /* Endpoint In */\
  7, TUSB_DESC_ENDPOINT, _epin, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0


//--------------------------------------------------------------------+
// HID Descriptor Templates
//--------------------------------------------------------------------+

// Length of template descriptor: 25 bytes
#define TUD_HID_DESC_LEN    (9 + 9 + 7)

// HID Input only descriptor
// Interface number, string index, protocol, report descriptor len, EP In address, size & polling interval
#define TUD_HID_DESCRIPTOR(_itfnum, _stridx, _boot_protocol, _report_desc_len, _epin, _epsize, _ep_interval) \
  /* Interface */\
  9, TUSB_DESC_INTERFACE, _itfnum, 0, 1, TUSB_CLASS_HID, (uint8_t)((_boot_protocol) ? (uint8_t)HID_SUBCLASS_BOOT : 0), _boot_protocol, _stridx,\
  /* HID descriptor */\
  9, HID_DESC_TYPE_HID, U16_TO_U8S_LE(0x0111), 0, 1, HID_DESC_TYPE_REPORT, U16_TO_U8S_LE(_report_desc_len),\
  /* Endpoint In */\
  7, TUSB_DESC_ENDPOINT, _epin, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(_epsize), _ep_interval

// Length of template descriptor: 32 bytes
#define TUD_HID_INOUT_DESC_LEN    (9 + 9 + 7 + 7)

// HID Input & Output descriptor
// Interface number, string index, protocol, report descriptor len, EP OUT & IN address, size & polling interval
#define TUD_HID_INOUT_DESCRIPTOR(_itfnum, _stridx, _boot_protocol, _report_desc_len, _epout, _epin, _epsize, _ep_interval) \
  /* Interface */\
  9, TUSB_DESC_INTERFACE, _itfnum, 0, 2, TUSB_CLASS_HID, (uint8_t)((_boot_protocol) ? (uint8_t)HID_SUBCLASS_BOOT : 0), _boot_protocol, _stridx,\
  /* HID descriptor */\
  9, HID_DESC_TYPE_HID, U16_TO_U8S_LE(0x0111), 0, 1, HID_DESC_TYPE_REPORT, U16_TO_U8S_LE(_report_desc_len),\
  /* Endpoint Out */\
  7, TUSB_DESC_ENDPOINT, _epout, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(_epsize), _ep_interval, \
  /* Endpoint In */\
  7, TUSB_DESC_ENDPOINT, _epin, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(_epsize), _ep_interval

//--------------------------------------------------------------------+
// MIDI Descriptor Templates
// Note: MIDI v1.0 is based on Audio v1.0
//--------------------------------------------------------------------+

#define TUD_MIDI_DESC_HEAD_LEN (9 + 9 + 9 + 7)
#define TUD_MIDI_DESC_HEAD(_itfnum,  _stridx, _numcables) \
  /* Audio Control (AC) Interface */\
  9, TUSB_DESC_INTERFACE, _itfnum, 0, 0, TUSB_CLASS_AUDIO, AUDIO_SUBCLASS_CONTROL, AUDIO_FUNC_PROTOCOL_CODE_UNDEF, _stridx,\
  /* AC Header */\
  9, TUSB_DESC_CS_INTERFACE, AUDIO_CS_AC_INTERFACE_HEADER, U16_TO_U8S_LE(0x0100), U16_TO_U8S_LE(0x0009), 1, (uint8_t)((_itfnum) + 1),\
  /* MIDI Streaming (MS) Interface */\
  9, TUSB_DESC_INTERFACE, (uint8_t)((_itfnum) + 1), 0, 2, TUSB_CLASS_AUDIO, AUDIO_SUBCLASS_MIDI_STREAMING, AUDIO_FUNC_PROTOCOL_CODE_UNDEF, 0,\
  /* MS Header */\
  7, TUSB_DESC_CS_INTERFACE, MIDI_CS_INTERFACE_HEADER, U16_TO_U8S_LE(0x0100), U16_TO_U8S_LE(7 + (_numcables) * TUD_MIDI_DESC_JACK_LEN + 2 * TUD_MIDI_DESC_EP_LEN(_numcables))

#define TUD_MIDI_JACKID_IN_EMB(_cablenum) \
  (uint8_t)(((_cablenum) - 1) * 4 + 1)

#define TUD_MIDI_JACKID_IN_EXT(_cablenum) \
  (uint8_t)(((_cablenum) - 1) * 4 + 2)

#define TUD_MIDI_JACKID_OUT_EMB(_cablenum) \
  (uint8_t)(((_cablenum) - 1) * 4 + 3)

#define TUD_MIDI_JACKID_OUT_EXT(_cablenum) \
  (uint8_t)(((_cablenum) - 1) * 4 + 4)

#define TUD_MIDI_DESC_JACK_LEN (6 + 6 + 9 + 9)
#define TUD_MIDI_DESC_JACK_DESC(_cablenum, _stridx) \
  /* MS In Jack (Embedded) */\
  6, TUSB_DESC_CS_INTERFACE, MIDI_CS_INTERFACE_IN_JACK, MIDI_JACK_EMBEDDED, TUD_MIDI_JACKID_IN_EMB(_cablenum), _stridx,\
  /* MS In Jack (External) */\
  6, TUSB_DESC_CS_INTERFACE, MIDI_CS_INTERFACE_IN_JACK, MIDI_JACK_EXTERNAL, TUD_MIDI_JACKID_IN_EXT(_cablenum), _stridx,\
  /* MS Out Jack (Embedded), connected to In Jack External */\
  9, TUSB_DESC_CS_INTERFACE, MIDI_CS_INTERFACE_OUT_JACK, MIDI_JACK_EMBEDDED, TUD_MIDI_JACKID_OUT_EMB(_cablenum), 1, TUD_MIDI_JACKID_IN_EXT(_cablenum), 1, _stridx,\
  /* MS Out Jack (External), connected to In Jack Embedded */\
  9, TUSB_DESC_CS_INTERFACE, MIDI_CS_INTERFACE_OUT_JACK, MIDI_JACK_EXTERNAL, TUD_MIDI_JACKID_OUT_EXT(_cablenum), 1, TUD_MIDI_JACKID_IN_EMB(_cablenum), 1, _stridx

#define TUD_MIDI_DESC_JACK(_cablenum) TUD_MIDI_DESC_JACK_DESC(_cablenum, 0)

#define TUD_MIDI_DESC_EP_LEN(_numcables) (9 + 4 + (_numcables))
#define TUD_MIDI_DESC_EP(_epout, _epsize, _numcables) \
  /* Endpoint: Note Audio v1.0's endpoint has 9 bytes instead of 7 */\
  9, TUSB_DESC_ENDPOINT, _epout, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0, 0, 0, \
  /* MS Endpoint (connected to embedded jack) */\
  (uint8_t)(4 + (_numcables)), TUSB_DESC_CS_ENDPOINT, MIDI_CS_ENDPOINT_GENERAL, _numcables

// Length of template descriptor (88 bytes)
#define TUD_MIDI_DESC_LEN (TUD_MIDI_DESC_HEAD_LEN + TUD_MIDI_DESC_JACK_LEN + TUD_MIDI_DESC_EP_LEN(1) * 2)

// MIDI simple descriptor
// - 1 Embedded Jack In connected to 1 External Jack Out
// - 1 Embedded Jack out connected to 1 External Jack In
#define TUD_MIDI_DESCRIPTOR(_itfnum, _stridx, _epout, _epin, _epsize) \
  TUD_MIDI_DESC_HEAD(_itfnum, _stridx, 1),\
  TUD_MIDI_DESC_JACK_DESC(1, 0),\
  TUD_MIDI_DESC_EP(_epout, _epsize, 1),\
  TUD_MIDI_JACKID_IN_EMB(1),\
  TUD_MIDI_DESC_EP(_epin, _epsize, 1),\
  TUD_MIDI_JACKID_OUT_EMB(1)

//--------------------------------------------------------------------+
// Audio v2.0 Descriptor Templates
//--------------------------------------------------------------------+

/* Standard Interface Association Descriptor (IAD) */
#define TUD_AUDIO_DESC_IAD_LEN 8
#define TUD_AUDIO_DESC_IAD(_firstitf, _nitfs, _stridx) \
  TUD_AUDIO_DESC_IAD_LEN, TUSB_DESC_INTERFACE_ASSOCIATION, _firstitf, _nitfs, TUSB_CLASS_AUDIO, AUDIO_FUNCTION_SUBCLASS_UNDEFINED, AUDIO_FUNC_PROTOCOL_CODE_V2, _stridx

/* Standard AC Interface Descriptor(4.7.1) */
#define TUD_AUDIO_DESC_STD_AC_LEN 9
#define TUD_AUDIO_DESC_STD_AC(_itfnum, _nEPs, _stridx) /* _nEPs is 0 or 1 */\
  TUD_AUDIO_DESC_STD_AC_LEN, TUSB_DESC_INTERFACE, _itfnum, /* fixed to zero */ 0x00, _nEPs, TUSB_CLASS_AUDIO, AUDIO_SUBCLASS_CONTROL, AUDIO_INT_PROTOCOL_CODE_V2, _stridx

/* Class-Specific AC Interface Header Descriptor(4.7.2) */
#define TUD_AUDIO_DESC_CS_AC_LEN 9
#define TUD_AUDIO_DESC_CS_AC(_bcdADC, _category, _totallen, _ctrl) /* _bcdADC : Audio Device Class Specification Release Number in Binary-Coded Decimal, _category : see audio_function_t, _totallen : Total number of bytes returned for the class-specific AudioControl interface i.e. Clock Source, Unit and Terminal descriptors - Do not include TUD_AUDIO_DESC_CS_AC_LEN, we already do this here*/ \
  TUD_AUDIO_DESC_CS_AC_LEN, TUSB_DESC_CS_INTERFACE, AUDIO_CS_AC_INTERFACE_HEADER, U16_TO_U8S_LE(_bcdADC), _category, U16_TO_U8S_LE(_totallen + TUD_AUDIO_DESC_CS_AC_LEN), _ctrl

/* Clock Source Descriptor(4.7.2.1) */
#define TUD_AUDIO_DESC_CLK_SRC_LEN 8
#define TUD_AUDIO_DESC_CLK_SRC(_clkid, _attr, _ctrl, _assocTerm, _stridx) \
  TUD_AUDIO_DESC_CLK_SRC_LEN, TUSB_DESC_CS_INTERFACE, AUDIO_CS_AC_INTERFACE_CLOCK_SOURCE, _clkid, _attr, _ctrl, _assocTerm, _stridx

/* Input Terminal Descriptor(4.7.2.4) */
#define TUD_AUDIO_DESC_INPUT_TERM_LEN 17
#define TUD_AUDIO_DESC_INPUT_TERM(_termid, _termtype, _assocTerm, _clkid, _nchannelslogical, _channelcfg, _idxchannelnames, _ctrl, _stridx) \
  TUD_AUDIO_DESC_INPUT_TERM_LEN, TUSB_DESC_CS_INTERFACE, AUDIO_CS_AC_INTERFACE_INPUT_TERMINAL, _termid, U16_TO_U8S_LE(_termtype), _assocTerm, _clkid, _nchannelslogical, U32_TO_U8S_LE(_channelcfg), _idxchannelnames, U16_TO_U8S_LE(_ctrl), _stridx

/* Output Terminal Descriptor(4.7.2.5) */
#define TUD_AUDIO_DESC_OUTPUT_TERM_LEN 12
#define TUD_AUDIO_DESC_OUTPUT_TERM(_termid, _termtype, _assocTerm, _srcid, _clkid, _ctrl, _stridx) \
  TUD_AUDIO_DESC_OUTPUT_TERM_LEN, TUSB_DESC_CS_INTERFACE, AUDIO_CS_AC_INTERFACE_OUTPUT_TERMINAL, _termid, U16_TO_U8S_LE(_termtype), _assocTerm, _srcid, _clkid, U16_TO_U8S_LE(_ctrl), _stridx

/* Feature Unit Descriptor(4.7.2.8) */
// 1 - Channel
#define TUD_AUDIO_DESC_FEATURE_UNIT_ONE_CHANNEL_LEN 6+(1+1)*4
#define TUD_AUDIO_DESC_FEATURE_UNIT_ONE_CHANNEL(_unitid, _srcid, _ctrlch0master, _ctrlch1, _stridx) \
  TUD_AUDIO_DESC_FEATURE_UNIT_ONE_CHANNEL_LEN, TUSB_DESC_CS_INTERFACE, AUDIO_CS_AC_INTERFACE_FEATURE_UNIT, _unitid, _srcid, U32_TO_U8S_LE(_ctrlch0master), U32_TO_U8S_LE(_ctrlch1), _stridx

// 2 - Channels
#define TUD_AUDIO_DESC_FEATURE_UNIT_TWO_CHANNEL_LEN (6+(2+1)*4)
#define TUD_AUDIO_DESC_FEATURE_UNIT_TWO_CHANNEL(_unitid, _srcid, _ctrlch0master, _ctrlch1, _ctrlch2, _stridx) \
		TUD_AUDIO_DESC_FEATURE_UNIT_TWO_CHANNEL_LEN, TUSB_DESC_CS_INTERFACE, AUDIO_CS_AC_INTERFACE_FEATURE_UNIT, _unitid, _srcid, U32_TO_U8S_LE(_ctrlch0master), U32_TO_U8S_LE(_ctrlch1), U32_TO_U8S_LE(_ctrlch2), _stridx
// 4 - Channels
#define TUD_AUDIO_DESC_FEATURE_UNIT_FOUR_CHANNEL_LEN (6+(4+1)*4)
#define TUD_AUDIO_DESC_FEATURE_UNIT_FOUR_CHANNEL(_unitid, _srcid, _ctrlch0master, _ctrlch1, _ctrlch2, _ctrlch3, _ctrlch4, _stridx) \
                    TUD_AUDIO_DESC_FEATURE_UNIT_FOUR_CHANNEL_LEN, TUSB_DESC_CS_INTERFACE, AUDIO_CS_AC_INTERFACE_FEATURE_UNIT, _unitid, _srcid, U32_TO_U8S_LE(_ctrlch0master), U32_TO_U8S_LE(_ctrlch1), U32_TO_U8S_LE(_ctrlch2), U32_TO_U8S_LE(_ctrlch3), U32_TO_U8S_LE(_ctrlch4), _stridx

// For more channels, add definitions here

/* Standard AC Interrupt Endpoint Descriptor(4.8.2.1) */
#define TUD_AUDIO_DESC_STD_AC_INT_EP_LEN 7
#define TUD_AUDIO_DESC_STD_AC_INT_EP(_ep, _interval) \
  TUD_AUDIO_DESC_STD_AC_INT_EP_LEN, TUSB_DESC_ENDPOINT, _ep, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(6), _interval

/* Standard AS Interface Descriptor(4.9.1) */
#define TUD_AUDIO_DESC_STD_AS_INT_LEN 9
#define TUD_AUDIO_DESC_STD_AS_INT(_itfnum, _altset, _nEPs, _stridx) \
  TUD_AUDIO_DESC_STD_AS_INT_LEN, TUSB_DESC_INTERFACE, _itfnum, _altset, _nEPs, TUSB_CLASS_AUDIO, AUDIO_SUBCLASS_STREAMING, AUDIO_INT_PROTOCOL_CODE_V2, _stridx

/* Class-Specific AS Interface Descriptor(4.9.2) */
#define TUD_AUDIO_DESC_CS_AS_INT_LEN 16
#define TUD_AUDIO_DESC_CS_AS_INT(_termid, _ctrl, _formattype, _formats, _nchannelsphysical, _channelcfg, _stridx) \
  TUD_AUDIO_DESC_CS_AS_INT_LEN, TUSB_DESC_CS_INTERFACE, AUDIO_CS_AS_INTERFACE_AS_GENERAL, _termid, _ctrl, _formattype, U32_TO_U8S_LE(_formats), _nchannelsphysical, U32_TO_U8S_LE(_channelcfg), _stridx

/* Type I Format Type Descriptor(2.3.1.6 - Audio Formats) */
#define TUD_AUDIO_DESC_TYPE_I_FORMAT_LEN 6
#define TUD_AUDIO_DESC_TYPE_I_FORMAT(_subslotsize, _bitresolution) /* _subslotsize is number of bytes per sample (i.e. subslot) and can be 1,2,3, or 4 */\
  TUD_AUDIO_DESC_TYPE_I_FORMAT_LEN, TUSB_DESC_CS_INTERFACE, AUDIO_CS_AS_INTERFACE_FORMAT_TYPE, AUDIO_FORMAT_TYPE_I, _subslotsize, _bitresolution

/* Standard AS Isochronous Audio Data Endpoint Descriptor(4.10.1.1) */
#define TUD_AUDIO_DESC_STD_AS_ISO_EP_LEN 7
#define TUD_AUDIO_DESC_STD_AS_ISO_EP(_ep, _attr, _maxEPsize, _interval) \
  TUD_AUDIO_DESC_STD_AS_ISO_EP_LEN, TUSB_DESC_ENDPOINT, _ep, _attr, U16_TO_U8S_LE(_maxEPsize), _interval

/* Class-Specific AS Isochronous Audio Data Endpoint Descriptor(4.10.1.2) */
#define TUD_AUDIO_DESC_CS_AS_ISO_EP_LEN 8
#define TUD_AUDIO_DESC_CS_AS_ISO_EP(_attr, _ctrl, _lockdelayunit, _lockdelay) \
  TUD_AUDIO_DESC_CS_AS_ISO_EP_LEN, TUSB_DESC_CS_ENDPOINT, AUDIO_CS_EP_SUBTYPE_GENERAL, _attr, _ctrl, _lockdelayunit, U16_TO_U8S_LE(_lockdelay)

/* Standard AS Isochronous Feedback Endpoint Descriptor(4.10.2.1) */
#define TUD_AUDIO_DESC_STD_AS_ISO_FB_EP_LEN 7
#define TUD_AUDIO_DESC_STD_AS_ISO_FB_EP(_ep, _interval) \
  TUD_AUDIO_DESC_STD_AS_ISO_FB_EP_LEN, TUSB_DESC_ENDPOINT, _ep, (uint8_t) ((uint8_t)TUSB_XFER_ISOCHRONOUS | (uint8_t)TUSB_ISO_EP_ATT_NO_SYNC | (uint8_t)TUSB_ISO_EP_ATT_EXPLICIT_FB), U16_TO_U8S_LE(4), _interval

// AUDIO simple descriptor (UAC2) for 1 microphone input
// - 1 Input Terminal, 1 Feature Unit (Mute and Volume Control), 1 Output Terminal, 1 Clock Source

#define TUD_AUDIO_MIC_ONE_CH_DESC_LEN (TUD_AUDIO_DESC_IAD_LEN\
  + TUD_AUDIO_DESC_STD_AC_LEN\
  + TUD_AUDIO_DESC_CS_AC_LEN\
  + TUD_AUDIO_DESC_CLK_SRC_LEN\
  + TUD_AUDIO_DESC_INPUT_TERM_LEN\
  + TUD_AUDIO_DESC_OUTPUT_TERM_LEN\
  + TUD_AUDIO_DESC_FEATURE_UNIT_ONE_CHANNEL_LEN\
  + TUD_AUDIO_DESC_STD_AS_INT_LEN\
  + TUD_AUDIO_DESC_STD_AS_INT_LEN\
  + TUD_AUDIO_DESC_CS_AS_INT_LEN\
  + TUD_AUDIO_DESC_TYPE_I_FORMAT_LEN\
  + TUD_AUDIO_DESC_STD_AS_ISO_EP_LEN\
  + TUD_AUDIO_DESC_CS_AS_ISO_EP_LEN)

#define TUD_AUDIO_MIC_ONE_CH_DESC_N_AS_INT 1 	// Number of AS interfaces

#define TUD_AUDIO_MIC_ONE_CH_DESCRIPTOR(_itfnum, _stridx, _nBytesPerSample, _nBitsUsedPerSample, _epin, _epsize) \
  /* Standard Interface Association Descriptor (IAD) */\
  TUD_AUDIO_DESC_IAD(/*_firstitf*/ _itfnum, /*_nitfs*/ 0x02, /*_stridx*/ 0x00),\
  /* Standard AC Interface Descriptor(4.7.1) */\
  TUD_AUDIO_DESC_STD_AC(/*_itfnum*/ _itfnum, /*_nEPs*/ 0x00, /*_stridx*/ _stridx),\
  /* Class-Specific AC Interface Header Descriptor(4.7.2) */\
  TUD_AUDIO_DESC_CS_AC(/*_bcdADC*/ 0x0200, /*_category*/ AUDIO_FUNC_MICROPHONE, /*_totallen*/ TUD_AUDIO_DESC_CLK_SRC_LEN+TUD_AUDIO_DESC_INPUT_TERM_LEN+TUD_AUDIO_DESC_OUTPUT_TERM_LEN+TUD_AUDIO_DESC_FEATURE_UNIT_ONE_CHANNEL_LEN, /*_ctrl*/ AUDIO_CS_AS_INTERFACE_CTRL_LATENCY_POS),\
  /* Clock Source Descriptor(4.7.2.1) */\
  TUD_AUDIO_DESC_CLK_SRC(/*_clkid*/ 0x04, /*_attr*/ AUDIO_CLOCK_SOURCE_ATT_INT_FIX_CLK, /*_ctrl*/ (AUDIO_CTRL_R << AUDIO_CLOCK_SOURCE_CTRL_CLK_FRQ_POS), /*_assocTerm*/ 0x01,  /*_stridx*/ 0x00),\
  /* Input Terminal Descriptor(4.7.2.4) */\
  TUD_AUDIO_DESC_INPUT_TERM(/*_termid*/ 0x01, /*_termtype*/ AUDIO_TERM_TYPE_IN_GENERIC_MIC, /*_assocTerm*/ 0x03, /*_clkid*/ 0x04, /*_nchannelslogical*/ 0x01, /*_channelcfg*/ AUDIO_CHANNEL_CONFIG_NON_PREDEFINED, /*_idxchannelnames*/ 0x00, /*_ctrl*/ AUDIO_CTRL_R << AUDIO_IN_TERM_CTRL_CONNECTOR_POS, /*_stridx*/ 0x00),\
  /* Output Terminal Descriptor(4.7.2.5) */\
  TUD_AUDIO_DESC_OUTPUT_TERM(/*_termid*/ 0x03, /*_termtype*/ AUDIO_TERM_TYPE_USB_STREAMING, /*_assocTerm*/ 0x01, /*_srcid*/ 0x02, /*_clkid*/ 0x04, /*_ctrl*/ 0x0000, /*_stridx*/ 0x00),\
  /* Feature Unit Descriptor(4.7.2.8) */\
  TUD_AUDIO_DESC_FEATURE_UNIT_ONE_CHANNEL(/*_unitid*/ 0x02, /*_srcid*/ 0x01, /*_ctrlch0master*/ AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_VOLUME_POS, /*_ctrlch1*/ AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_VOLUME_POS, /*_stridx*/ 0x00),\
  /* Standard AS Interface Descriptor(4.9.1) */\
  /* Interface 1, Alternate 0 - default alternate setting with 0 bandwidth */\
  TUD_AUDIO_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)((_itfnum)+1), /*_altset*/ 0x00, /*_nEPs*/ 0x00, /*_stridx*/ 0x00),\
  /* Standard AS Interface Descriptor(4.9.1) */\
  /* Interface 1, Alternate 1 - alternate interface for data streaming */\
  TUD_AUDIO_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)((_itfnum)+1), /*_altset*/ 0x01, /*_nEPs*/ 0x01, /*_stridx*/ 0x00),\
  /* Class-Specific AS Interface Descriptor(4.9.2) */\
  TUD_AUDIO_DESC_CS_AS_INT(/*_termid*/ 0x03, /*_ctrl*/ AUDIO_CTRL_NONE, /*_formattype*/ AUDIO_FORMAT_TYPE_I, /*_formats*/ AUDIO_DATA_FORMAT_TYPE_I_PCM, /*_nchannelsphysical*/ 0x01, /*_channelcfg*/ AUDIO_CHANNEL_CONFIG_NON_PREDEFINED, /*_stridx*/ 0x00),\
  /* Type I Format Type Descriptor(2.3.1.6 - Audio Formats) */\
  TUD_AUDIO_DESC_TYPE_I_FORMAT(_nBytesPerSample, _nBitsUsedPerSample),\
  /* Standard AS Isochronous Audio Data Endpoint Descriptor(4.10.1.1) */\
  TUD_AUDIO_DESC_STD_AS_ISO_EP(/*_ep*/ _epin, /*_attr*/ (uint8_t) ((uint8_t)TUSB_XFER_ISOCHRONOUS | (uint8_t)TUSB_ISO_EP_ATT_ASYNCHRONOUS | (uint8_t)TUSB_ISO_EP_ATT_DATA), /*_maxEPsize*/ _epsize, /*_interval*/ 0x01),\
  /* Class-Specific AS Isochronous Audio Data Endpoint Descriptor(4.10.1.2) */\
  TUD_AUDIO_DESC_CS_AS_ISO_EP(/*_attr*/ AUDIO_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK, /*_ctrl*/ AUDIO_CTRL_NONE, /*_lockdelayunit*/ AUDIO_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_UNDEFINED, /*_lockdelay*/ 0x0000)

// AUDIO simple descriptor (UAC2) for 4 microphone input
// - 1 Input Terminal, 1 Feature Unit (Mute and Volume Control), 1 Output Terminal, 1 Clock Source

#define TUD_AUDIO_MIC_FOUR_CH_DESC_LEN (TUD_AUDIO_DESC_IAD_LEN\
  + TUD_AUDIO_DESC_STD_AC_LEN\
  + TUD_AUDIO_DESC_CS_AC_LEN\
  + TUD_AUDIO_DESC_CLK_SRC_LEN\
  + TUD_AUDIO_DESC_INPUT_TERM_LEN\
  + TUD_AUDIO_DESC_OUTPUT_TERM_LEN\
  + TUD_AUDIO_DESC_FEATURE_UNIT_FOUR_CHANNEL_LEN\
  + TUD_AUDIO_DESC_STD_AS_INT_LEN\
  + TUD_AUDIO_DESC_STD_AS_INT_LEN\
  + TUD_AUDIO_DESC_CS_AS_INT_LEN\
  + TUD_AUDIO_DESC_TYPE_I_FORMAT_LEN\
  + TUD_AUDIO_DESC_STD_AS_ISO_EP_LEN\
  + TUD_AUDIO_DESC_CS_AS_ISO_EP_LEN)

#define TUD_AUDIO_MIC_FOUR_CH_DESC_N_AS_INT 1   // Number of AS interfaces

#define TUD_AUDIO_MIC_FOUR_CH_DESCRIPTOR(_itfnum, _stridx, _nBytesPerSample, _nBitsUsedPerSample, _epin, _epsize) \
  /* Standard Interface Association Descriptor (IAD) */\
  TUD_AUDIO_DESC_IAD(/*_firstitf*/ _itfnum, /*_nitfs*/ 0x02, /*_stridx*/ 0x00),\
  /* Standard AC Interface Descriptor(4.7.1) */\
  TUD_AUDIO_DESC_STD_AC(/*_itfnum*/ _itfnum, /*_nEPs*/ 0x00, /*_stridx*/ _stridx),\
  /* Class-Specific AC Interface Header Descriptor(4.7.2) */\
  TUD_AUDIO_DESC_CS_AC(/*_bcdADC*/ 0x0200, /*_category*/ AUDIO_FUNC_MICROPHONE, /*_totallen*/ TUD_AUDIO_DESC_CLK_SRC_LEN+TUD_AUDIO_DESC_INPUT_TERM_LEN+TUD_AUDIO_DESC_OUTPUT_TERM_LEN+TUD_AUDIO_DESC_FEATURE_UNIT_FOUR_CHANNEL_LEN, /*_ctrl*/ AUDIO_CS_AS_INTERFACE_CTRL_LATENCY_POS),\
  /* Clock Source Descriptor(4.7.2.1) */\
  TUD_AUDIO_DESC_CLK_SRC(/*_clkid*/ 0x04, /*_attr*/ AUDIO_CLOCK_SOURCE_ATT_INT_FIX_CLK, /*_ctrl*/ (AUDIO_CTRL_R << AUDIO_CLOCK_SOURCE_CTRL_CLK_FRQ_POS), /*_assocTerm*/ 0x01,  /*_stridx*/ 0x00),\
  /* Input Terminal Descriptor(4.7.2.4) */\
  TUD_AUDIO_DESC_INPUT_TERM(/*_termid*/ 0x01, /*_termtype*/ AUDIO_TERM_TYPE_IN_GENERIC_MIC, /*_assocTerm*/ 0x03, /*_clkid*/ 0x04, /*_nchannelslogical*/ 0x04, /*_channelcfg*/ AUDIO_CHANNEL_CONFIG_NON_PREDEFINED, /*_idxchannelnames*/ 0x00, /*_ctrl*/ AUDIO_CTRL_R << AUDIO_IN_TERM_CTRL_CONNECTOR_POS, /*_stridx*/ 0x00),\
  /* Output Terminal Descriptor(4.7.2.5) */\
  TUD_AUDIO_DESC_OUTPUT_TERM(/*_termid*/ 0x03, /*_termtype*/ AUDIO_TERM_TYPE_USB_STREAMING, /*_assocTerm*/ 0x01, /*_srcid*/ 0x02, /*_clkid*/ 0x04, /*_ctrl*/ 0x0000, /*_stridx*/ 0x00),\
  /* Feature Unit Descriptor(4.7.2.8) */\
  TUD_AUDIO_DESC_FEATURE_UNIT_FOUR_CHANNEL(/*_unitid*/ 0x02, /*_srcid*/ 0x01, /*_ctrlch0master*/ AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_VOLUME_POS, /*_ctrlch1*/ AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_VOLUME_POS, /*_ctrlch2*/ AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_VOLUME_POS, /*_ctrlch3*/ AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_VOLUME_POS, /*_ctrlch4*/ AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_VOLUME_POS, /*_stridx*/ 0x00),\
  /* Standard AS Interface Descriptor(4.9.1) */\
  /* Interface 1, Alternate 0 - default alternate setting with 0 bandwidth */\
  TUD_AUDIO_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)((_itfnum)+1), /*_altset*/ 0x00, /*_nEPs*/ 0x00, /*_stridx*/ 0x00),\
  /* Standard AS Interface Descriptor(4.9.1) */\
  /* Interface 1, Alternate 1 - alternate interface for data streaming */\
  TUD_AUDIO_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)((_itfnum)+1), /*_altset*/ 0x01, /*_nEPs*/ 0x01, /*_stridx*/ 0x00),\
  /* Class-Specific AS Interface Descriptor(4.9.2) */\
  TUD_AUDIO_DESC_CS_AS_INT(/*_termid*/ 0x03, /*_ctrl*/ AUDIO_CTRL_NONE, /*_formattype*/ AUDIO_FORMAT_TYPE_I, /*_formats*/ AUDIO_DATA_FORMAT_TYPE_I_PCM, /*_nchannelsphysical*/ 0x04, /*_channelcfg*/ AUDIO_CHANNEL_CONFIG_NON_PREDEFINED, /*_stridx*/ 0x00),\
  /* Type I Format Type Descriptor(2.3.1.6 - Audio Formats) */\
  TUD_AUDIO_DESC_TYPE_I_FORMAT(_nBytesPerSample, _nBitsUsedPerSample),\
  /* Standard AS Isochronous Audio Data Endpoint Descriptor(4.10.1.1) */\
  TUD_AUDIO_DESC_STD_AS_ISO_EP(/*_ep*/ _epin, /*_attr*/ (uint8_t) ((uint8_t)TUSB_XFER_ISOCHRONOUS | (uint8_t)TUSB_ISO_EP_ATT_ASYNCHRONOUS | (uint8_t)TUSB_ISO_EP_ATT_DATA), /*_maxEPsize*/ _epsize, /*_interval*/ 0x01),\
  /* Class-Specific AS Isochronous Audio Data Endpoint Descriptor(4.10.1.2) */\
  TUD_AUDIO_DESC_CS_AS_ISO_EP(/*_attr*/ AUDIO_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK, /*_ctrl*/ AUDIO_CTRL_NONE, /*_lockdelayunit*/ AUDIO_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_UNDEFINED, /*_lockdelay*/ 0x0000)

// AUDIO simple descriptor (UAC2) for mono speaker
// - 1 Input Terminal, 2 Feature Unit (Mute and Volume Control), 3 Output Terminal, 4 Clock Source

#define TUD_AUDIO_SPEAKER_MONO_FB_DESC_LEN (TUD_AUDIO_DESC_IAD_LEN\
  + TUD_AUDIO_DESC_STD_AC_LEN\
  + TUD_AUDIO_DESC_CS_AC_LEN\
  + TUD_AUDIO_DESC_CLK_SRC_LEN\
  + TUD_AUDIO_DESC_INPUT_TERM_LEN\
  + TUD_AUDIO_DESC_OUTPUT_TERM_LEN\
  + TUD_AUDIO_DESC_FEATURE_UNIT_ONE_CHANNEL_LEN\
  + TUD_AUDIO_DESC_STD_AS_INT_LEN\
  + TUD_AUDIO_DESC_STD_AS_INT_LEN\
  + TUD_AUDIO_DESC_CS_AS_INT_LEN\
  + TUD_AUDIO_DESC_TYPE_I_FORMAT_LEN\
  + TUD_AUDIO_DESC_STD_AS_ISO_EP_LEN\
  + TUD_AUDIO_DESC_CS_AS_ISO_EP_LEN\
  + TUD_AUDIO_DESC_STD_AS_ISO_FB_EP_LEN)

#define TUD_AUDIO_SPEAKER_MONO_FB_DESCRIPTOR(_itfnum, _stridx, _nBytesPerSample, _nBitsUsedPerSample, _epout, _epsize, _epfb) \
  /* Standard Interface Association Descriptor (IAD) */\
  TUD_AUDIO_DESC_IAD(/*_firstitf*/ _itfnum, /*_nitfs*/ 0x02, /*_stridx*/ 0x00),\
  /* Standard AC Interface Descriptor(4.7.1) */\
  TUD_AUDIO_DESC_STD_AC(/*_itfnum*/ _itfnum, /*_nEPs*/ 0x00, /*_stridx*/ _stridx),\
  /* Class-Specific AC Interface Header Descriptor(4.7.2) */\
  TUD_AUDIO_DESC_CS_AC(/*_bcdADC*/ 0x0200, /*_category*/ AUDIO_FUNC_DESKTOP_SPEAKER, /*_totallen*/ TUD_AUDIO_DESC_CLK_SRC_LEN+TUD_AUDIO_DESC_INPUT_TERM_LEN+TUD_AUDIO_DESC_OUTPUT_TERM_LEN+TUD_AUDIO_DESC_FEATURE_UNIT_ONE_CHANNEL_LEN, /*_ctrl*/ AUDIO_CS_AS_INTERFACE_CTRL_LATENCY_POS),\
  /* Clock Source Descriptor(4.7.2.1) */\
  TUD_AUDIO_DESC_CLK_SRC(/*_clkid*/ 0x04, /*_attr*/ AUDIO_CLOCK_SOURCE_ATT_INT_FIX_CLK, /*_ctrl*/ (AUDIO_CTRL_R << AUDIO_CLOCK_SOURCE_CTRL_CLK_FRQ_POS), /*_assocTerm*/ 0x01,  /*_stridx*/ 0x00),\
  /* Input Terminal Descriptor(4.7.2.4) */\
  TUD_AUDIO_DESC_INPUT_TERM(/*_termid*/ 0x01, /*_termtype*/ AUDIO_TERM_TYPE_USB_STREAMING, /*_assocTerm*/ 0x00, /*_clkid*/ 0x04, /*_nchannelslogical*/ 0x01, /*_channelcfg*/ AUDIO_CHANNEL_CONFIG_NON_PREDEFINED, /*_idxchannelnames*/ 0x00, /*_ctrl*/ 0 * (AUDIO_CTRL_R << AUDIO_IN_TERM_CTRL_CONNECTOR_POS), /*_stridx*/ 0x00),\
  /* Output Terminal Descriptor(4.7.2.5) */\
  TUD_AUDIO_DESC_OUTPUT_TERM(/*_termid*/ 0x03, /*_termtype*/ AUDIO_TERM_TYPE_OUT_DESKTOP_SPEAKER, /*_assocTerm*/ 0x01, /*_srcid*/ 0x02, /*_clkid*/ 0x04, /*_ctrl*/ 0x0000, /*_stridx*/ 0x00),\
  /* Feature Unit Descriptor(4.7.2.8) */\
  TUD_AUDIO_DESC_FEATURE_UNIT_ONE_CHANNEL(/*_unitid*/ 0x02, /*_srcid*/ 0x01, /*_ctrlch0master*/ 0 * (AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_VOLUME_POS), /*_ctrlch1*/ 0 * (AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_VOLUME_POS), /*_stridx*/ 0x00),\
  /* Standard AS Interface Descriptor(4.9.1) */\
  /* Interface 1, Alternate 0 - default alternate setting with 0 bandwidth */\
  TUD_AUDIO_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)((_itfnum) + 1), /*_altset*/ 0x00, /*_nEPs*/ 0x00, /*_stridx*/ 0x00),\
  /* Standard AS Interface Descriptor(4.9.1) */\
  /* Interface 1, Alternate 1 - alternate interface for data streaming */\
  TUD_AUDIO_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)((_itfnum) + 1), /*_altset*/ 0x01, /*_nEPs*/ 0x02, /*_stridx*/ 0x00),\
  /* Class-Specific AS Interface Descriptor(4.9.2) */\
  TUD_AUDIO_DESC_CS_AS_INT(/*_termid*/ 0x01, /*_ctrl*/ AUDIO_CTRL_NONE, /*_formattype*/ AUDIO_FORMAT_TYPE_I, /*_formats*/ AUDIO_DATA_FORMAT_TYPE_I_PCM, /*_nchannelsphysical*/ 0x01, /*_channelcfg*/ AUDIO_CHANNEL_CONFIG_NON_PREDEFINED, /*_stridx*/ 0x00),\
  /* Type I Format Type Descriptor(2.3.1.6 - Audio Formats) */\
  TUD_AUDIO_DESC_TYPE_I_FORMAT(_nBytesPerSample, _nBitsUsedPerSample),\
  /* Standard AS Isochronous Audio Data Endpoint Descriptor(4.10.1.1) */\
  TUD_AUDIO_DESC_STD_AS_ISO_EP(/*_ep*/ _epout, /*_attr*/ (uint8_t) ((uint8_t)TUSB_XFER_ISOCHRONOUS | (uint8_t)TUSB_ISO_EP_ATT_ASYNCHRONOUS | (uint8_t)TUSB_ISO_EP_ATT_DATA), /*_maxEPsize*/ _epsize, /*_interval*/ 0x01),\
  /* Class-Specific AS Isochronous Audio Data Endpoint Descriptor(4.10.1.2) */\
  TUD_AUDIO_DESC_CS_AS_ISO_EP(/*_attr*/ AUDIO_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK, /*_ctrl*/ AUDIO_CTRL_NONE, /*_lockdelayunit*/ AUDIO_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_UNDEFINED, /*_lockdelay*/ 0x0000),\
  /* Standard AS Isochronous Feedback Endpoint Descriptor(4.10.2.1) */\
  TUD_AUDIO_DESC_STD_AS_ISO_FB_EP(/*_ep*/ _epfb, /*_interval*/ 1)\

//   Calculate wMaxPacketSize of Endpoints
#define TUD_AUDIO_EP_SIZE(_maxFrequency, _nBytesPerSample, _nChannels) \
    ((((_maxFrequency + (TUD_OPT_HIGH_SPEED ? 7999 : 999)) / (TUD_OPT_HIGH_SPEED ? 8000 : 1000)) + 1) * _nBytesPerSample * _nChannels)


//--------------------------------------------------------------------+
// USBTMC/USB488 Descriptor Templates
//--------------------------------------------------------------------+

#define TUD_USBTMC_APP_CLASS    (TUSB_CLASS_APPLICATION_SPECIFIC)
#define TUD_USBTMC_APP_SUBCLASS 0x03u

#define TUD_USBTMC_PROTOCOL_STD    0x00u
#define TUD_USBTMC_PROTOCOL_USB488 0x01u

//   Interface number, number of endpoints, EP string index, USB_TMC_PROTOCOL*, bulk-out endpoint ID,
//   bulk-in endpoint ID
#define TUD_USBTMC_IF_DESCRIPTOR(_itfnum, _bNumEndpoints, _stridx, _itfProtocol) \
  /* Interface */ \
  0x09, TUSB_DESC_INTERFACE, _itfnum, 0x00, _bNumEndpoints, TUD_USBTMC_APP_CLASS, TUD_USBTMC_APP_SUBCLASS, _itfProtocol, _stridx

#define TUD_USBTMC_IF_DESCRIPTOR_LEN 9u

#define TUD_USBTMC_BULK_DESCRIPTORS(_epout, _epin, _bulk_epsize) \
  /* Endpoint Out */ \
  7, TUSB_DESC_ENDPOINT, _epout, TUSB_XFER_BULK, U16_TO_U8S_LE(_bulk_epsize), 0u, \
  /* Endpoint In */ \
  7, TUSB_DESC_ENDPOINT, _epin, TUSB_XFER_BULK, U16_TO_U8S_LE(_bulk_epsize), 0u

#define TUD_USBTMC_BULK_DESCRIPTORS_LEN (7u+7u)

/* optional interrupt endpoint */ \
// _int_pollingInterval : for LS/FS, expressed in frames (1ms each). 16 may be a good number?
#define TUD_USBTMC_INT_DESCRIPTOR(_ep_interrupt, _ep_interrupt_size, _int_pollingInterval ) \
  7, TUSB_DESC_ENDPOINT, _ep_interrupt, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(_ep_interrupt_size), _int_pollingInterval

#define TUD_USBTMC_INT_DESCRIPTOR_LEN (7u)

//--------------------------------------------------------------------+
// Vendor Descriptor Templates
//--------------------------------------------------------------------+

#define TUD_VENDOR_DESC_LEN  (9+7+7)

// Interface number, string index, EP Out & IN address, EP size
#define TUD_VENDOR_DESCRIPTOR(_itfnum, _stridx, _epout, _epin, _epsize) \
  /* Interface */\
  9, TUSB_DESC_INTERFACE, _itfnum, 0, 2, TUSB_CLASS_VENDOR_SPECIFIC, 0x00, 0x00, _stridx,\
  /* Endpoint Out */\
  7, TUSB_DESC_ENDPOINT, _epout, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0,\
  /* Endpoint In */\
  7, TUSB_DESC_ENDPOINT, _epin, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0

//--------------------------------------------------------------------+
// DFU Runtime Descriptor Templates
//--------------------------------------------------------------------+

#define TUD_DFU_APP_CLASS    (TUSB_CLASS_APPLICATION_SPECIFIC)
#define TUD_DFU_APP_SUBCLASS (APP_SUBCLASS_DFU_RUNTIME)

// Length of template descriptr: 18 bytes
#define TUD_DFU_RT_DESC_LEN (9 + 9)

// DFU runtime descriptor
// Interface number, string index, attributes, detach timeout, transfer size
#define TUD_DFU_RT_DESCRIPTOR(_itfnum, _stridx, _attr, _timeout, _xfer_size) \
  /* Interface */ \
  9, TUSB_DESC_INTERFACE, _itfnum, 0, 0, TUD_DFU_APP_CLASS, TUD_DFU_APP_SUBCLASS, DFU_PROTOCOL_RT, _stridx, \
  /* Function */ \
  9, DFU_DESC_FUNCTIONAL, _attr, U16_TO_U8S_LE(_timeout), U16_TO_U8S_LE(_xfer_size), U16_TO_U8S_LE(0x0101)

//--------------------------------------------------------------------+
// DFU Descriptor Templates
//--------------------------------------------------------------------+

// Length of template descriptor: 9 bytes + number of alternatives * 9
#define TUD_DFU_DESC_LEN(_alt_count)    (9 + (_alt_count) * 9)

// Interface number, Alternate count, starting string index, attributes, detach timeout, transfer size
// Note: Alternate count must be numeric or macro, string index is increased by one for each Alt interface
#define TUD_DFU_DESCRIPTOR(_itfnum, _alt_count, _stridx, _attr, _timeout, _xfer_size) \
  TU_XSTRCAT(_TUD_DFU_ALT_,_alt_count)(_itfnum, 0, _stridx), \
  /* Function */ \
  9, DFU_DESC_FUNCTIONAL, _attr, U16_TO_U8S_LE(_timeout), U16_TO_U8S_LE(_xfer_size), U16_TO_U8S_LE(0x0101)

#define _TUD_DFU_ALT(_itfnum, _alt, _stridx) \
  /* Interface */ \
  9, TUSB_DESC_INTERFACE, _itfnum, _alt, 0, TUD_DFU_APP_CLASS, TUD_DFU_APP_SUBCLASS, DFU_PROTOCOL_DFU, _stridx

#define _TUD_DFU_ALT_1(_itfnum, _alt_count, _stridx) \
  _TUD_DFU_ALT(_itfnum, _alt_count, _stridx)

#define _TUD_DFU_ALT_2(_itfnum, _alt_count, _stridx) \
  _TUD_DFU_ALT(_itfnum, _alt_count, _stridx),      \
  _TUD_DFU_ALT_1(_itfnum, _alt_count+1, _stridx+1)

#define _TUD_DFU_ALT_3(_itfnum, _alt_count, _stridx) \
  _TUD_DFU_ALT(_itfnum, _alt_count, _stridx),      \
  _TUD_DFU_ALT_2(_itfnum, _alt_count+1, _stridx+1)

#define _TUD_DFU_ALT_4(_itfnum, _alt_count, _stridx) \
  _TUD_DFU_ALT(_itfnum, _alt_count, _stridx),      \
  _TUD_DFU_ALT_3(_itfnum, _alt_count+1, _stridx+1)

#define _TUD_DFU_ALT_5(_itfnum, _alt_count, _stridx) \
  _TUD_DFU_ALT(_itfnum, _alt_count, _stridx),      \
  _TUD_DFU_ALT_4(_itfnum, _alt_count+1, _stridx+1)

#define _TUD_DFU_ALT_6(_itfnum, _alt_count, _stridx) \
  _TUD_DFU_ALT(_itfnum, _alt_count, _stridx),      \
  _TUD_DFU_ALT_5(_itfnum, _alt_count+1, _stridx+1)

#define _TUD_DFU_ALT_7(_itfnum, _alt_count, _stridx) \
  _TUD_DFU_ALT(_itfnum, _alt_count, _stridx),      \
  _TUD_DFU_ALT_6(_itfnum, _alt_count+1, _stridx+1)

#define _TUD_DFU_ALT_8(_itfnum, _alt_count, _stridx) \
  _TUD_DFU_ALT(_itfnum, _alt_count, _stridx),      \
  _TUD_DFU_ALT_7(_itfnum, _alt_count+1, _stridx+1)

//--------------------------------------------------------------------+
// CDC-ECM Descriptor Templates
//--------------------------------------------------------------------+

// Length of template descriptor: 71 bytes
#define TUD_CDC_ECM_DESC_LEN  (8+9+5+5+13+7+9+9+7+7)

// CDC-ECM Descriptor Template
// Interface number, description string index, MAC address string index, EP notification address and size, EP data address (out, in), and size, max segment size.
#define TUD_CDC_ECM_DESCRIPTOR(_itfnum, _desc_stridx, _mac_stridx, _ep_notif, _ep_notif_size, _epout, _epin, _epsize, _maxsegmentsize) \
  /* Interface Association */\
  8, TUSB_DESC_INTERFACE_ASSOCIATION, _itfnum, 2, TUSB_CLASS_CDC, CDC_COMM_SUBCLASS_ETHERNET_CONTROL_MODEL, 0, 0,\
  /* CDC Control Interface */\
  9, TUSB_DESC_INTERFACE, _itfnum, 0, 1, TUSB_CLASS_CDC, CDC_COMM_SUBCLASS_ETHERNET_CONTROL_MODEL, 0, _desc_stridx,\
  /* CDC-ECM Header */\
  5, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_HEADER, U16_TO_U8S_LE(0x0120),\
  /* CDC-ECM Union */\
  5, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_UNION, _itfnum, (uint8_t)((_itfnum) + 1),\
  /* CDC-ECM Functional Descriptor */\
  13, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_ETHERNET_NETWORKING, _mac_stridx, 0, 0, 0, 0, U16_TO_U8S_LE(_maxsegmentsize), U16_TO_U8S_LE(0), 0,\
  /* Endpoint Notification */\
  7, TUSB_DESC_ENDPOINT, _ep_notif, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(_ep_notif_size), 1,\
  /* CDC Data Interface (default inactive) */\
  9, TUSB_DESC_INTERFACE, (uint8_t)((_itfnum)+1), 0, 0, TUSB_CLASS_CDC_DATA, 0, 0, 0,\
  /* CDC Data Interface (alternative active) */\
  9, TUSB_DESC_INTERFACE, (uint8_t)((_itfnum)+1), 1, 2, TUSB_CLASS_CDC_DATA, 0, 0, 0,\
  /* Endpoint In */\
  7, TUSB_DESC_ENDPOINT, _epin, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0,\
  /* Endpoint Out */\
  7, TUSB_DESC_ENDPOINT, _epout, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0

//--------------------------------------------------------------------+
// RNDIS Descriptor Templates
//--------------------------------------------------------------------+

#if 0
/* Windows XP */
#define TUD_RNDIS_ITF_CLASS    TUSB_CLASS_CDC
#define TUD_RNDIS_ITF_SUBCLASS CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL
#define TUD_RNDIS_ITF_PROTOCOL 0xFF /* CDC_COMM_PROTOCOL_MICROSOFT_RNDIS */
#else
/* Windows 7+ */
#define TUD_RNDIS_ITF_CLASS    TUSB_CLASS_WIRELESS_CONTROLLER
#define TUD_RNDIS_ITF_SUBCLASS 0x01
#define TUD_RNDIS_ITF_PROTOCOL 0x03
#endif

// Length of template descriptor: 66 bytes
#define TUD_RNDIS_DESC_LEN  (8+9+5+5+4+5+7+9+7+7)

// RNDIS Descriptor Template
// Interface number, string index, EP notification address and size, EP data address (out, in) and size.
#define TUD_RNDIS_DESCRIPTOR(_itfnum, _stridx, _ep_notif, _ep_notif_size, _epout, _epin, _epsize) \
  /* Interface Association */\
  8, TUSB_DESC_INTERFACE_ASSOCIATION, _itfnum, 2, TUD_RNDIS_ITF_CLASS, TUD_RNDIS_ITF_SUBCLASS, TUD_RNDIS_ITF_PROTOCOL, 0,\
  /* CDC Control Interface */\
  9, TUSB_DESC_INTERFACE, _itfnum, 0, 1, TUD_RNDIS_ITF_CLASS, TUD_RNDIS_ITF_SUBCLASS, TUD_RNDIS_ITF_PROTOCOL, _stridx,\
  /* CDC-ACM Header */\
  5, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_HEADER, U16_TO_U8S_LE(0x0110),\
  /* CDC Call Management */\
  5, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_CALL_MANAGEMENT, 0, (uint8_t)((_itfnum) + 1),\
  /* ACM */\
  4, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_ABSTRACT_CONTROL_MANAGEMENT, 0,\
  /* CDC Union */\
  5, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_UNION, _itfnum, (uint8_t)((_itfnum) + 1),\
  /* Endpoint Notification */\
  7, TUSB_DESC_ENDPOINT, _ep_notif, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(_ep_notif_size), 1,\
  /* CDC Data Interface */\
  9, TUSB_DESC_INTERFACE, (uint8_t)((_itfnum)+1), 0, 2, TUSB_CLASS_CDC_DATA, 0, 0, 0,\
  /* Endpoint In */\
  7, TUSB_DESC_ENDPOINT, _epin, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0,\
  /* Endpoint Out */\
  7, TUSB_DESC_ENDPOINT, _epout, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0

//--------------------------------------------------------------------+
// Bluetooth Radio Descriptor Templates
//--------------------------------------------------------------------+

#define TUD_BT_APP_CLASS                    (TUSB_CLASS_WIRELESS_CONTROLLER)
#define TUD_BT_APP_SUBCLASS                 0x01
#define TUD_BT_PROTOCOL_PRIMARY_CONTROLLER  0x01
#define TUD_BT_PROTOCOL_AMP_CONTROLLER      0x02

// Length of template descriptor: 38 bytes + number of ISO alternatives * 23
#define TUD_BTH_DESC_LEN (8 + 9 + 7 + 7 + 7 + (CFG_TUD_BTH_ISO_ALT_COUNT) * (9 + 7 + 7))

/* Primary Interface */
#define TUD_BTH_PRI_ITF(_itfnum, _stridx, _ep_evt, _ep_evt_size, _ep_evt_interval, _ep_in, _ep_out, _ep_size) \
  9, TUSB_DESC_INTERFACE, _itfnum, 0, 3, TUD_BT_APP_CLASS, TUD_BT_APP_SUBCLASS, TUD_BT_PROTOCOL_PRIMARY_CONTROLLER, _stridx, \
  /* Endpoint In for events */ \
  7, TUSB_DESC_ENDPOINT, _ep_evt, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(_ep_evt_size), _ep_evt_interval, \
  /* Endpoint In for ACL data */ \
  7, TUSB_DESC_ENDPOINT, _ep_in, TUSB_XFER_BULK, U16_TO_U8S_LE(_ep_size), 1, \
  /* Endpoint Out for ACL data */ \
  7, TUSB_DESC_ENDPOINT, _ep_out, TUSB_XFER_BULK, U16_TO_U8S_LE(_ep_size), 1

#define TUD_BTH_ISO_ITF(_itfnum, _alt, _ep_in, _ep_out, _n) ,\
  /* Interface with 2 endpoints */ \
  9, TUSB_DESC_INTERFACE, _itfnum, _alt, 2, TUD_BT_APP_CLASS, TUD_BT_APP_SUBCLASS, TUD_BT_PROTOCOL_PRIMARY_CONTROLLER, 0, \
  /* Isochronous endpoints */ \
  7, TUSB_DESC_ENDPOINT, _ep_in, TUSB_XFER_ISOCHRONOUS, U16_TO_U8S_LE(_n), 1, \
  7, TUSB_DESC_ENDPOINT, _ep_out, TUSB_XFER_ISOCHRONOUS, U16_TO_U8S_LE(_n), 1

#define _FIRST(a, ...) a
#define _REST(a, ...) __VA_ARGS__

#define TUD_BTH_ISO_ITF_0(_itfnum, ...)
#define TUD_BTH_ISO_ITF_1(_itfnum, _ep_in, _ep_out, ...) TUD_BTH_ISO_ITF(_itfnum, (CFG_TUD_BTH_ISO_ALT_COUNT) - 1, _ep_in, _ep_out, _FIRST(__VA_ARGS__))
#define TUD_BTH_ISO_ITF_2(_itfnum, _ep_in, _ep_out, ...) TUD_BTH_ISO_ITF(_itfnum, (CFG_TUD_BTH_ISO_ALT_COUNT) - 2, _ep_in, _ep_out, _FIRST(__VA_ARGS__)) \
  TUD_BTH_ISO_ITF_1(_itfnum, _ep_in, _ep_out, _REST(__VA_ARGS__))
#define TUD_BTH_ISO_ITF_3(_itfnum, _ep_in, _ep_out, ...) TUD_BTH_ISO_ITF(_itfnum, (CFG_TUD_BTH_ISO_ALT_COUNT) - 3, _ep_in, _ep_out, _FIRST(__VA_ARGS__)) \
  TUD_BTH_ISO_ITF_2(_itfnum, _ep_in, _ep_out, _REST(__VA_ARGS__))
#define TUD_BTH_ISO_ITF_4(_itfnum, _ep_in, _ep_out, ...) TUD_BTH_ISO_ITF(_itfnum, (CFG_TUD_BTH_ISO_ALT_COUNT) - 4, _ep_in, _ep_out, _FIRST(__VA_ARGS__)) \
  TUD_BTH_ISO_ITF_3(_itfnum, _ep_in, _ep_out, _REST(__VA_ARGS__))
#define TUD_BTH_ISO_ITF_5(_itfnum, _ep_in, _ep_out, ...) TUD_BTH_ISO_ITF(_itfnum, (CFG_TUD_BTH_ISO_ALT_COUNT) - 5, _ep_in, _ep_out, _FIRST(__VA_ARGS__)) \
  TUD_BTH_ISO_ITF_4(_itfnum, _ep_in, _ep_out, _REST(__VA_ARGS__))
#define TUD_BTH_ISO_ITF_6(_itfnum, _ep_in, _ep_out, ...) TUD_BTH_ISO_ITF(_itfnum, (CFG_TUD_BTH_ISO_ALT_COUNT) - 6, _ep_in, _ep_out, _FIRST(__VA_ARGS__)) \
  TUD_BTH_ISO_ITF_5(_itfnum, _ep_in, _ep_out, _REST(__VA_ARGS__))

#define TUD_BTH_ISO_ITFS(_itfnum, _ep_in, _ep_out, ...) \
  TU_XSTRCAT(TUD_BTH_ISO_ITF_, CFG_TUD_BTH_ISO_ALT_COUNT)(_itfnum, _ep_in, _ep_out, __VA_ARGS__)

// BT Primary controller descriptor
// Interface number, string index, attributes, event endpoint, event endpoint size, interval, data in, data out, data endpoint size, iso endpoint sizes
// TODO BTH should also use IAD like CDC for composite device
#define TUD_BTH_DESCRIPTOR(_itfnum, _stridx, _ep_evt, _ep_evt_size, _ep_evt_interval, _ep_in, _ep_out, _ep_size,...) \
  /* Interface Associate */\
  8, TUSB_DESC_INTERFACE_ASSOCIATION, _itfnum, 2, TUD_BT_APP_CLASS, TUD_BT_APP_SUBCLASS, TUD_BT_PROTOCOL_PRIMARY_CONTROLLER, 0,\
  TUD_BTH_PRI_ITF(_itfnum, _stridx, _ep_evt, _ep_evt_size, _ep_evt_interval, _ep_in, _ep_out, _ep_size) \
  TUD_BTH_ISO_ITFS(_itfnum + 1, _ep_in + 1, _ep_out + 1, __VA_ARGS__)

//--------------------------------------------------------------------+
// CDC-NCM Descriptor Templates
//--------------------------------------------------------------------+

// Length of template descriptor
#define TUD_CDC_NCM_DESC_LEN  (8+9+5+5+13+6+7+9+9+7+7)

// CDC-ECM Descriptor Template
// Interface number, description string index, MAC address string index, EP notification address and size, EP data address (out, in), and size, max segment size.
#define TUD_CDC_NCM_DESCRIPTOR(_itfnum, _desc_stridx, _mac_stridx, _ep_notif, _ep_notif_size, _epout, _epin, _epsize, _maxsegmentsize) \
  /* Interface Association */\
  8, TUSB_DESC_INTERFACE_ASSOCIATION, _itfnum, 2, TUSB_CLASS_CDC, CDC_COMM_SUBCLASS_NETWORK_CONTROL_MODEL, 0, 0,\
  /* CDC Control Interface */\
  9, TUSB_DESC_INTERFACE, _itfnum, 0, 1, TUSB_CLASS_CDC, CDC_COMM_SUBCLASS_NETWORK_CONTROL_MODEL, 0, _desc_stridx,\
  /* CDC-NCM Header */\
  5, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_HEADER, U16_TO_U8S_LE(0x0110),\
  /* CDC-NCM Union */\
  5, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_UNION, _itfnum, (uint8_t)((_itfnum) + 1),\
  /* CDC-NCM Functional Descriptor */\
  13, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_ETHERNET_NETWORKING, _mac_stridx, 0, 0, 0, 0, U16_TO_U8S_LE(_maxsegmentsize), U16_TO_U8S_LE(0), 0, \
  /* CDC-NCM Functional Descriptor */\
  6, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_NCM, U16_TO_U8S_LE(0x0100), 0, \
  /* Endpoint Notification */\
  7, TUSB_DESC_ENDPOINT, _ep_notif, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(_ep_notif_size), 50,\
  /* CDC Data Interface (default inactive) */\
  9, TUSB_DESC_INTERFACE, (uint8_t)((_itfnum)+1), 0, 0, TUSB_CLASS_CDC_DATA, 0, NCM_DATA_PROTOCOL_NETWORK_TRANSFER_BLOCK, 0,\
  /* CDC Data Interface (alternative active) */\
  9, TUSB_DESC_INTERFACE, (uint8_t)((_itfnum)+1), 1, 2, TUSB_CLASS_CDC_DATA, 0, NCM_DATA_PROTOCOL_NETWORK_TRANSFER_BLOCK, 0,\
  /* Endpoint In */\
  7, TUSB_DESC_ENDPOINT, _epin, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0,\
  /* Endpoint Out */\
  7, TUSB_DESC_ENDPOINT, _epout, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0

#ifdef __cplusplus
}
#endif

#endif /* _TUSB_USBD_H_ */

/** @} */
