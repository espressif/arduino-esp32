/*********************************************************************
 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
 Copyright (c) 2019 Ha Thach for Adafruit Industries
 All text above, and the splash screen below must be included in
 any redistribution
*********************************************************************/

/* This sketch demonstrates USB HID Mouse and Keyboard with Joy Feather Wing.
 * - The analog stick move mouse cursor
 * - Button A, B, X, Y will send character a, b, x, y
 * - Any actions will wake up PC host if it is in suspended (standby) mode.
 *
 * Joy Feather Wing: https://www.adafruit.com/product/3632
 *
 * Following library is required
 *  - Adafruit_seesaw
 */

#include "Adafruit_TinyUSB.h"
#include "Adafruit_seesaw.h"

#define BUTTON_A  6
#define BUTTON_B  7
#define BUTTON_Y  9
#define BUTTON_X  10
uint32_t button_mask = (1 << BUTTON_A) | (1 << BUTTON_B) |
                       (1 << BUTTON_Y) | (1 << BUTTON_X);

Adafruit_seesaw ss;

// Report ID
enum
{
  RID_KEYBOARD = 1,
  RID_MOUSE
};

// HID report descriptor using TinyUSB's template
uint8_t const desc_hid_report[] =
{
  TUD_HID_REPORT_DESC_KEYBOARD( HID_REPORT_ID(RID_KEYBOARD) ),
  TUD_HID_REPORT_DESC_MOUSE   ( HID_REPORT_ID(RID_MOUSE) )
};

// USB HID object
Adafruit_USBD_HID usb_hid;

int last_x, last_y;

// the setup function runs once when you press reset or power the board
void setup()
{
  usb_hid.setPollInterval(2);
  usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));

  usb_hid.begin();

  Serial.begin(115200);
  Serial.println("Adafruit TinyUSB HID Mouse with Joy FeatherWing example");

  if(!ss.begin(0x49)){
    Serial.println("ERROR! seesaw not found");
    while(1);
  } else {
    Serial.println("seesaw started");
    Serial.print("version: ");
    Serial.println(ss.getVersion(), HEX);
  }
  ss.pinModeBulk(button_mask, INPUT_PULLUP);
  ss.setGPIOInterrupts(button_mask, 1);

  last_y = ss.analogRead(2);
  last_x = ss.analogRead(3);

  // wait until device mounted
  while( !USBDevice.mounted() ) delay(1);
}

void loop()
{
  // poll gpio once each 10 ms
  delay(10);

  // If either analog stick or any buttons is pressed
  bool has_action = false;

  /*------------- Mouse -------------*/
  int y = ss.analogRead(2);
  int x = ss.analogRead(3);

  // reduce the delta by half to slow down the cursor move
  int dx = (x - last_x) / 2;
  int dy = (y - last_y) / 2;

  if ( (abs(dx) > 3) || (abs(dy) > 3) )
  {
    has_action = true;

    if ( usb_hid.ready() )
    {
      usb_hid.mouseMove(RID_MOUSE, dx, dy); // no ID: right + down

      last_x = x;
      last_y = y;

      // delay a bit before attempt to send keyboard report
      delay(10);
    }
  }


  /*------------- Keyboard -------------*/
  // button is active low, invert read value for convenience
  uint32_t buttons = ~ss.digitalReadBulk(button_mask);

  if ( usb_hid.ready() )
  {
    // use to prevent sending multiple consecutive zero report
    static bool has_key = false;

    if ( buttons & button_mask )
    {
      has_action = true;
      has_key = true;

      uint8_t keycode[6] = { 0 };
      
      if ( buttons & (1 << BUTTON_A) ) keycode[0] = HID_KEY_A;
      if ( buttons & (1 << BUTTON_B) ) keycode[0] = HID_KEY_B;
      if ( buttons & (1 << BUTTON_X) ) keycode[0] = HID_KEY_X;
      if ( buttons & (1 << BUTTON_Y) ) keycode[0] = HID_KEY_Y;

      usb_hid.keyboardReport(RID_KEYBOARD, 0, keycode);
    }else
    {
      // send empty key report if previously has key pressed
      if (has_key) usb_hid.keyboardRelease(RID_KEYBOARD);
      has_key = false;
    }
  }

  /*------------- Remote Wakeup -------------*/
  // Remote wakeup if PC is suspended and we has user interaction with joy feather wing
  if ( has_action && USBDevice.suspended() )
  {
    // Wake up only works if REMOTE_WAKEUP feature is enable by host
    // Usually this is the case with Mouse/Keyboard device
    USBDevice.remoteWakeup();
  }
}
