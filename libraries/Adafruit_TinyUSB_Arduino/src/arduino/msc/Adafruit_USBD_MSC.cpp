/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach for Adafruit Industries
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
 */

#include "tusb_option.h"

#if TUSB_OPT_DEVICE_ENABLED && CFG_TUD_MSC

#include "Adafruit_USBD_MSC.h"

#define EPOUT 0x00
#define EPIN 0x80
#define EPSIZE 64 // TODO must be 512 for highspeed device

static Adafruit_USBD_MSC *_msc_dev = NULL;

Adafruit_USBD_MSC::Adafruit_USBD_MSC(void) {
  _maxlun = 1;
  memset(_lun, 0, sizeof(_lun));
}

uint16_t Adafruit_USBD_MSC::getInterfaceDescriptor(uint8_t itfnum, uint8_t *buf,
                                                   uint16_t bufsize) {
  // usb core will automatically update endpoint number
  uint8_t desc[] = {TUD_MSC_DESCRIPTOR(itfnum, 0, EPOUT, EPIN, EPSIZE)};
  uint16_t const len = sizeof(desc);

  if (bufsize < len) {
    return 0;
  }

  memcpy(buf, desc, len);
  return len;
}

void Adafruit_USBD_MSC::setMaxLun(uint8_t maxlun) { _maxlun = maxlun; }

uint8_t Adafruit_USBD_MSC::getMaxLun(void) { return _maxlun; }

void Adafruit_USBD_MSC::setID(uint8_t lun, const char *vendor_id,
                              const char *product_id, const char *product_rev) {
  _lun[lun]._inquiry_vid = vendor_id;
  _lun[lun]._inquiry_pid = product_id;
  _lun[lun]._inquiry_rev = product_rev;
}

void Adafruit_USBD_MSC::setCapacity(uint8_t lun, uint32_t block_count,
                                    uint16_t block_size) {
  _lun[lun].block_count = block_count;
  _lun[lun].block_size = block_size;
}

void Adafruit_USBD_MSC::setUnitReady(uint8_t lun, bool ready) {
  _lun[lun].unit_ready = ready;
}

void Adafruit_USBD_MSC::setReadWriteCallback(uint8_t lun, read_callback_t rd_cb,
                                             write_callback_t wr_cb,
                                             flush_callback_t fl_cb) {
  _lun[lun].rd_cb = rd_cb;
  _lun[lun].wr_cb = wr_cb;
  _lun[lun].fl_cb = fl_cb;
}

void Adafruit_USBD_MSC::setReadyCallback(uint8_t lun, ready_callback_t cb) {
  _lun[lun].ready_cb = cb;
}

bool Adafruit_USBD_MSC::begin(void) {
  if (!USBDevice.addInterface(*this)) {
    return false;
  }

  _msc_dev = this;
  return true;
}

//------------- TinyUSB callbacks -------------//
extern "C" {

// Invoked to determine max LUN
uint8_t tud_msc_get_maxlun_cb(void) {
  if (!_msc_dev) {
    return 0;
  }
  return _msc_dev->getMaxLun();
}

// Invoked when received SCSI_CMD_INQUIRY
// Application fill vendor id, product id and revision with string up to 8, 16,
// 4 characters respectively
void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8],
                        uint8_t product_id[16], uint8_t product_rev[4]) {
  if (!_msc_dev) {
    return;
  }

  // If not set use default ID "Adafruit - Mass Storage - 1.0"
  const char *vid =
      (_msc_dev->_lun[lun]._inquiry_vid ? _msc_dev->_lun[lun]._inquiry_vid
                                        : "Adafruit");
  const char *pid =
      (_msc_dev->_lun[lun]._inquiry_pid ? _msc_dev->_lun[lun]._inquiry_pid
                                        : "Mass Storage");
  const char *rev =
      (_msc_dev->_lun[lun]._inquiry_rev ? _msc_dev->_lun[lun]._inquiry_rev
                                        : "1.0");

  memcpy(vendor_id, vid, tu_min32(strlen(vid), 8));
  memcpy(product_id, pid, tu_min32(strlen(pid), 16));
  memcpy(product_rev, rev, tu_min32(strlen(rev), 4));
}

// Invoked when received Test Unit Ready command.
// return true allowing host to read/write this LUN e.g SD card inserted
bool tud_msc_test_unit_ready_cb(uint8_t lun) {
  if (!_msc_dev) {
    return false;
  }

  if (_msc_dev->_lun[lun].ready_cb) {
    _msc_dev->_lun[lun].unit_ready = _msc_dev->_lun[lun].ready_cb();
  }

  return _msc_dev->_lun[lun].unit_ready;
}

// Callback invoked to determine disk's size
void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count,
                         uint16_t *block_size) {
  if (!_msc_dev) {
    return;
  }

  *block_count = _msc_dev->_lun[lun].block_count;
  *block_size = _msc_dev->_lun[lun].block_size;
}

// Callback invoked when received an SCSI command not in built-in list below
// - READ_CAPACITY10, READ_FORMAT_CAPACITY, INQUIRY, MODE_SENSE6, REQUEST_SENSE
// - READ10 and WRITE10 has their own callbacks
int32_t tud_msc_scsi_cb(uint8_t lun, const uint8_t scsi_cmd[16], void *buffer,
                        uint16_t bufsize) {
  const void *response = NULL;
  uint16_t resplen = 0;

  switch (scsi_cmd[0]) {
  case SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL:
    // Host is about to read/write etc ... better not to disconnect disk
    resplen = 0;
    break;

  default:
    // Set Sense = Invalid Command Operation
    tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);

    // negative means error -> tinyusb could stall and/or response with failed
    // status
    resplen = -1;
    break;
  }

  // return len must not larger than bufsize
  if (resplen > bufsize) {
    resplen = bufsize;
  }

  // copy response to stack's buffer if any
  if (response && resplen) {
    memcpy(buffer, response, resplen);
  }

  return resplen;
}

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and return number of copied bytes.
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                          void *buffer, uint32_t bufsize) {
  (void)offset;

  if (!(_msc_dev && _msc_dev->_lun[lun].rd_cb)) {
    return -1;
  }

  return _msc_dev->_lun[lun].rd_cb(lba, buffer, bufsize);
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and return number of written bytes
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                           uint8_t *buffer, uint32_t bufsize) {
  (void)offset;

  if (!(_msc_dev && _msc_dev->_lun[lun].wr_cb)) {
    return -1;
  }

  return _msc_dev->_lun[lun].wr_cb(lba, buffer, bufsize);
}

// Callback invoked when WRITE10 command is completed (status received and
// accepted by host). used to flush any pending cache.
void tud_msc_write10_complete_cb(uint8_t lun) {
  if (!(_msc_dev && _msc_dev->_lun[lun].fl_cb)) {
    return;
  }

  // flush pending cache when write10 is complete
  return _msc_dev->_lun[lun].fl_cb();
}

} // extern "C"

#endif // TUSB_OPT_DEVICE_ENABLED
