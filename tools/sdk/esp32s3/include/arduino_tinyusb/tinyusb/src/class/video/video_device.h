/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 * Copyright (c) 2021 Koji KITAYAMA
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

#ifndef TUSB_VIDEO_DEVICE_H_
#define TUSB_VIDEO_DEVICE_H_

#include "common/tusb_common.h"
#include "video.h"

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------+
// Application API (Multiple Ports)
// CFG_TUD_VIDEO > 1
//--------------------------------------------------------------------+

/** Return true if streaming
 *
 * @param[in] ctl_idx    Destination control interface index
 * @param[in] stm_idx    Destination streaming interface index */
bool tud_video_n_streaming(uint_fast8_t ctl_idx, uint_fast8_t stm_idx);

/** Transfer a frame
 *
 * @param[in] ctl_idx    Destination control interface index
 * @param[in] stm_idx    Destination streaming interface index
 * @param[in] buffer     Frame buffer. The caller must not use this buffer until the operation is completed.
 * @param[in] bufsize    Byte size of the frame buffer */
bool tud_video_n_frame_xfer(uint_fast8_t ctl_idx, uint_fast8_t stm_idx, void *buffer, size_t bufsize);

/*------------- Optional callbacks -------------*/
/** Invoked when compeletion of a frame transfer
 *
 * @param[in] ctl_idx    Destination control interface index
 * @param[in] stm_idx    Destination streaming interface index */
TU_ATTR_WEAK void tud_video_frame_xfer_complete_cb(uint_fast8_t ctl_idx, uint_fast8_t stm_idx);

//--------------------------------------------------------------------+
// Application Callback API (weak is optional)
//--------------------------------------------------------------------+

/** Invoked when SET_POWER_MODE request received
 *
 * @param[in] ctl_idx    Destination control interface index
 * @param[in] stm_idx    Destination streaming interface index
 * @return video_error_code_t */
TU_ATTR_WEAK int tud_video_power_mode_cb(uint_fast8_t ctl_idx, uint8_t power_mod);

/** Invoked when VS_COMMIT_CONTROL(SET_CUR) request received
 *
 * @param[in] ctl_idx     Destination control interface index
 * @param[in] stm_idx     Destination streaming interface index
 * @param[in] parameters  Video streaming parameters
 * @return video_error_code_t */
TU_ATTR_WEAK int tud_video_commit_cb(uint_fast8_t ctl_idx, uint_fast8_t stm_idx,
                                     video_probe_and_commit_control_t const *parameters);

//--------------------------------------------------------------------+
// INTERNAL USBD-CLASS DRIVER API
//--------------------------------------------------------------------+
void     videod_init           (void);
bool     videod_deinit         (void);
void     videod_reset          (uint8_t rhport);
uint16_t videod_open           (uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t max_len);
bool     videod_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request);
bool     videod_xfer_cb        (uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes);

#ifdef __cplusplus
 }
#endif

#endif
