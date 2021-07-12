/*********************************************************************
 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
 Copyright (c) 2019 Ha Thach for Adafruit Industries
 All text above, and the splash screen below must be included in
 any redistribution
*********************************************************************/

/* This sketch demonstrates WebUSB as web serial with browser with WebUSB support (e.g Chrome).
 * After enumerated successfully, Browser will pop-up notification
 * with URL to landing page, click on it to test
 *  - Click "Connect" and select device, When connected the neopixel LED will change color to Green.
 *  - When received color from browser in format '#RRGGBB', device will change the color of neopixel accordingly
 *  
 * Note: 
 * - The WebUSB landing page notification is currently disabled in Chrome 
 * on Windows due to Chromium issue 656702 (https://crbug.com/656702). You have to 
 * go to landing page (below) to test
 * 
 * - On Windows 7 and prior: You need to use Zadig tool to manually bind the 
 * WebUSB interface with the WinUSB driver for Chrome to access. From windows 8 and 10, this
 * is done automatically by firmware.
 */

#include "Adafruit_TinyUSB.h"
#include <Adafruit_NeoPixel.h>

// Which pin on the Arduino is connected to the NeoPixels?
// On a Trinket or Gemma we suggest changing this to 1
// use on-board neopixel PIN_NEOPIXEL if existed
#ifdef PIN_NEOPIXEL
  #define PIN      PIN_NEOPIXEL
#else
  #define PIN      8
#endif

// How many NeoPixels are attached to the Arduino?
#define NUMPIXELS  10

// When we setup the NeoPixel library, we tell it how many pixels, and which pin to use to send signals.
// Note that for older NeoPixel strips you might need to change the third parameter--see the strandtest
// example for more information on possible values.
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

// USB WebUSB object
Adafruit_USBD_WebUSB usb_web;

// Landing Page: scheme (0: http, 1: https), url
WEBUSB_URL_DEF(landingPage, 1 /*https*/, "adafruit.github.io/Adafruit_TinyUSB_Arduino/examples/webusb-rgb/index.html");

// the setup function runs once when you press reset or power the board
void setup()
{
  //usb_web.setStringDescriptor("TinyUSB WebUSB");
  usb_web.setLandingPage(&landingPage);
  usb_web.setLineStateCallback(line_state_callback);
  usb_web.begin();

  Serial.begin(115200);

  // This initializes the NeoPixel with RED
  pixels.begin();
  pixels.setBrightness(50);
  pixels.fill(0xff0000);
  pixels.show();

  // wait until device mounted
  while( !USBDevice.mounted() ) delay(1);

  Serial.println("TinyUSB WebUSB RGB example");
}

// convert a hex character to number
uint8_t char2num(char c)
{
  if (c >= 'a') return c - 'a' + 10;
  if (c >= 'A') return c - 'A' + 10;
  return c - '0';  
}

void loop()
{
  // Landing Page 7 characters as hex color '#RRGGBB'
  if (usb_web.available() < 7) return;

  uint8_t input[7];
  usb_web.readBytes(input, 7);

  // Print to serial for debugging
  Serial.write(input, 7);
  Serial.println();

  uint8_t red   = 16*char2num(input[1]) + char2num(input[2]);
  uint8_t green = 16*char2num(input[3]) + char2num(input[4]);
  uint8_t blue  = 16*char2num(input[5]) + char2num(input[6]);

  uint32_t color = (red << 16) | (green << 8) | blue;
  pixels.fill(color);
  pixels.show();
}

void line_state_callback(bool connected)
{
  // connected = green, disconnected = red
  pixels.fill(connected ? 0x00ff00 : 0xff0000);
  pixels.show();
}
