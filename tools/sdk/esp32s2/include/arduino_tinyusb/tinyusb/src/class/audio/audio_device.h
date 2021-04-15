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

// All sizes are in bytes!

#ifndef CFG_TUD_AUDIO_FUNC_1_DESC_LEN
#error You must tell the driver the length of the audio function descriptor including IAD descriptor
#endif
#if CFG_TUD_AUDIO > 1
#ifndef CFG_TUD_AUDIO_FUNC_2_DESC_LEN
#error You must tell the driver the length of the audio function descriptor including IAD descriptor
#endif
#endif
#if CFG_TUD_AUDIO > 2
#ifndef CFG_TUD_AUDIO_FUNC_3_DESC_LEN
#error You must tell the driver the length of the audio function descriptor including IAD descriptor
#endif
#endif

// Number of Standard AS Interface Descriptors (4.9.1) defined per audio function - this is required to be able to remember the current alternate settings of these interfaces
#ifndef CFG_TUD_AUDIO_FUNC_1_N_AS_INT
#error You must tell the driver the number of Standard AS Interface Descriptors you have defined in the audio function descriptor!
#endif
#if CFG_TUD_AUDIO > 1
#ifndef CFG_TUD_AUDIO_FUNC_2_N_AS_INT
#error You must tell the driver the number of Standard AS Interface Descriptors you have defined in the audio function descriptor!
#endif
#endif
#if CFG_TUD_AUDIO > 2
#ifndef CFG_TUD_AUDIO_FUNC_3_N_AS_INT
#error You must tell the driver the number of Standard AS Interface Descriptors you have defined in the audio function descriptor!
#endif
#endif

// Size of control buffer used to receive and send control messages via EP0 - has to be big enough to hold your biggest request structure e.g. range requests with multiple intervals defined or cluster descriptors
#ifndef CFG_TUD_AUDIO_FUNC_1_CTRL_BUF_SZ
#error You must define an audio class control request buffer size!
#endif

#if CFG_TUD_AUDIO > 1
#ifndef CFG_TUD_AUDIO_FUNC_2_CTRL_BUF_SZ
#error You must define an audio class control request buffer size!
#endif
#endif

#if CFG_TUD_AUDIO > 2
#ifndef CFG_TUD_AUDIO_FUNC_3_CTRL_BUF_SZ
#error You must define an audio class control request buffer size!
#endif
#endif

// End point sizes IN BYTES - Limits: Full Speed <= 1023, High Speed <= 1024
#ifndef CFG_TUD_AUDIO_ENABLE_EP_IN
#define CFG_TUD_AUDIO_ENABLE_EP_IN 0   // TX
#endif

#ifndef CFG_TUD_AUDIO_ENABLE_EP_OUT
#define CFG_TUD_AUDIO_ENABLE_EP_OUT 0  // RX
#endif

// Maximum EP sizes for all alternate AS interface settings - used for checks and buffer allocation
#if CFG_TUD_AUDIO_ENABLE_EP_IN
#ifndef CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX
#error You must tell the driver the biggest EP IN size!
#endif
#if CFG_TUD_AUDIO > 1
#ifndef CFG_TUD_AUDIO_FUNC_2_EP_IN_SZ_MAX
#error You must tell the driver the biggest EP IN size!
#endif
#endif
#if CFG_TUD_AUDIO > 2
#ifndef CFG_TUD_AUDIO_FUNC_3_EP_IN_SZ_MAX
#error You must tell the driver the biggest EP IN size!
#endif
#endif
#endif // CFG_TUD_AUDIO_ENABLE_EP_IN

#if CFG_TUD_AUDIO_ENABLE_EP_OUT
#ifndef CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX
#error You must tell the driver the biggest EP OUT size!
#endif
#if CFG_TUD_AUDIO > 1
#ifndef CFG_TUD_AUDIO_FUNC_2_EP_OUT_SZ_MAX
#error You must tell the driver the biggest EP OUT size!
#endif
#endif
#if CFG_TUD_AUDIO > 2
#ifndef CFG_TUD_AUDIO_FUNC_3_EP_OUT_SZ_MAX
#error You must tell the driver the biggest EP OUT size!
#endif
#endif
#endif // CFG_TUD_AUDIO_ENABLE_EP_OUT

// Software EP FIFO buffer sizes - must be >= max EP SIZEs!
#ifndef CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ                0
#endif
#ifndef CFG_TUD_AUDIO_FUNC_2_EP_IN_SW_BUF_SZ
#define CFG_TUD_AUDIO_FUNC_2_EP_IN_SW_BUF_SZ                0
#endif
#ifndef CFG_TUD_AUDIO_FUNC_3_EP_IN_SW_BUF_SZ
#define CFG_TUD_AUDIO_FUNC_3_EP_IN_SW_BUF_SZ                0
#endif

#ifndef CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ               0
#endif
#ifndef CFG_TUD_AUDIO_FUNC_2_EP_OUT_SW_BUF_SZ
#define CFG_TUD_AUDIO_FUNC_2_EP_OUT_SW_BUF_SZ               0
#endif
#ifndef CFG_TUD_AUDIO_FUNC_3_EP_OUT_SW_BUF_SZ
#define CFG_TUD_AUDIO_FUNC_3_EP_OUT_SW_BUF_SZ               0
#endif

#if CFG_TUD_AUDIO_ENABLE_EP_IN
#if CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ < CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX
#error EP software buffer size MUST BE at least as big as maximum EP size
#endif

#if CFG_TUD_AUDIO > 1
#if CFG_TUD_AUDIO_FUNC_2_EP_IN_SW_BUF_SZ < CFG_TUD_AUDIO_FUNC_2_EP_IN_SZ_MAX
#error EP software buffer size MUST BE at least as big as maximum EP size
#endif
#endif

#if CFG_TUD_AUDIO > 2
#if CFG_TUD_AUDIO_FUNC_3_EP_IN_SW_BUF_SZ < CFG_TUD_AUDIO_FUNC_3_EP_IN_SZ_MAX
#error EP software buffer size MUST BE at least as big as maximum EP size
#endif
#endif
#endif

#if CFG_TUD_AUDIO_ENABLE_EP_OUT
#if CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ < CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX
#error EP software buffer size MUST BE at least as big as maximum EP size
#endif

#if CFG_TUD_AUDIO > 1
#if CFG_TUD_AUDIO_FUNC_2_EP_OUT_SW_BUF_SZ < CFG_TUD_AUDIO_FUNC_2_EP_OUT_SZ_MAX
#error EP software buffer size MUST BE at least as big as maximum EP size
#endif
#endif

#if CFG_TUD_AUDIO > 2
#if CFG_TUD_AUDIO_FUNC_3_EP_OUT_SW_BUF_SZ < CFG_TUD_AUDIO_FUNC_3_EP_OUT_SZ_MAX
#error EP software buffer size MUST BE at least as big as maximum EP size
#endif
#endif
#endif

// Enable/disable feedback EP (required for asynchronous RX applications)
#ifndef CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
#define CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP                    0                             // Feedback - 0 or 1
#endif

// Audio interrupt control EP size - disabled if 0
#ifndef CFG_TUD_AUDIO_INT_CTR_EPSIZE_IN
#define CFG_TUD_AUDIO_INT_CTR_EPSIZE_IN                     0                             // Audio interrupt control - if required - 6 Bytes according to UAC 2 specification (p. 74)
#endif

#ifndef CFG_TUD_AUDIO_INT_CTR_EP_IN_SW_BUFFER_SIZE
#define CFG_TUD_AUDIO_INT_CTR_EP_IN_SW_BUFFER_SIZE          6                             // Buffer size of audio control interrupt EP - 6 Bytes according to UAC 2 specification (p. 74)
#endif

// Use software encoding/decoding

// The software coding feature of the driver is not mandatory. It is useful if, for instance, you have two I2S streams which need to be interleaved
// into a single PCM stream as SAMPLE_1 | SAMPLE_2 | SAMPLE_3 | SAMPLE_4.
//
// Currently, only PCM type I encoding/decoding is supported!
//
// If the coding feature is to be used, support FIFOs need to be configured. Their sizes and numbers are defined below.

// Encoding/decoding is done in software and thus time consuming. If you can encode/decode your stream more efficiently do not use the
// support FIFOs but write/read directly into/from the EP_X_SW_BUFFER_FIFOs using
// - tud_audio_n_write() or
// - tud_audio_n_read().
// To write/read to/from the support FIFOs use
// - tud_audio_n_write_support_ff() or
// - tud_audio_n_read_support_ff().
//
// The encoding/decoding format type done is defined below.
//
// The encoding/decoding starts when the private callback functions
// - audio_tx_done_cb()
// - audio_rx_done_cb()
// are invoked. If support FIFOs are used, the corresponding encoding/decoding functions are called from there.
// Once encoding/decoding is done the result is put directly into the EP_X_SW_BUFFER_FIFOs. You can use the public callback functions
// - tud_audio_tx_done_pre_load_cb() or tud_audio_tx_done_post_load_cb()
// - tud_audio_rx_done_pre_read_cb() or tud_audio_rx_done_post_read_cb()
// if you want to get informed what happened.
//
// If you don't use the support FIFOs you may use the public callback functions
// - tud_audio_tx_done_pre_load_cb() or tud_audio_tx_done_post_load_cb()
// - tud_audio_rx_done_pre_read_cb() or tud_audio_rx_done_post_read_cb()
// to write/read from/into the EP_X_SW_BUFFER_FIFOs at the right time.
//
// If you need a different encoding which is not support so far implement it in the
// - audio_tx_done_cb()
// - audio_rx_done_cb()
// functions.

// Enable encoding/decodings - for these to work, support FIFOs need to be setup in appropriate numbers and size
// The actual coding parameters of active AS alternate interface is parsed from the descriptors

// The item size of the FIFO is always fixed to one i.e. bytes! Furthermore, the actively used FIFO depth is reconfigured such that the depth is a multiple of the current sample size in order to avoid samples to get split up in case of a wrap in the FIFO ring buffer (depth = (max_depth / sampe_sz) * sampe_sz)!
// This is important to remind in case you use DMAs! If the sample sizes changes, the DMA MUST BE RECONFIGURED just like the FIFOs for a different depth!!!

// For PCM encoding/decoding

#ifndef CFG_TUD_AUDIO_ENABLE_ENCODING
#define CFG_TUD_AUDIO_ENABLE_ENCODING                       0
#endif

#ifndef CFG_TUD_AUDIO_ENABLE_DECODING
#define CFG_TUD_AUDIO_ENABLE_DECODING                       0
#endif

// This enabling allows to save the current coding parameters e.g. # of bytes per sample etc. - TYPE_I includes common PCM encoding
#ifndef CFG_TUD_AUDIO_ENABLE_TYPE_I_ENCODING
#define CFG_TUD_AUDIO_ENABLE_TYPE_I_ENCODING                0
#endif

#ifndef CFG_TUD_AUDIO_ENABLE_TYPE_I_DECODING
#define CFG_TUD_AUDIO_ENABLE_TYPE_I_DECODING                0
#endif

// Type I Coding parameters not given within UAC2 descriptors
// It would be possible to allow for a more flexible setting and not fix this parameter as done below. However, this is most often not needed and kept for later if really necessary. The more flexible setting could be implemented within set_interface(), however, how the values are saved per alternate setting is to be determined!
#if CFG_TUD_AUDIO_ENABLE_EP_IN && CFG_TUD_AUDIO_ENABLE_ENCODING && CFG_TUD_AUDIO_ENABLE_TYPE_I_ENCODING
#ifndef CFG_TUD_AUDIO_FUNC_1_CHANNEL_PER_FIFO_TX
#error You must tell the driver the number of channels per FIFO for the interleaved encoding! E.g. for an I2S interface having two channels, CHANNEL_PER_FIFO = 2 as the I2S stream having two channels is usually saved within one FIFO
#endif
#if CFG_TUD_AUDIO > 1
#ifndef CFG_TUD_AUDIO_FUNC_2_CHANNEL_PER_FIFO_TX
#error You must tell the driver the number of channels per FIFO for the interleaved encoding! E.g. for an I2S interface having two channels, CHANNEL_PER_FIFO = 2 as the I2S stream having two channels is usually saved within one FIFO
#endif
#endif
#if CFG_TUD_AUDIO > 2
#ifndef CFG_TUD_AUDIO_FUNC_3_CHANNEL_PER_FIFO_TX
#error You must tell the driver the number of channels per FIFO for the interleaved encoding! E.g. for an I2S interface having two channels, CHANNEL_PER_FIFO = 2 as the I2S stream having two channels is usually saved within one FIFO
#endif
#endif
#endif

#if CFG_TUD_AUDIO_ENABLE_EP_OUT && CFG_TUD_AUDIO_ENABLE_DECODING && CFG_TUD_AUDIO_ENABLE_TYPE_I_DECODING
#ifndef CFG_TUD_AUDIO_FUNC_1_CHANNEL_PER_FIFO_RX
#error You must tell the driver the number of channels per FIFO for the interleaved encoding! E.g. for an I2S interface having two channels, CHANNEL_PER_FIFO = 2 as the I2S stream having two channels is usually saved within one FIFO
#endif
#if CFG_TUD_AUDIO > 1
#ifndef CFG_TUD_AUDIO_FUNC_2_CHANNEL_PER_FIFO_RX
#error You must tell the driver the number of channels per FIFO for the interleaved encoding! E.g. for an I2S interface having two channels, CHANNEL_PER_FIFO = 2 as the I2S stream having two channels is usually saved within one FIFO
#endif
#endif
#if CFG_TUD_AUDIO > 2
#ifndef CFG_TUD_AUDIO_FUNC_3_CHANNEL_PER_FIFO_RX
#error You must tell the driver the number of channels per FIFO for the interleaved encoding! E.g. for an I2S interface having two channels, CHANNEL_PER_FIFO = 2 as the I2S stream having two channels is usually saved within one FIFO
#endif
#endif
#endif

// Remaining types not support so far

// Number of support FIFOs to set up - multiple channels can be handled by one FIFO - very common is two channels per FIFO stemming from one I2S interface
#ifndef CFG_TUD_AUDIO_FUNC_1_N_TX_SUPP_SW_FIFO
#define CFG_TUD_AUDIO_FUNC_1_N_TX_SUPP_SW_FIFO              0
#endif
#ifndef CFG_TUD_AUDIO_FUNC_2_N_TX_SUPP_SW_FIFO
#define CFG_TUD_AUDIO_FUNC_2_N_TX_SUPP_SW_FIFO              0
#endif
#ifndef CFG_TUD_AUDIO_FUNC_3_N_TX_SUPP_SW_FIFO
#define CFG_TUD_AUDIO_FUNC_3_N_TX_SUPP_SW_FIFO              0
#endif

#ifndef CFG_TUD_AUDIO_FUNC_1_N_RX_SUPP_SW_FIFO
#define CFG_TUD_AUDIO_FUNC_1_N_RX_SUPP_SW_FIFO              0
#endif
#ifndef CFG_TUD_AUDIO_FUNC_2_N_RX_SUPP_SW_FIFO
#define CFG_TUD_AUDIO_FUNC_2_N_RX_SUPP_SW_FIFO              0
#endif
#ifndef CFG_TUD_AUDIO_FUNC_3_N_RX_SUPP_SW_FIFO
#define CFG_TUD_AUDIO_FUNC_3_N_RX_SUPP_SW_FIFO              0
#endif

// Size of support FIFOs IN BYTES - if size > 0 there are as many FIFOs set up as CFG_TUD_AUDIO_FUNC_X_N_TX_SUPP_SW_FIFO and CFG_TUD_AUDIO_FUNC_X_N_RX_SUPP_SW_FIFO
#ifndef CFG_TUD_AUDIO_FUNC_1_TX_SUPP_SW_FIFO_SZ
#define CFG_TUD_AUDIO_FUNC_1_TX_SUPP_SW_FIFO_SZ             0         // FIFO size - minimum size: ceil(f_s/1000) * max(# of TX channels) / (# of TX support FIFOs) * max(# of bytes per sample)
#endif
#ifndef CFG_TUD_AUDIO_FUNC_2_TX_SUPP_SW_FIFO_SZ
#define CFG_TUD_AUDIO_FUNC_2_TX_SUPP_SW_FIFO_SZ             0
#endif
#ifndef CFG_TUD_AUDIO_FUNC_3_TX_SUPP_SW_FIFO_SZ
#define CFG_TUD_AUDIO_FUNC_3_TX_SUPP_SW_FIFO_SZ             0
#endif

#ifndef CFG_TUD_AUDIO_FUNC_1_RX_SUPP_SW_FIFO_SZ
#define CFG_TUD_AUDIO_FUNC_1_RX_SUPP_SW_FIFO_SZ             0         // FIFO size - minimum size: ceil(f_s/1000) * max(# of RX channels) / (# of RX support FIFOs) * max(# of bytes per sample)
#endif
#ifndef CFG_TUD_AUDIO_FUNC_2_RX_SUPP_SW_FIFO_SZ
#define CFG_TUD_AUDIO_FUNC_2_RX_SUPP_SW_FIFO_SZ             0
#endif
#ifndef CFG_TUD_AUDIO_FUNC_3_RX_SUPP_SW_FIFO_SZ
#define CFG_TUD_AUDIO_FUNC_3_RX_SUPP_SW_FIFO_SZ             0
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

#if CFG_TUD_AUDIO_ENABLE_EP_OUT && !CFG_TUD_AUDIO_ENABLE_DECODING
uint16_t tud_audio_n_available                    (uint8_t itf);
uint16_t tud_audio_n_read                         (uint8_t itf, void* buffer, uint16_t bufsize);
bool     tud_audio_n_clear_ep_out_ff              (uint8_t itf);                          // Delete all content in the EP OUT FIFO
#endif

#if CFG_TUD_AUDIO_ENABLE_EP_OUT && CFG_TUD_AUDIO_ENABLE_DECODING
bool     tud_audio_n_clear_rx_support_ff          (uint8_t itf, uint8_t channelId);       // Delete all content in the support RX FIFOs
uint16_t tud_audio_n_available_support_ff         (uint8_t itf, uint8_t channelId);
uint16_t tud_audio_n_read_support_ff              (uint8_t itf, uint8_t channelId, void* buffer, uint16_t bufsize);
#endif

#if CFG_TUD_AUDIO_ENABLE_EP_IN && !CFG_TUD_AUDIO_ENABLE_ENCODING
uint16_t tud_audio_n_write                        (uint8_t itf, const void * data, uint16_t len);
bool     tud_audio_n_clear_ep_in_ff               (uint8_t itf);                          // Delete all content in the EP IN FIFO
#endif

#if CFG_TUD_AUDIO_ENABLE_EP_IN && CFG_TUD_AUDIO_ENABLE_ENCODING
uint16_t tud_audio_n_flush_tx_support_ff          (uint8_t itf);      // Force all content in the support TX FIFOs to be written into EP SW FIFO
bool     tud_audio_n_clear_tx_support_ff          (uint8_t itf, uint8_t channelId);
uint16_t tud_audio_n_write_support_ff             (uint8_t itf, uint8_t channelId, const void * data, uint16_t len);
#endif

#if CFG_TUD_AUDIO_INT_CTR_EPSIZE_IN
uint16_t    tud_audio_int_ctr_n_write             (uint8_t itf, uint8_t const* buffer, uint16_t len);
#endif

//--------------------------------------------------------------------+
// Application API (Interface0)
//--------------------------------------------------------------------+

static inline bool         tud_audio_mounted                (void);

// RX API

#if CFG_TUD_AUDIO_ENABLE_EP_OUT && !CFG_TUD_AUDIO_ENABLE_DECODING
static inline uint16_t     tud_audio_available              (void);
static inline bool         tud_audio_clear_ep_out_ff        (void);                       // Delete all content in the EP OUT FIFO
static inline uint16_t     tud_audio_read                   (void* buffer, uint16_t bufsize);
#endif

#if CFG_TUD_AUDIO_ENABLE_EP_OUT && CFG_TUD_AUDIO_ENABLE_DECODING
static inline bool     tud_audio_clear_rx_support_ff        (uint8_t channelId);
static inline uint16_t tud_audio_available_support_ff       (uint8_t channelId);
static inline uint16_t tud_audio_read_support_ff            (uint8_t channelId, void* buffer, uint16_t bufsize);
#endif

// TX API

#if CFG_TUD_AUDIO_ENABLE_EP_IN && !CFG_TUD_AUDIO_ENABLE_ENCODING
static inline uint16_t tud_audio_write                      (const void * data, uint16_t len);
static inline bool 	   tud_audio_clear_ep_in_ff             (void);
#endif

#if CFG_TUD_AUDIO_ENABLE_EP_IN && CFG_TUD_AUDIO_ENABLE_ENCODING
static inline uint16_t tud_audio_flush_tx_support_ff        (void);
static inline uint16_t tud_audio_clear_tx_support_ff        (uint8_t channelId);
static inline uint16_t tud_audio_write_support_ff           (uint8_t channelId, const void * data, uint16_t len);
#endif

// INT CTR API

#if CFG_TUD_AUDIO_INT_CTR_EPSIZE_IN
static inline uint16_t tud_audio_int_ctr_write              (uint8_t const* buffer, uint16_t len);
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

#if CFG_TUD_AUDIO_ENABLE_EP_IN
TU_ATTR_WEAK bool tud_audio_tx_done_pre_load_cb(uint8_t rhport, uint8_t itf, uint8_t ep_in, uint8_t cur_alt_setting);
TU_ATTR_WEAK bool tud_audio_tx_done_post_load_cb(uint8_t rhport, uint16_t n_bytes_copied, uint8_t itf, uint8_t ep_in, uint8_t cur_alt_setting);
#endif

#if CFG_TUD_AUDIO_ENABLE_EP_OUT
TU_ATTR_WEAK bool tud_audio_rx_done_pre_read_cb(uint8_t rhport, uint16_t n_bytes_received, uint8_t itf, uint8_t ep_out, uint8_t cur_alt_setting);
TU_ATTR_WEAK bool tud_audio_rx_done_post_read_cb(uint8_t rhport, uint16_t n_bytes_received, uint8_t itf, uint8_t ep_out, uint8_t cur_alt_setting);
#endif

#if CFG_TUD_AUDIO_ENABLE_EP_OUT && CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
TU_ATTR_WEAK bool tud_audio_fb_done_cb(uint8_t rhport);
// User code should call this function with feedback value in 16.16 format for FS and HS.
// Value will be corrected for FS to 10.14 format automatically.
// (see Universal Serial Bus Specification Revision 2.0 5.12.4.2).
// Feedback value will be sent at FB endpoint interval till it's changed.
bool tud_audio_n_fb_set(uint8_t itf, uint32_t feedback);
static inline bool tud_audio_fb_set(uint32_t feedback);
#endif

#if CFG_TUD_AUDIO_INT_CTR_EPSIZE_IN
TU_ATTR_WEAK bool tud_audio_int_ctr_done_cb(uint8_t rhport, uint16_t n_bytes_copied);
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

// RX API

#if CFG_TUD_AUDIO_ENABLE_EP_OUT && !CFG_TUD_AUDIO_ENABLE_DECODING

static inline uint16_t tud_audio_available(void)
{
  return tud_audio_n_available(0);
}

static inline uint16_t tud_audio_read(void* buffer, uint16_t bufsize)
{
  return tud_audio_n_read(0, buffer, bufsize);
}

static inline bool tud_audio_clear_ep_out_ff(void)
{
  return tud_audio_n_clear_ep_out_ff(0);
}

#endif

#if CFG_TUD_AUDIO_ENABLE_EP_OUT && CFG_TUD_AUDIO_ENABLE_DECODING

static inline bool tud_audio_clear_rx_support_ff(uint8_t channelId)
{
  return tud_audio_n_clear_rx_support_ff(0, channelId);
}

static inline uint16_t tud_audio_available_support_ff(uint8_t channelId)
{
  return tud_audio_n_available_support_ff(0, channelId);
}

static inline uint16_t tud_audio_read_support_ff(uint8_t channelId, void* buffer, uint16_t bufsize)
{
  return tud_audio_n_read_support_ff(0, channelId, buffer, bufsize);
}

#endif

// TX API

#if CFG_TUD_AUDIO_ENABLE_EP_IN && !CFG_TUD_AUDIO_ENABLE_ENCODING

static inline uint16_t tud_audio_write(const void * data, uint16_t len)
{
  return tud_audio_n_write(0, data, len);
}

static inline bool tud_audio_clear_ep_in_ff(void)
{
  return tud_audio_n_clear_ep_in_ff(0);
}

#endif

#if CFG_TUD_AUDIO_ENABLE_EP_IN && CFG_TUD_AUDIO_ENABLE_ENCODING

static inline uint16_t tud_audio_flush_tx_support_ff(void)
{
  return tud_audio_n_flush_tx_support_ff(0);
}

static inline uint16_t tud_audio_clear_tx_support_ff(uint8_t channelId)
{
  return tud_audio_n_clear_tx_support_ff(0, channelId);
}

static inline uint16_t tud_audio_write_support_ff(uint8_t channelId, const void * data, uint16_t len)
{
  return tud_audio_n_write_support_ff(0, channelId, data, len);
}

#endif

#if CFG_TUD_AUDIO_INT_CTR_EPSIZE_IN
static inline uint16_t tud_audio_int_ctr_write(uint8_t const* buffer, uint16_t len)
{
  return tud_audio_int_ctr_n_write(0, buffer, len);
}
#endif

#if CFG_TUD_AUDIO_ENABLE_EP_OUT && CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
static inline bool tud_audio_fb_set(uint32_t feedback)
{
  return tud_audio_n_fb_set(0, feedback);
}
#endif

//--------------------------------------------------------------------+
// Internal Class Driver API
//--------------------------------------------------------------------+
void     audiod_init           (void);
void     audiod_reset          (uint8_t rhport);
uint16_t audiod_open           (uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t max_len);
bool     audiod_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request);
bool     audiod_xfer_cb        (uint8_t rhport, uint8_t edpt_addr, xfer_result_t result, uint32_t xferred_bytes);

#ifdef __cplusplus
}
#endif

#endif /* _TUSB_AUDIO_DEVICE_H_ */

/** @} */
/** @} */
