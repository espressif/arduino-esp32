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

/* This sketch demonstrates USB CDC Serial can be dropped by simply
 * call Serial.end() within setup(). This must be called before any
 * other USB interfaces (MSC / HID) begin to have a clean configuration
 *
 * Note: this will cause device to loose the touch1200 and require
 * user manual interaction to put device into bootloader/DFU mode.
 */

int led = LED_BUILTIN;

void setup()
{
  Serial.end();
  pinMode(led, OUTPUT);
}

void loop()
{
  digitalWrite(led, HIGH);
  delay(1000);
  digitalWrite(led, LOW);
  delay(1000);
}
