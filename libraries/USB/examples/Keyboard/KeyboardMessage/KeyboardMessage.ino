/*
  Keyboard Message test

  For the Arduino Leonardo and Micro.

  Sends a text string when a button is pressed.

  The circuit:
  - pushbutton attached from pin 0 to ground
  - 10 kilohm resistor attached from pin 0 to +5V

  created 24 Oct 2011
  modified 27 Mar 2012
  by Tom Igoe
  modified 11 Nov 2013
  by Scott Fitzgerald

  This example code is in the public domain.

  http://www.arduino.cc/en/Tutorial/KeyboardMessage
*/
#if ARDUINO_USB_MODE
#warning This sketch should be used when USB is in OTG mode
void setup(){}
void loop(){}
#else

#include "USB.h"
#include "USBHIDKeyboard.h"
USBHIDKeyboard Keyboard;

const int buttonPin = 0;          // input pin for pushbutton
int previousButtonState = HIGH;   // for checking the state of a pushButton
int counter = 0;                  // button push counter

void setup() {
  // make the pushButton pin an input:
  pinMode(buttonPin, INPUT_PULLUP);
  // initialize control over the keyboard:
  Keyboard.begin();
  USB.begin();
}

void loop() {
  // read the pushbutton:
  int buttonState = digitalRead(buttonPin);
  // if the button state has changed,
  if ((buttonState != previousButtonState)
      // and it's currently pressed:
      && (buttonState == LOW)) {
    // increment the button counter
    counter++;
    // type out a message
    Keyboard.print("You pressed the button ");
    Keyboard.print(counter);
    Keyboard.println(" times.");
  }
  // save the current button state for comparison next time:
  previousButtonState = buttonState;
}
#endif /* ARDUINO_USB_MODE */
