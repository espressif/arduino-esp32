#if ARDUINO_USB_MODE
#warning This sketch should be used when USB is in OTG mode
void setup(){}
void loop(){}
#else
#include "USB.h"
#include "USBHIDGamepad.h"
USBHIDGamepad Gamepad;

// This sketch works correctly for the ESP32-S2 and ESP32-S3 in OTG mode (TinyUSB)

const int buttonPin = 0;  // GPIO 0 is the BOOT button of the board
int previousButtonState = HIGH;

void setup() {
  pinMode(buttonPin, INPUT_PULLUP);
  Gamepad.begin();
  USB.begin();
}

void loop() {
  int buttonState = digitalRead(buttonPin);
  if (buttonState != previousButtonState) {
    if (buttonState == LOW) { // BOOT Button pressed
      // changes many states
      Gamepad.send(-100, 100, 50, 100, -100, -50, HAT_DOWN_RIGHT, BUTTON_TL);
    } else {
      // restores neutral states
      Gamepad.send(0, 0, 0, 0, 0, 0, 0, 0);
    }
  }
  previousButtonState = buttonState;
}
#endif /* ARDUINO_USB_MODE */
