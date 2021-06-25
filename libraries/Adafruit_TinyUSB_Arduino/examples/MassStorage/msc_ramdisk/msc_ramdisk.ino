/*********************************************************************
 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
 Copyright (c) 2019 Ha Thach for Adafruit Industries
 All text above, and the splash screen below must be included in
 any redistribution
*********************************************************************/

#include "Adafruit_TinyUSB.h"

// 8KB is the smallest size that windows allow to mount
#define DISK_BLOCK_NUM  16
#define DISK_BLOCK_SIZE 512
#include "ramdisk.h"

Adafruit_USBD_MSC usb_msc;

// the setup function runs once when you press reset or power the board
void setup()
{
  // Set disk vendor id, product id and revision with string up to 8, 16, 4 characters respectively
  usb_msc.setID("Adafruit", "Mass Storage", "1.0");
  
  // Set disk size
  usb_msc.setCapacity(DISK_BLOCK_NUM, DISK_BLOCK_SIZE);

  // Set callback
  usb_msc.setReadWriteCallback(msc_read_cb, msc_write_cb, msc_flush_cb);

  // Set Lun ready (RAM disk is always ready)
  usb_msc.setUnitReady(true);
  
  usb_msc.begin();

  Serial.begin(115200);
  while ( !Serial ) delay(10);   // wait for native usb

  Serial.println("Adafruit TinyUSB Mass Storage RAM Disk example");
}

void loop()
{
  // nothing to do
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
