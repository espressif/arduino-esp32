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

#ifndef _TUSB_MIDI_DEVICE_H_
#define _TUSB_MIDI_DEVICE_H_

#include "class/audio/audio.h"
#include "midi.h"

//--------------------------------------------------------------------+
// Class Driver Configuration
//--------------------------------------------------------------------+

#if !defined(CFG_TUD_MIDI_EP_BUFSIZE) && defined(CFG_TUD_MIDI_EPSIZE)
  #warning CFG_TUD_MIDI_EPSIZE is renamed to CFG_TUD_MIDI_EP_BUFSIZE, please update to use the new name
  #define CFG_TUD_MIDI_EP_BUFSIZE    CFG_TUD_MIDI_EPSIZE
#endif

#ifndef CFG_TUD_MIDI_EP_BUFSIZE
  #define CFG_TUD_MIDI_EP_BUFSIZE     (TUD_OPT_HIGH_SPEED ? 512 : 64)
#endif

#ifdef __cplusplus
 extern "C" {
#endif

/** \addtogroup MIDI_Serial Serial
 *  @{
 *  \defgroup   MIDI_Serial_Device Device
 *  @{ */

//--------------------------------------------------------------------+
// Application API (Multiple Interfaces)
// CFG_TUD_MIDI > 1
//--------------------------------------------------------------------+

// Check if midi interface is mounted
bool     tud_midi_n_mounted      (uint8_t itf);

// Get the number of bytes available for reading
uint32_t tud_midi_n_available    (uint8_t itf, uint8_t cable_num);

// Read byte stream              (legacy)
uint32_t tud_midi_n_stream_read  (uint8_t itf, uint8_t cable_num, void* buffer, uint32_t bufsize);

// Write byte Stream             (legacy)
uint32_t tud_midi_n_stream_write (uint8_t itf, uint8_t cable_num, uint8_t const* buffer, uint32_t bufsize);

// Read event packet             (4 bytes)
bool     tud_midi_n_packet_read  (uint8_t itf, uint8_t packet[4]);

// Write event packet            (4 bytes)
bool     tud_midi_n_packet_write (uint8_t itf, uint8_t const packet[4]);

//--------------------------------------------------------------------+
// Application API (Single Interface)
//--------------------------------------------------------------------+
static inline bool     tud_midi_mounted      (void);
static inline uint32_t tud_midi_available    (void);

static inline uint32_t tud_midi_stream_read  (void* buffer, uint32_t bufsize);
static inline uint32_t tud_midi_stream_write (uint8_t cable_num, uint8_t const* buffer, uint32_t bufsize);

static inline bool     tud_midi_packet_read  (uint8_t packet[4]);
static inline bool     tud_midi_packet_write (uint8_t const packet[4]);

//------------- Deprecated API name  -------------//
// TODO remove after 0.10.0 release

TU_ATTR_DEPRECATED("tud_midi_read() is renamed to tud_midi_stream_read()")
static inline uint32_t tud_midi_read (void* buffer, uint32_t bufsize)
{
  return tud_midi_stream_read(buffer, bufsize);
}

TU_ATTR_DEPRECATED("tud_midi_write() is renamed to tud_midi_stream_write()")
static inline uint32_t tud_midi_write(uint8_t cable_num, uint8_t const* buffer, uint32_t bufsize)
{
  return tud_midi_stream_write(cable_num, buffer, bufsize);
}


TU_ATTR_DEPRECATED("tud_midi_send() is renamed to tud_midi_packet_write()")
static inline bool tud_midi_send(uint8_t packet[4])
{
  return tud_midi_packet_write(packet);
}

TU_ATTR_DEPRECATED("tud_midi_receive() is renamed to tud_midi_packet_read()")
static inline bool tud_midi_receive(uint8_t packet[4])
{
  return tud_midi_packet_read(packet);
}

//--------------------------------------------------------------------+
// Application Callback API (weak is optional)
//--------------------------------------------------------------------+
TU_ATTR_WEAK void tud_midi_rx_cb(uint8_t itf);

//--------------------------------------------------------------------+
// Inline Functions
//--------------------------------------------------------------------+

static inline bool tud_midi_mounted (void)
{
  return tud_midi_n_mounted(0);
}

static inline uint32_t tud_midi_available (void)
{
  return tud_midi_n_available(0, 0);
}

static inline uint32_t tud_midi_stream_read (void* buffer, uint32_t bufsize)
{
  return tud_midi_n_stream_read(0, 0, buffer, bufsize);
}

static inline uint32_t tud_midi_stream_write (uint8_t cable_num, uint8_t const* buffer, uint32_t bufsize)
{
  return tud_midi_n_stream_write(0, cable_num, buffer, bufsize);
}

static inline bool tud_midi_packet_read (uint8_t packet[4])
{
  return tud_midi_n_packet_read(0, packet);
}

static inline bool tud_midi_packet_write (uint8_t const packet[4])
{
  return tud_midi_n_packet_write(0, packet);
}

//--------------------------------------------------------------------+
// Internal Class Driver API
//--------------------------------------------------------------------+
void     midid_init            (void);
bool     midid_deinit          (void);
void     midid_reset           (uint8_t rhport);
uint16_t midid_open            (uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t max_len);
bool     midid_control_xfer_cb (uint8_t rhport, uint8_t stage, tusb_control_request_t const * request);
bool     midid_xfer_cb         (uint8_t rhport, uint8_t edpt_addr, xfer_result_t result, uint32_t xferred_bytes);

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_MIDI_DEVICE_H_ */

/** @} */
/** @} */
