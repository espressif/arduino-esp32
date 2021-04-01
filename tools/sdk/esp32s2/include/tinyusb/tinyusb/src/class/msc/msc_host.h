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

#ifndef _TUSB_MSC_HOST_H_
#define _TUSB_MSC_HOST_H_

#include "common/tusb_common.h"
#include "host/usbh.h"
#include "msc.h"

#ifdef __cplusplus
 extern "C" {
#endif

/** \addtogroup ClassDriver_MSC
 *  @{
 * \defgroup MSC_Host Host
 *  The interface API includes status checking function, data transferring function and callback functions
 *  @{ */

typedef bool (*tuh_msc_complete_cb_t)(uint8_t dev_addr, msc_cbw_t const* cbw, msc_csw_t const* csw);

//--------------------------------------------------------------------+
// Application API
//--------------------------------------------------------------------+

// Check if device supports MassStorage interface.
// This function true after tuh_msc_mounted_cb() and false after tuh_msc_unmounted_cb()
bool tuh_msc_mounted(uint8_t dev_addr);

/** \brief      Check if the interface is currently busy or not
 * \param[in]   dev_addr device address
 * \retval      true if the interface is busy meaning the stack is still transferring/waiting data from/to device
 * \retval      false if the interface is not busy meaning the stack successfully transferred data from/to device
 * \note        This function is used to check if previous transfer is complete (success or error), so that the next transfer
 *              can be scheduled. User needs to make sure the corresponding interface is mounted (by \ref tuh_msc_is_mounted)
 *              before calling this function
 */
bool          tuh_msc_is_busy(uint8_t dev_addr);

// Get Max Lun
uint8_t tuh_msc_get_maxlun(uint8_t dev_addr);

// Carry out a full SCSI command (cbw, data, csw) in non-blocking manner.
// `complete_cb` callback is invoked when SCSI op is complete.
// return true if success, false if there is already pending operation.
bool tuh_msc_scsi_command(uint8_t dev_addr, msc_cbw_t const* cbw, void* data, tuh_msc_complete_cb_t complete_cb);

// Carry out SCSI INQUIRY command in non-blocking manner.
bool tuh_msc_scsi_inquiry(uint8_t dev_addr, uint8_t lun, scsi_inquiry_resp_t* response, tuh_msc_complete_cb_t complete_cb);

// Carry out SCSI REQUEST SENSE (10) command in non-blocking manner.
bool tuh_msc_test_unit_ready(uint8_t dev_addr, uint8_t lun, tuh_msc_complete_cb_t complete_cb);

// Carry out SCSI REQUEST SENSE (10) command in non-blocking manner.
bool tuh_msc_request_sense(uint8_t dev_addr, uint8_t lun, void *resposne, tuh_msc_complete_cb_t complete_cb);

// Carry out SCSI READ CAPACITY (10) command in non-blocking manner.
bool tuh_msc_read_capacity(uint8_t dev_addr, uint8_t lun, scsi_read_capacity10_resp_t* response, tuh_msc_complete_cb_t complete_cb);

#if 0
/** \brief 			Perform SCSI READ 10 command to read data from MassStorage device
 * \param[in]		dev_addr	device address
 * \param[in]		lun       Targeted Logical Unit
 * \param[out]	p_buffer  Buffer used to store data read from device. Must be accessible by USB controller (see \ref CFG_TUSB_MEM_SECTION)
 * \param[in]		lba       Starting Logical Block Address to be read
 * \param[in]		block_count Number of Block to be read
 * \retval      TUSB_ERROR_NONE on success
 * \retval      TUSB_ERROR_INTERFACE_IS_BUSY if the interface is already transferring data with device
 * \retval      TUSB_ERROR_DEVICE_NOT_READY if device is not yet configured (by SET CONFIGURED request)
 * \retval      TUSB_ERROR_INVALID_PARA if input parameters are not correct
 * \note        This function is non-blocking and returns immediately. The result of USB transfer will be reported by the interface's callback function
 */
tusb_error_t tuh_msc_read10 (uint8_t dev_addr, uint8_t lun, void * p_buffer, uint32_t lba, uint16_t block_count);

/** \brief 			Perform SCSI WRITE 10 command to write data to MassStorage device
 * \param[in]		dev_addr	device address
 * \param[in]		lun       Targeted Logical Unit
 * \param[in]	  p_buffer  Buffer containing data. Must be accessible by USB controller (see \ref CFG_TUSB_MEM_SECTION)
 * \param[in]		lba       Starting Logical Block Address to be written
 * \param[in]		block_count Number of Block to be written
 * \retval      TUSB_ERROR_NONE on success
 * \retval      TUSB_ERROR_INTERFACE_IS_BUSY if the interface is already transferring data with device
 * \retval      TUSB_ERROR_DEVICE_NOT_READY if device is not yet configured (by SET CONFIGURED request)
 * \retval      TUSB_ERROR_INVALID_PARA if input parameters are not correct
 * \note        This function is non-blocking and returns immediately. The result of USB transfer will be reported by the interface's callback function
 */
tusb_error_t tuh_msc_write10(uint8_t dev_addr, uint8_t lun, void const * p_buffer, uint32_t lba, uint16_t block_count);
#endif

//------------- Application Callback -------------//

// Invoked when a device with MassStorage interface is mounted
void tuh_msc_mounted_cb(uint8_t dev_addr);

// Invoked when a device with MassStorage interface is unmounted
void tuh_msc_unmounted_cb(uint8_t dev_addr);

//--------------------------------------------------------------------+
// Internal Class Driver API
//--------------------------------------------------------------------+

void msch_init(void);
bool msch_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_interface_t const *itf_desc, uint16_t *p_length);
bool msch_set_config(uint8_t dev_addr, uint8_t itf_num);
bool msch_xfer_cb(uint8_t dev_addr, uint8_t ep_addr, xfer_result_t event, uint32_t xferred_bytes);
void msch_close(uint8_t dev_addr);

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_MSC_HOST_H_ */

/// @}
/// @}
