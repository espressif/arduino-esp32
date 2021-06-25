/*********************************************************************
 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
 Copyright (c) 2019 Ha Thach for Adafruit Industries
 All text above, and the splash screen below must be included in
 any redistribution
*********************************************************************/

/* This example exposes both external flash and SD card as mass storage (dual LUNs)
 * Following library is required
 *   - Adafruit_SPIFlash https://github.com/adafruit/Adafruit_SPIFlash
 *   - SdFat https://github.com/adafruit/SdFat
 *
 * Note: Adafruit fork of SdFat enabled ENABLE_EXTENDED_TRANSFER_CLASS and FAT12_SUPPORT
 * in SdFatConfig.h, which is needed to run SdFat on external flash. You can use original
 * SdFat library and manually change those macros
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

// File system object on external flash from SdFat
FatFileSystem fatfs;

const int chipSelect = 10;

// File system on SD Card
SdFat sd;

// USB Mass Storage object
Adafruit_USBD_MSC usb_msc;

// Set to true when PC write to flash
bool sd_changed = false;
bool flash_changed = false;

// the setup function runs once when you press reset or power the board
void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);

  // MSC with 2 Logical Units
  usb_msc.setMaxLun(2);

  usb_msc.setID(0, "Adafruit", "External Flash", "1.0");
  usb_msc.setID(1, "Adafruit", "SD Card", "1.0");

  // Since initialize both external flash and SD card can take time.
  // If it takes too long, our board could be enumerated as CDC device only
  // i.e without Mass Storage. To prevent this, we call Mass Storage begin first
  // LUN readiness will always be set later on
  usb_msc.begin();

  //------------- Lun 0 for external flash -------------//
  flash.begin();
  fatfs.begin(&flash);

  usb_msc.setCapacity(0, flash.size()/512, 512);
  usb_msc.setReadWriteCallback(0, external_flash_read_cb, external_flash_write_cb, external_flash_flush_cb);
  usb_msc.setUnitReady(0, true);

  flash_changed = true; // to print contents initially

  //------------- Lun 1 for SD card -------------//
  if ( sd.begin(chipSelect, SD_SCK_MHZ(50)) )
  {
    uint32_t block_count = sd.card()->cardSize();
    usb_msc.setCapacity(1, block_count, 512);
    usb_msc.setReadWriteCallback(1, sdcard_read_cb, sdcard_write_cb, sdcard_flush_cb);
    usb_msc.setUnitReady(1, true);

    sd_changed = true; // to print contents initially
  }

  Serial.begin(115200);
  while ( !Serial ) delay(10);   // wait for native usb

  Serial.println("Adafruit TinyUSB Mass Storage External Flash + SD Card example");
  delay(1000);
}

void print_rootdir(File* rdir)
{
  File file;

  // Open next file in root.
  // Warning, openNext starts at the current directory position
  // so a rewind of the directory may be required.
  while ( file.openNext(rdir, O_RDONLY) )
  {
    file.printFileSize(&Serial);
    Serial.write(' ');
    file.printName(&Serial);
    if ( file.isDir() )
    {
      // Indicate a directory.
      Serial.write('/');
    }
    Serial.println();
    file.close();
  }
}

void loop()
{
  if ( flash_changed )
  {
    File root;
    root = fatfs.open("/");

    Serial.println("Flash contents:");
    print_rootdir(&root);
    Serial.println();

    root.close();

    flash_changed = false;
  }

  if ( sd_changed )
  {
    File root;
    root = sd.open("/");

    Serial.println("SD contents:");
    print_rootdir(&root);
    Serial.println();

    root.close();

    sd_changed = false;
  }

  delay(1000); // refresh every 1 second
}


//--------------------------------------------------------------------+
// SD Card callbacks
//--------------------------------------------------------------------+

int32_t sdcard_read_cb (uint32_t lba, void* buffer, uint32_t bufsize)
{
  return sd.card()->readBlocks(lba, (uint8_t*) buffer, bufsize/512) ? bufsize : -1;
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and 
// return number of written bytes (must be multiple of block size)
int32_t sdcard_write_cb (uint32_t lba, uint8_t* buffer, uint32_t bufsize)
{
  digitalWrite(LED_BUILTIN, HIGH);

  return sd.card()->writeBlocks(lba, buffer, bufsize/512) ? bufsize : -1;
}

// Callback invoked when WRITE10 command is completed (status received and accepted by host).
// used to flush any pending cache.
void sdcard_flush_cb (void)
{
  sd.card()->syncBlocks();

  // clear file system's cache to force refresh
  sd.cacheClear();

  sd_changed = true;

  digitalWrite(LED_BUILTIN, LOW);
}

//--------------------------------------------------------------------+
// External Flash callbacks
//--------------------------------------------------------------------+

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and
// return number of copied bytes (must be multiple of block size)
int32_t external_flash_read_cb (uint32_t lba, void* buffer, uint32_t bufsize)
{
  // Note: SPIFLash Bock API: readBlocks/writeBlocks/syncBlocks
  // already include 4K sector caching internally. We don't need to cache it, yahhhh!!
  return flash.readBlocks(lba, (uint8_t*) buffer, bufsize/512) ? bufsize : -1;
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and
// return number of written bytes (must be multiple of block size)
int32_t external_flash_write_cb (uint32_t lba, uint8_t* buffer, uint32_t bufsize)
{
  digitalWrite(LED_BUILTIN, HIGH);

  // Note: SPIFLash Bock API: readBlocks/writeBlocks/syncBlocks
  // already include 4K sector caching internally. We don't need to cache it, yahhhh!!
  return flash.writeBlocks(lba, buffer, bufsize/512) ? bufsize : -1;
}

// Callback invoked when WRITE10 command is completed (status received and accepted by host).
// used to flush any pending cache.
void external_flash_flush_cb (void)
{
  flash.syncBlocks();

  // clear file system's cache to force refresh
  fatfs.cacheClear();

  flash_changed = true;

  digitalWrite(LED_BUILTIN, LOW);
}
