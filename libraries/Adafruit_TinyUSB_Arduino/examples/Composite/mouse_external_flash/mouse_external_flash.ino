/*********************************************************************
 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
 Copyright (c) 2019 Ha Thach for Adafruit Industries
 All text above, and the splash screen below must be included in
 any redistribution
*********************************************************************/

/* This sketch demonstrates USB Mass Storage and HID mouse (and CDC)
 * - Enumerated as disk using on-board external flash
 * - Press button pin will move mouse toward bottom right of monitor
 */

#include "SPI.h"
#include "SdFat.h"
#include "Adafruit_SPIFlash.h"
#include "Adafruit_TinyUSB.h"

// Uncomment to run example with FRAM
// #define FRAM_CS   A5
// #define FRAM_SPI  SPI

#if defined(FRAM_CS) && defined(FRAM_SPI)
  Adafruit_FlashTransport_SPI flashTransport(FRAM_CS, FRAM_SPI);

#else
  // On-board external flash (QSPI or SPI) macros should already
  // defined in your board variant if supported
  // - EXTERNAL_FLASH_USE_QSPI
  // - EXTERNAL_FLASH_USE_CS/EXTERNAL_FLASH_USE_SPI
  #if defined(EXTERNAL_FLASH_USE_QSPI)
    Adafruit_FlashTransport_QSPI flashTransport;

  #elif defined(EXTERNAL_FLASH_USE_SPI)
    Adafruit_FlashTransport_SPI flashTransport(EXTERNAL_FLASH_USE_CS, EXTERNAL_FLASH_USE_SPI);

  #else
    #error No QSPI/SPI flash are defined on your board variant.h !
  #endif
#endif

Adafruit_SPIFlash flash(&flashTransport);

Adafruit_USBD_MSC usb_msc;

// HID report descriptor using TinyUSB's template
// Single Report (no ID) descriptor
uint8_t const desc_hid_report[] =
{
  TUD_HID_REPORT_DESC_MOUSE()
};

Adafruit_USBD_HID usb_hid;

#if defined ARDUINO_SAMD_CIRCUITPLAYGROUND_EXPRESS
  const int pin = 4; // Left Button
  bool activeState = true;
#elif defined ARDUINO_NRF52840_FEATHER
  const int pin = 7; // UserSw
  bool activeState = false;
#else
  const int pin = 12;
  bool activeState = false;
#endif

// the setup function runs once when you press reset or power the board
void setup()
{
  flash.begin();

  pinMode(LED_BUILTIN, OUTPUT);

  // Set disk vendor id, product id and revision with string up to 8, 16, 4 characters respectively
  usb_msc.setID("Adafruit", "External Flash", "1.0");

  // Set callback
  usb_msc.setReadWriteCallback(msc_read_cb, msc_write_cb, msc_flush_cb);

  // Set disk size, block size should be 512 regardless of spi flash page size
  usb_msc.setCapacity(flash.size()/512, 512);

  // MSC is ready for read/write
  usb_msc.setUnitReady(true);
  
  usb_msc.begin();


  // Set up button
  pinMode(pin, activeState ? INPUT_PULLDOWN : INPUT_PULLUP);

  usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
  usb_hid.begin();

  Serial.begin(115200);
  //while ( !Serial ) delay(10);   // wait for native usb

  Serial.println("Adafruit TinyUSB Mouse + Mass Storage (external flash) example");
}

void loop()
{
  // poll gpio once each 10 ms
  delay(10);

  // button is active low
  uint32_t const btn = (digitalRead(pin) == activeState);

  // Remote wakeup
  if ( USBDevice.suspended() && btn )
  {
    // Wake up host if we are in suspend mode
    // and REMOTE_WAKEUP feature is enabled by host
    tud_remote_wakeup();
  }

  /*------------- Mouse -------------*/
  if ( usb_hid.ready() )
  {
    if ( btn )
    {
      int8_t const delta = 5;
      usb_hid.mouseMove(0, delta, delta); // no ID: right + down

      // delay a bit before attempt to send keyboard report
      delay(10);
    }
  }
}

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and 
// return number of copied bytes (must be multiple of block size) 
int32_t msc_read_cb (uint32_t lba, void* buffer, uint32_t bufsize)
{
  // Note: SPIFLash Bock API: readBlocks/writeBlocks/syncBlocks
  // already include 4K sector caching internally. We don't need to cache it, yahhhh!!
  return flash.readBlocks(lba, (uint8_t*) buffer, bufsize/512) ? bufsize : -1;
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and 
// return number of written bytes (must be multiple of block size)
int32_t msc_write_cb (uint32_t lba, uint8_t* buffer, uint32_t bufsize)
{
  // Note: SPIFLash Bock API: readBlocks/writeBlocks/syncBlocks
  // already include 4K sector caching internally. We don't need to cache it, yahhhh!!
  return flash.writeBlocks(lba, buffer, bufsize/512) ? bufsize : -1;
}

// Callback invoked when WRITE10 command is completed (status received and accepted by host).
// used to flush any pending cache.
void msc_flush_cb (void)
{
  flash.syncBlocks();
}
