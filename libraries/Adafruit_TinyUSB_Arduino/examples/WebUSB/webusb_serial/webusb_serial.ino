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
 *  - Click "Connect" and select device, When connected the on-board LED will litted up.
 *  - Any charters received from either webusb/Serial will be echo back to webusb and Serial
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

// USB WebUSB object
Adafruit_USBD_WebUSB usb_web;

// Landing Page: scheme (0: http, 1: https), url
WEBUSB_URL_DEF(landingPage, 1 /*https*/, "adafruit.github.io/Adafruit_TinyUSB_Arduino/examples/webusb-serial/index.html");

int led_pin = LED_BUILTIN;

// the setup function runs once when you press reset or power the board
void setup()
{
  pinMode(led_pin, OUTPUT);
  digitalWrite(led_pin, LOW);
  
  usb_web.setLandingPage(&landingPage);
  usb_web.setLineStateCallback(line_state_callback);
  //usb_web.setStringDescriptor("TinyUSB WebUSB");
  usb_web.begin();

  Serial.begin(115200);

  // wait until device mounted
  while( !USBDevice.mounted() ) delay(1);

  Serial.println("TinyUSB WebUSB Serial example");
}

// function to echo to both Serial and WebUSB
void echo_all(char chr)
{
  Serial.write(chr);
  // print extra newline for Serial
  if ( chr == '\r' ) Serial.write('\n');
  
  usb_web.write(chr);
}

void loop()
{
  // from WebUSB to both Serial & webUSB
  if (usb_web.available()) echo_all(usb_web.read());

  // From Serial to both Serial & webUSB
  if (Serial.available())   echo_all(Serial.read());  
}

void line_state_callback(bool connected)
{
  digitalWrite(led_pin, connected);

  if ( connected ) usb_web.println("TinyUSB WebUSB Serial example");
}
