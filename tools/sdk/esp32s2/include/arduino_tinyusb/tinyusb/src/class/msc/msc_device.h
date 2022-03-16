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

#ifndef _TUSB_MSC_DEVICE_H_
#define _TUSB_MSC_DEVICE_H_

#include "common/tusb_common.h"
#include "msc.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// Class Driver Configuration
//--------------------------------------------------------------------+

#if !defined(CFG_TUD_MSC_EP_BUFSIZE) & defined(CFG_TUD_MSC_BUFSIZE)
  // TODO warn user to use new name later on
  // #warning CFG_TUD_MSC_BUFSIZE is renamed to CFG_TUD_MSC_EP_BUFSIZE, please update to use the new name
  #define CFG_TUD_MSC_EP_BUFSIZE  CFG_TUD_MSC_BUFSIZE
#endif

#ifndef CFG_TUD_MSC_EP_BUFSIZE
  #error CFG_TUD_MSC_EP_BUFSIZE must be defined, value of a block size should work well, the more the better
#endif

TU_VERIFY_STATIC(CFG_TUD_MSC_EP_BUFSIZE < UINT16_MAX, "Size is not correct");

//--------------------------------------------------------------------+
// Application API
//--------------------------------------------------------------------+

// Set SCSI sense response
bool tud_msc_set_sense(uint8_t lun, uint8_t sense_key, uint8_t add_sense_code, uint8_t add_sense_qualifier);

//--------------------------------------------------------------------+
// Application Callbacks (WEAK is optional)
//--------------------------------------------------------------------+

// Invoked when received SCSI READ10 command
// - Address = lba * BLOCK_SIZE + offset
//   - offset is only needed if CFG_TUD_MSC_EP_BUFSIZE is smaller than BLOCK_SIZE.
//
// - Application fill the buffer (up to bufsize) with address contents and return number of read byte. If
//   - read < bufsize : These bytes are transferred first and callback invoked again for remaining data.
//
//   - read == 0      : Indicate application is not ready yet e.g disk I/O busy.
//                      Callback invoked again with the same parameters later on.
//
//   - read < 0       : Indicate application error e.g invalid address. This request will be STALLed
//                      and return failed status in command status wrapper phase.
int32_t tud_msc_read10_cb (uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize);

// Invoked when received SCSI WRITE10 command
// - Address = lba * BLOCK_SIZE + offset
//   - offset is only needed if CFG_TUD_MSC_EP_BUFSIZE is smaller than BLOCK_SIZE.
//
// - Application write data from buffer to address contents (up to bufsize) and return number of written byte. If
//   - write < bufsize : callback invoked again with remaining data later on.
//
//   - write == 0      : Indicate application is not ready yet e.g disk I/O busy.
//                       Callback invoked again with the same parameters later on.
//
//   - write < 0       : Indicate application error e.g invalid address. This request will be STALLed
//                       and return failed status in command status wrapper phase.
//
// TODO change buffer to const uint8_t*
int32_t tud_msc_write10_cb (uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize);

// Invoked when received SCSI_CMD_INQUIRY
// Application fill vendor id, product id and revision with string up to 8, 16, 4 characters respectively
void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4]);

// Invoked when received Test Unit Ready command.
// return true allowing host to read/write this LUN e.g SD card inserted
bool tud_msc_test_unit_ready_cb(uint8_t lun);

// Invoked when received SCSI_CMD_READ_CAPACITY_10 and SCSI_CMD_READ_FORMAT_CAPACITY to determine the disk size
// Application update block count and block size
void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size);

/**
 * Invoked when received an SCSI command not in built-in list below.
 * - READ_CAPACITY10, READ_FORMAT_CAPACITY, INQUIRY, TEST_UNIT_READY, START_STOP_UNIT, MODE_SENSE6, REQUEST_SENSE
 * - READ10 and WRITE10 has their own callbacks
 *
 * \param[in]   lun         Logical unit number
 * \param[in]   scsi_cmd    SCSI command contents which application must examine to response accordingly
 * \param[out]  buffer      Buffer for SCSI Data Stage.
 *                            - For INPUT: application must fill this with response.
 *                            - For OUTPUT it holds the Data from host
 * \param[in]   bufsize     Buffer's length.
 *
 * \return      Actual bytes processed, can be zero for no-data command.
 * \retval      negative    Indicate error e.g unsupported command, tinyusb will \b STALL the corresponding
 *                          endpoint and return failed status in command status wrapper phase.
 */
int32_t tud_msc_scsi_cb (uint8_t lun, uint8_t const scsi_cmd[16], void* buffer, uint16_t bufsize);

/*------------- Optional callbacks -------------*/

// Invoked when received GET_MAX_LUN request, required for multiple LUNs implementation
TU_ATTR_WEAK uint8_t tud_msc_get_maxlun_cb(void);

// Invoked when received Start Stop Unit command
// - Start = 0 : stopped power mode, if load_eject = 1 : unload disk storage
// - Start = 1 : active mode, if load_eject = 1 : load disk storage
TU_ATTR_WEAK bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject);

// Invoked when received REQUEST_SENSE
TU_ATTR_WEAK int32_t tud_msc_request_sense_cb(uint8_t lun, void* buffer, uint16_t bufsize);

// Invoked when Read10 command is complete
TU_ATTR_WEAK void tud_msc_read10_complete_cb(uint8_t lun);

// Invoke when Write10 command is complete, can be used to flush flash caching
TU_ATTR_WEAK void tud_msc_write10_complete_cb(uint8_t lun);

// Invoked when command in tud_msc_scsi_cb is complete
TU_ATTR_WEAK void tud_msc_scsi_complete_cb(uint8_t lun, uint8_t const scsi_cmd[16]);

// Invoked to check if device is writable as part of SCSI WRITE10
TU_ATTR_WEAK bool tud_msc_is_writable_cb(uint8_t lun);

//--------------------------------------------------------------------+
// Internal Class Driver API
//--------------------------------------------------------------------+
void     mscd_init            (void);
void     mscd_reset           (uint8_t rhport);
uint16_t mscd_open            (uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t max_len);
bool     mscd_control_xfer_cb (uint8_t rhport, uint8_t stage, tusb_control_request_t const * p_request);
bool     mscd_xfer_cb         (uint8_t rhport, uint8_t ep_addr, xfer_result_t event, uint32_t xferred_bytes);

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_MSC_DEVICE_H_ */
