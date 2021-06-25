/*********************************************************************
 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
 Copyright (c) 2019 Ha Thach for Adafruit Industries
 All text above, and the splash screen below must be included in
 any redistribution
*********************************************************************/

/* This example expose SD card as mass storage using
 * SdFat Library
 */

#include "SPI.h"
#include "SdFat.h"
#include "Adafruit_TinyUSB.h"

const int chipSelect = 10;

// File system on SD Card
SdFat sd;

SdFile root;
SdFile file;

// USB Mass Storage object
Adafruit_USBD_MSC usb_msc;

// Set to true when PC write to flash
bool changed;

// the setup function runs once when you press reset or power the board
void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);

  // Set disk vendor id, product id and revision with string up to 8, 16, 4 characters respectively
  usb_msc.setID("Adafruit", "SD Card", "1.0");

  // Set read write callback
  usb_msc.setReadWriteCallback(msc_read_cb, msc_write_cb, msc_flush_cb);

  // Still initialize MSC but tell usb stack that MSC is not ready to read/write
  // If we don't initialize, board will be enumerated as CDC only
  usb_msc.setUnitReady(false);
  usb_msc.begin();

  Serial.begin(115200);
  while ( !Serial ) delay(10);   // wait for native usb

  Serial.println("Adafruit TinyUSB Mass Storage SD Card example");

  Serial.print("\nInitializing SD card ... ");
  Serial.print("CS = "); Serial.println(chipSelect);

  if ( !sd.begin(chipSelect, SD_SCK_MHZ(50)) )
  {
    Serial.println("initialization failed. Things to check:");
    Serial.println("* is a card inserted?");
    Serial.println("* is your wiring correct?");
    Serial.println("* did you change the chipSelect pin to match your shield or module?");
    while (1) delay(1);
  }

  // Size in blocks (512 bytes)
  uint32_t block_count = sd.card()->cardSize();

  Serial.print("Volume size (MB):  ");
  Serial.println((block_count/2) / 1024);

  // Set disk size, SD block size is always 512
  usb_msc.setCapacity(block_count, 512);

  // MSC is ready for read/write
  usb_msc.setUnitReady(true);

  changed = true; // to print contents initially
}

void loop()
{
  if ( changed )
  {
    root.open("/");
    Serial.println("SD contents:");

    // Open next file in root.
    // Warning, openNext starts at the current directory position
    // so a rewind of the directory may be required.
    while ( file.openNext(&root, O_RDONLY) )
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

    root.close();

    Serial.println();

    changed = false;
    delay(1000); // refresh every 0.5 second
  }
}

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and
// return number of copied bytes (must be multiple of block size)
int32_t msc_read_cb (uint32_t lba, void* buffer, uint32_t bufsize)
{
  return sd.card()->readBlocks(lba, (uint8_t*) buffer, bufsize/512) ? bufsize : -1;
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and 
// return number of written bytes (must be multiple of block size)
int32_t msc_write_cb (uint32_t lba, uint8_t* buffer, uint32_t bufsize)
{
  digitalWrite(LED_BUILTIN, HIGH);

  return sd.card()->writeBlocks(lba, buffer, bufsize/512) ? bufsize : -1;
}

// Callback invoked when WRITE10 command is completed (status received and accepted by host).
// used to flush any pending cache.
void msc_flush_cb (void)
{
  sd.card()->syncBlocks();

  // clear file system's cache to force refresh
  sd.cacheClear();

  changed = true;

  digitalWrite(LED_BUILTIN, LOW);
}
