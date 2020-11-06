/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Ha Thach (tinyusb.org)
 * Copyright (c) 2020 Reinhard Panhuber
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

#ifndef _TUSB_AUDIO_DEVICE_H_
#define _TUSB_AUDIO_DEVICE_H_

#include "assert.h"
#include "common/tusb_common.h"
#include "device/usbd.h"

#include "audio.h"

//--------------------------------------------------------------------+
// Class Driver Configuration
//--------------------------------------------------------------------+

// Number of Standard AS Interface Descriptors (4.9.1) defined per audio function - this is required to be able to remember the current alternate settings of these interfaces - We restrict us here to have a constant number for all audio functions (which means this has to be the maximum number of AS interfaces an audio function has and a second audio function with less AS interfaces just waste a few bytes)
#ifndef CFG_TUD_AUDIO_N_AS_INT
#define CFG_TUD_AUDIO_N_AS_INT  0
#endif

// Size of control buffer used to receive and send control messages via EP0 - has to be big enough to hold your biggest request structure e.g. range requests with multiple intervals defined or cluster descriptors
#ifndef CFG_TUD_AUDIO_CTRL_BUF_SIZE
#error You must define an audio class control request buffer size!
#endif

// Use of TX/RX FIFOs - If sizes are not zero, audio.c implements FIFOs for RX and TX (whatever defined).
// For RX: the input stream gets decoded into its corresponding channels, where for each channel a FIFO is setup to hold its data -> see: audio_rx_done_cb().
// For TX: the output stream is composed from CFG_TUD_AUDIO_N_CHANNELS_TX channels, where for each channel a FIFO is defined.
// Further, it implements encoding and decoding of the individual channels (parameterized by the defines below).
// If you don't use the FIFOs you need to handle encoding and decoding on your own in audio_rx_done_cb() and audio_tx_done_cb(). This, however, allows for optimizations.

#ifndef CFG_TUD_AUDIO_TX_FIFO_SIZE
#define CFG_TUD_AUDIO_TX_FIFO_SIZE  0                           // Buffer size per channel
#endif

#ifndef CFG_TUD_AUDIO_RX_FIFO_SIZE
#define CFG_TUD_AUDIO_RX_FIFO_SIZE  0                           // Buffer size per channel
#endif

// End point sizes - Limits: Full Speed <= 1023, High Speed <= 1024
#ifndef CFG_TUD_AUDIO_EPSIZE_IN
#define CFG_TUD_AUDIO_EPSIZE_IN 0   // TX
#endif

#ifndef CFG_TUD_AUDIO_EPSIZE_OUT
#define CFG_TUD_AUDIO_EPSIZE_OUT 0  // RX
#endif

#ifndef CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
#define CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP 0                      // Feedback
#endif

#ifndef CFG_TUD_AUDIO_INT_CTR_EPSIZE_IN
#define CFG_TUD_AUDIO_INT_CTR_EPSIZE_IN 0                       // Audio interrupt control
#endif

#if CFG_TUD_AUDIO_INT_CTR_EPSIZE_IN
#ifndef CFG_TUD_AUDIO_INT_CTR_BUFSIZE
#define CFG_TUD_AUDIO_INT_CTR_BUFSIZE   6                       // Buffer size of audio control interrupt EP - 6 Bytes according to UAC 2 specification (p. 74)
#endif
#endif

#ifndef CFG_TUD_AUDIO_N_CHANNELS_TX
#define CFG_TUD_AUDIO_N_CHANNELS_TX                     1
#endif

#ifndef CFG_TUD_AUDIO_N_CHANNELS_RX
#define CFG_TUD_AUDIO_N_CHANNELS_RX                     1
#endif

// Audio data format types
#ifndef CFG_TUD_AUDIO_FORMAT_TYPE_TX
#define CFG_TUD_AUDIO_FORMAT_TYPE_TX                AUDIO_FORMAT_TYPE_UNDEFINED     // If this option is used, an encoding function has to be implemented in audio_device.c
#endif

#ifndef CFG_TUD_AUDIO_FORMAT_TYPE_RX
#define CFG_TUD_AUDIO_FORMAT_TYPE_RX                AUDIO_FORMAT_TYPE_UNDEFINED     // If this option is used, a decoding function has to be implemented in audio_device.c
#endif

// Audio data format type I specifications
#if CFG_TUD_AUDIO_FORMAT_TYPE_TX == AUDIO_FORMAT_TYPE_I

// Type definitions - for possible formats see: audio_data_format_type_I_t and further in UAC2 specifications.
#ifndef CFG_TUD_AUDIO_FORMAT_TYPE_I_TX
#define CFG_TUD_AUDIO_FORMAT_TYPE_I_TX              AUDIO_DATA_FORMAT_TYPE_I_PCM
#endif

#ifndef CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_TX             // bSubslotSize
#define CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_TX             1
#endif

#ifndef CFG_TUD_AUDIO_TX_ITEMSIZE
#if CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_TX == 1
#define CFG_TUD_AUDIO_TX_ITEMSIZE 1
#elif CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_TX == 2
#define CFG_TUD_AUDIO_TX_ITEMSIZE 2
#else
#define CFG_TUD_AUDIO_TX_ITEMSIZE 4
#endif
#endif

#if CFG_TUD_AUDIO_TX_ITEMSIZE < CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_TX
#error FIFO element size (ITEMSIZE) must not be smaller then sample size
#endif

#endif

#if CFG_TUD_AUDIO_FORMAT_TYPE_RX == AUDIO_FORMAT_TYPE_I

#ifndef CFG_TUD_AUDIO_FORMAT_TYPE_I_RX
#define CFG_TUD_AUDIO_FORMAT_TYPE_I_RX              AUDIO_DATA_FORMAT_TYPE_I_PCM
#endif

#ifndef CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_RX             // bSubslotSize
#define CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_RX             1
#endif

#if CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_RX == 1
#define CFG_TUD_AUDIO_RX_ITEMSIZE 1
#elif CFG_TUD_AUDIO_N_BYTES_PER_SAMPLE_RX == 2
#define CFG_TUD_AUDIO_RX_ITEMSIZE 2
#else
#define CFG_TUD_AUDIO_RX_ITEMSIZE 4
#endif

#endif

//static_assert(sizeof(tud_audio_desc_lengths) != CFG_TUD_AUDIO, "Supply audio function descriptor pack length!");

// Supported types of this driver:
// AUDIO_DATA_FORMAT_TYPE_I_PCM     -   Required definitions: CFG_TUD_AUDIO_N_CHANNELS and CFG_TUD_AUDIO_BYTES_PER_CHANNEL

#ifdef __cplusplus
extern "C" {
#endif

/** \addtogroup AUDIO_Serial Serial
 *  @{
 *  \defgroup   AUDIO_Serial_Device Device
 *  @{ */

//--------------------------------------------------------------------+
// Application API (Multiple Interfaces)
// CFG_TUD_AUDIO > 1
//--------------------------------------------------------------------+
bool     tud_audio_n_mounted    (uint8_t itf);

#if CFG_TUD_AUDIO_EPSIZE_OUT && CFG_TUD_AUDIO_RX_FIFO_SIZE
#if CFG_TUD_AUDIO_RX_FIFO_COUNT > 1
uint16_t tud_audio_n_available  (uint8_t itf, uint8_t channelId);
uint16_t tud_audio_n_read       (uint8_t itf, uint8_t channelId, void* buffer, uint16_t bufsize);
void     tud_audio_n_read_flush (uint8_t itf, uint8_t channelId);
#else
uint16_t tud_audio_n_available  (uint8_t itf);
uint16_t tud_audio_n_read       (uint8_t itf, void* buffer, uint16_t bufsize);
void     tud_audio_n_read_flush (uint8_t itf);
#endif
#endif

/*  This function is intended for later use once EP buffers (at least for ISO EPs) are implemented as ring buffers
#if CFG_TUD_AUDIO_EPSIZE_IN && !CFG_TUD_AUDIO_TX_FIFO_SIZE
uint16_t tud_audio_n_write_ep_in_buffer(uint8_t itf, const void * data, uint16_t len)
#endif
*/

#ifndef CFG_TUD_AUDIO_TX_FIFO_COUNT
#define CFG_TUD_AUDIO_TX_FIFO_COUNT 1
#endif

#if CFG_TUD_AUDIO_EPSIZE_IN && CFG_TUD_AUDIO_TX_FIFO_SIZE
#if CFG_TUD_AUDIO_TX_FIFO_COUNT > 1
uint16_t tud_audio_n_write      (uint8_t itf, uint8_t channelId, const void * data, uint16_t len);
#else
uint16_t tud_audio_n_write      (uint8_t itf, const void * data, uint16_t len);
#endif
uint16_t tud_audio_n_write_flush(uint8_t itf);
#endif

#if CFG_TUD_AUDIO_INT_CTR_EPSIZE_IN > 0
uint16_t    tud_audio_int_ctr_n_available   (uint8_t itf);
uint16_t    tud_audio_int_ctr_n_read        (uint8_t itf, void* buffer, uint16_t bufsize);
void        tud_audio_int_ctr_n_read_flush  (uint8_t itf);
uint16_t    tud_audio_int_ctr_n_write       (uint8_t itf, uint8_t const* buffer, uint16_t bufsize);
#endif

//--------------------------------------------------------------------+
// Application API (Interface0)
//--------------------------------------------------------------------+

static inline bool         tud_audio_mounted    (void);

#if CFG_TUD_AUDIO_EPSIZE_OUT && CFG_TUD_AUDIO_RX_FIFO_SIZE
static inline uint16_t     tud_audio_available  (void);
static inline uint16_t     tud_audio_read       (void* buffer, uint16_t bufsize);
static inline void         tud_audio_read_flush (void);
#endif

#if CFG_TUD_AUDIO_EPSIZE_IN && CFG_TUD_AUDIO_TX_FIFO_SIZE
#if CFG_TUD_AUDIO_TX_FIFO_COUNT > 1
static inline uint16_t tud_audio_write      (uint8_t channelId, uint8_t const* buffer, uint16_t bufsize);
#else
static inline uint16_t tud_audio_write      (uint8_t const* buffer, uint16_t bufsize);
#endif
#endif

#if CFG_TUD_AUDIO_INT_CTR_EPSIZE_IN > 0
static inline uint32_t     tud_audio_int_ctr_available     (void);
static inline uint32_t     tud_audio_int_ctr_read          (void* buffer, uint32_t bufsize);
static inline void         tud_audio_int_ctr_read_flush    (void);
static inline uint32_t     tud_audio_int_ctr_write         (uint8_t const* buffer, uint32_t bufsize);
#endif

// Buffer control EP data and schedule a transmit
// This function is intended to be used if you do not have a persistent buffer or memory location available (e.g. non-local variables) and need to answer onto a
// get request. This function buffers your answer request frame into the control buffer of the corresponding audio driver and schedules a transmit for sending it.
// Since transmission is triggered via interrupts, a persistent memory location is required onto which the buffer pointer in pointing. If you already have such
// available you may directly use 'tud_control_xfer(...)'. In this case data does not need to be copied into an additional buffer and you save some time.
// If the request's wLength is zero, a status packet is sent instead.
bool tud_audio_buffer_and_schedule_control_xfer(uint8_t rhport, tusb_control_request_t const * p_request, void* data, uint16_t len);

//--------------------------------------------------------------------+
// Application Callback API (weak is optional)
//--------------------------------------------------------------------+

#if CFG_TUD_AUDIO_EPSIZE_IN
TU_ATTR_WEAK bool tud_audio_tx_done_pre_load_cb(uint8_t rhport, uint8_t itf, uint8_t ep_in, uint8_t cur_alt_setting);
TU_ATTR_WEAK bool tud_audio_tx_done_post_load_cb(uint8_t rhport, uint16_t n_bytes_copied, uint8_t itf, uint8_t ep_in, uint8_t cur_alt_setting);
#endif

#if CFG_TUD_AUDIO_EPSIZE_OUT
TU_ATTR_WEAK bool tud_audio_rx_done_cb(uint8_t rhport, uint8_t * buffer, uint16_t bufsize);
#endif

#if CFG_TUD_AUDIO_EPSIZE_OUT > 0 && CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
TU_ATTR_WEAK bool tud_audio_fb_done_cb(uint8_t rhport);
// User code should call this function with feedback value in 16.16 format for FS and HS.
// Value will be corrected for FS to 10.14 format automatically.
// (see Universal Serial Bus Specification Revision 2.0 5.12.4.2).
// Feedback value will be sent at FB endpoint interval till it's changed.
bool tud_audio_fb_set(uint8_t rhport, uint32_t feedback);
#endif

#if CFG_TUD_AUDIO_INT_CTR_EPSIZE_IN
TU_ATTR_WEAK bool tud_audio_int_ctr_done_cb(uint8_t rhport, uint16_t * n_bytes_copied);
#endif

// Invoked when audio set interface request received
TU_ATTR_WEAK bool tud_audio_set_itf_cb(uint8_t rhport, tusb_control_request_t const * p_request);

// Invoked when audio set interface request received which closes an EP
TU_ATTR_WEAK bool tud_audio_set_itf_close_EP_cb(uint8_t rhport, tusb_control_request_t const * p_request);

// Invoked when audio class specific set request received for an EP
TU_ATTR_WEAK bool tud_audio_set_req_ep_cb(uint8_t rhport, tusb_control_request_t const * p_request, uint8_t *pBuff);

// Invoked when audio class specific set request received for an interface
TU_ATTR_WEAK bool tud_audio_set_req_itf_cb(uint8_t rhport, tusb_control_request_t const * p_request, uint8_t *pBuff);

// Invoked when audio class specific set request received for an entity
TU_ATTR_WEAK bool tud_audio_set_req_entity_cb(uint8_t rhport, tusb_control_request_t const * p_request, uint8_t *pBuff);

// Invoked when audio class specific get request received for an EP
TU_ATTR_WEAK bool tud_audio_get_req_ep_cb(uint8_t rhport, tusb_control_request_t const * p_request);

// Invoked when audio class specific get request received for an interface
TU_ATTR_WEAK bool tud_audio_get_req_itf_cb(uint8_t rhport, tusb_control_request_t const * p_request);

// Invoked when audio class specific get request received for an entity
TU_ATTR_WEAK bool tud_audio_get_req_entity_cb(uint8_t rhport, tusb_control_request_t const * p_request);

//--------------------------------------------------------------------+
// Inline Functions
//--------------------------------------------------------------------+

static inline bool tud_audio_mounted(void)
{
  return tud_audio_n_mounted(0);
}

#if CFG_TUD_AUDIO_EPSIZE_IN
#if CFG_TUD_AUDIO_TX_FIFO_SIZE && CFG_TUD_AUDIO_TX_FIFO_COUNT > 1
static inline uint16_t tud_audio_write (uint8_t channelId, uint8_t const* buffer, uint16_t n_bytes)    // Short version if only one audio function is used
{
  return tud_audio_n_write(0, channelId, buffer, n_bytes);
}
#else
static inline uint16_t tud_audio_write (uint8_t const* buffer, uint16_t n_bytes)    // Short version if only one audio function is used
{
  return tud_audio_n_write(0, buffer, n_bytes);
}
#endif

static inline uint16_t tud_audio_write_flush (void)    // Short version if only one audio function is used
{
#if CFG_TUD_AUDIO_TX_FIFO_SIZE
  return tud_audio_n_write_flush(0);
#else
  return 0;
#endif
}
#endif  // CFG_TUD_AUDIO_EPSIZE_IN && CFG_TUD_AUDIO_TX_FIFO_SIZE

#if CFG_TUD_AUDIO_EPSIZE_OUT && CFG_TUD_AUDIO_RX_FIFO_SIZE
#if CFG_TUD_AUDIO_RX_FIFO_COUNT > 1
static inline uint16_t tud_audio_available(uint8_t channelId)
{
  return tud_audio_n_available(0, channelId);
}

static inline uint16_t tud_audio_read(uint8_t channelId, void* buffer, uint16_t bufsize)
{
  return tud_audio_n_read(0, channelId, buffer, bufsize);
}

static inline void tud_audio_read_flush(uint8_t channelId)
{
  tud_audio_n_read_flush(0, channelId);
}
#else
static inline uint16_t tud_audio_available(void)
{
  return tud_audio_n_available(0);
}

static inline uint16_t tud_audio_read(void *buffer, uint16_t bufsize)
{
  return tud_audio_n_read(0, buffer, bufsize);
}

static inline void tud_audio_read_flush(void)
{
  tud_audio_n_read_flush(0);
}
#endif
#endif

#if CFG_TUD_AUDIO_INT_CTR_EPSIZE_IN > 0
static inline uint16_t tud_audio_int_ctr_available(void)
{
  return tud_audio_int_ctr_n_available(0);
}

static inline uint16_t tud_audio_int_ctr_read(void* buffer, uint16_t bufsize)
{
  return tud_audio_int_ctr_n_read(0, buffer, bufsize);
}

static inline void tud_audio_int_ctr_read_flush(void)
{
  return tud_audio_int_ctr_n_read_flush(0);
}

static inline uint16_t tud_audio_int_ctr_write(uint8_t const* buffer, uint16_t bufsize)
{
  return tud_audio_int_ctr_n_write(0, buffer, bufsize);
}
#endif

//--------------------------------------------------------------------+
// Internal Class Driver API
//--------------------------------------------------------------------+
void audiod_init             (void);
void audiod_reset            (uint8_t rhport);
uint16_t audiod_open         (uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t max_len);
bool audiod_control_request  (uint8_t rhport, tusb_control_request_t const * request);
bool audiod_control_complete (uint8_t rhport, tusb_control_request_t const * request);
bool audiod_xfer_cb          (uint8_t rhport, uint8_t edpt_addr, xfer_result_t result, uint32_t xferred_bytes);

#ifdef __cplusplus
}
#endif

#endif /* _TUSB_AUDIO_DEVICE_H_ */

/** @} */
/** @} */
