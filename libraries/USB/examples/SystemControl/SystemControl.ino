#ifndef ARDUINO_USB_MODE
#error This ESP32 SoC has no Native USB interface
#elif ARDUINO_USB_MODE == 1
#warning This sketch should be used when USB is in OTG mode
void setup(){}
void loop(){}
#else
#include "USB.h"
#include "USBHIDSystemControl.h"
USBHIDSystemControl SystemControl;

const int buttonPin = 0;
int previousButtonState = HIGH;

void setup() {
  pinMode(buttonPin, INPUT_PULLUP);
  SystemControl.begin();
  USB.begin();
}

void loop() {
  int buttonState = digitalRead(buttonPin);
  if ((buttonState != previousButtonState) && (buttonState == LOW)) {
    SystemControl.press(SYSTEM_CONTROL_POWER_OFF);
    SystemControl.release();
  }
  previousButtonState = buttonState;
}
#endif /* ARDUINO_USB_MODE */
