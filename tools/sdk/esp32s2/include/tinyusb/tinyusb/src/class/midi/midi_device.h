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

#include "common/tusb_common.h"
#include "device/usbd.h"

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
bool     tud_midi_n_mounted    (uint8_t itf);
uint32_t tud_midi_n_available  (uint8_t itf, uint8_t jack_id);
uint32_t tud_midi_n_read       (uint8_t itf, uint8_t jack_id, void* buffer, uint32_t bufsize);
void     tud_midi_n_read_flush (uint8_t itf, uint8_t jack_id);
uint32_t tud_midi_n_write      (uint8_t itf, uint8_t jack_id, uint8_t const* buffer, uint32_t bufsize);

static inline
uint32_t tud_midi_n_write24    (uint8_t itf, uint8_t jack_id, uint8_t b1, uint8_t b2, uint8_t b3);

bool tud_midi_n_receive        (uint8_t itf, uint8_t packet[4]);
bool tud_midi_n_send           (uint8_t itf, uint8_t const packet[4]);

//--------------------------------------------------------------------+
// Application API (Interface0)
//--------------------------------------------------------------------+
static inline bool     tud_midi_mounted    (void);
static inline uint32_t tud_midi_available  (void);
static inline uint32_t tud_midi_read       (void* buffer, uint32_t bufsize);
static inline void     tud_midi_read_flush (void);
static inline uint32_t tud_midi_write      (uint8_t jack_id, uint8_t const* buffer, uint32_t bufsize);
static inline uint32_t tudi_midi_write24   (uint8_t jack_id, uint8_t b1, uint8_t b2, uint8_t b3);
static inline bool     tud_midi_receive    (uint8_t packet[4]);
static inline bool     tud_midi_send       (uint8_t const packet[4]);

//--------------------------------------------------------------------+
// Application Callback API (weak is optional)
//--------------------------------------------------------------------+
TU_ATTR_WEAK void tud_midi_rx_cb(uint8_t itf);

//--------------------------------------------------------------------+
// Inline Functions
//--------------------------------------------------------------------+

static inline uint32_t tud_midi_n_write24 (uint8_t itf, uint8_t jack_id, uint8_t b1, uint8_t b2, uint8_t b3)
{
  uint8_t msg[3] = { b1, b2, b3 };
  return tud_midi_n_write(itf, jack_id, msg, 3);
}

static inline bool tud_midi_mounted (void)
{
  return tud_midi_n_mounted(0);
}

static inline uint32_t tud_midi_available (void)
{
  return tud_midi_n_available(0, 0);
}

static inline uint32_t tud_midi_read (void* buffer, uint32_t bufsize)
{
  return tud_midi_n_read(0, 0, buffer, bufsize);
}

static inline void tud_midi_read_flush (void)
{
  tud_midi_n_read_flush(0, 0);
}

static inline uint32_t tud_midi_write (uint8_t jack_id, uint8_t const* buffer, uint32_t bufsize)
{
  return tud_midi_n_write(0, jack_id, buffer, bufsize);
}

static inline uint32_t tudi_midi_write24 (uint8_t jack_id, uint8_t b1, uint8_t b2, uint8_t b3)
{
  uint8_t msg[3] = { b1, b2, b3 };
  return tud_midi_write(jack_id, msg, 3);
}

static inline bool tud_midi_receive (uint8_t packet[4])
{
  return tud_midi_n_receive(0, packet);
}

static inline bool tud_midi_send (uint8_t const packet[4])
{
  return tud_midi_n_send(0, packet);
}

//--------------------------------------------------------------------+
// Internal Class Driver API
//--------------------------------------------------------------------+
void     midid_init             (void);
void     midid_reset            (uint8_t rhport);
uint16_t midid_open             (uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t max_len);
bool     midid_control_request  (uint8_t rhport, tusb_control_request_t const * request);
bool     midid_control_complete (uint8_t rhport, tusb_control_request_t const * request);
bool     midid_xfer_cb          (uint8_t rhport, uint8_t edpt_addr, xfer_result_t result, uint32_t xferred_bytes);

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_MIDI_DEVICE_H_ */

/** @} */
/** @} */
