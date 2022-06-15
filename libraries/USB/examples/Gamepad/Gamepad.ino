#if ARDUINO_USB_MODE
#warning This sketch should be used when USB is in OTG mode
void setup(){}
void loop(){}
#else
#include "USB.h"
#include "USBHIDGamepad.h"
USBHIDGamepad Gamepad;

const int buttonPin = 0;
int previousButtonState = HIGH;

void setup() {
  pinMode(buttonPin, INPUT_PULLUP);
  Gamepad.begin();
  USB.begin();
}

void loop() {
  int buttonState = digitalRead(buttonPin);
  if ((buttonState != previousButtonState) && (buttonState == LOW)) {
    Gamepad.pressButton(BUTTON_START);
    Gamepad.releaseButton(BUTTON_START);
  }
  previousButtonState = buttonState;
}
#endif /* ARDUINO_USB_MODE */
