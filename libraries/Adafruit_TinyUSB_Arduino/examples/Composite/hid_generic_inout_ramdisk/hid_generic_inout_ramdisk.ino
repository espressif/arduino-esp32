/*********************************************************************
 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
 Copyright (c) 2019 Ha Thach for Adafruit Industries
 All text above, and the splash screen below must be included in
 any redistribution
*********************************************************************/

/* This example demonstrate HID Generic raw Input & Output.
 * It will receive data from Host (In endpoint) and echo back (Out endpoint).
 * HID Report descriptor use vendor for usage page (using template TUD_HID_REPORT_DESC_GENERIC_INOUT)
 *
 * There are 2 ways to test the sketch
 * 1. Using nodejs
 *    - Install nodejs and nmp to your PC
 *    - Install execellent node-hid (https://github.com/node-hid/node-hid) by
 *      $ npm install node-hid
 *    - Run provided hid test script 
 *      $ node hid_test.js
 *    
 * 2. Using python hidRun 
 *    - Python and `hid` package is required, for installation please follow https://pypi.org/project/hid/
 *    - Run provided hid test script to send and receive data to this device.
 *      $ python3 hid_test.py
 */

#include "Adafruit_TinyUSB.h"

// 8KB is the smallest size that windows allow to mount
#define DISK_BLOCK_NUM  16
#define DISK_BLOCK_SIZE 512
#include "ramdisk.h"

// HID report descriptor using TinyUSB's template
// Generic In Out with 64 bytes report (max)
uint8_t const desc_hid_report[] =
{
  TUD_HID_REPORT_DESC_GENERIC_INOUT(64)
};

Adafruit_USBD_HID usb_hid;
Adafruit_USBD_MSC usb_msc;

// the setup function runs once when you press reset or power the board
void setup()
{
  usb_msc.setID("Adafruit", "Mass Storage", "1.0");
  
  // Set disk size
  usb_msc.setCapacity(DISK_BLOCK_NUM, DISK_BLOCK_SIZE);

  // Set callback
  usb_msc.setReadWriteCallback(msc_read_cb, msc_write_cb, msc_flush_cb);

  // Set Lun ready (RAM disk is always ready)
  usb_msc.setUnitReady(true);
  
  usb_msc.begin();
  
  usb_hid.enableOutEndpoint(true);
  usb_hid.setPollInterval(2);
  usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
  usb_hid.setReportCallback(get_report_callback, set_report_callback);

  usb_hid.begin();

  Serial.begin(115200);
  Serial.println("Waiting for USBDevice mount");
  // wait until device mounted
  while( !USBDevice.mounted() ) delay(1);

  Serial.println("Adafruit TinyUSB HID Generic In Out example");
}

void loop()
{
  // nothing to do
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t get_report_callback (uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
  // not used in this example
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) reqlen;
  return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void set_report_callback(uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
  // This example doesn't use multiple report and report ID
  (void) report_id;
  (void) report_type;

  // echo back anything we received from host
  usb_hid.sendReport(0, buffer, bufsize);
}

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and 
// return number of copied bytes (must be multiple of block size) 
int32_t msc_read_cb (uint32_t lba, void* buffer, uint32_t bufsize)
{
  uint8_t const* addr = msc_disk[lba];
  memcpy(buffer, addr, bufsize);

  return bufsize;
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and 
// return number of written bytes (must be multiple of block size)
int32_t msc_write_cb (uint32_t lba, uint8_t* buffer, uint32_t bufsize)
{
  uint8_t* addr = msc_disk[lba];
  memcpy(addr, buffer, bufsize);

  return bufsize;
}

// Callback invoked when WRITE10 command is completed (status received and accepted by host).
// used to flush any pending cache.
void msc_flush_cb (void)
{
  // nothing to do
}
