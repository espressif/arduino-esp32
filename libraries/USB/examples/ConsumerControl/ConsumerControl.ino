#ifndef ARDUINO_USB_MODE
#error This ESP32 SoC has no Native USB interface
#elif ARDUINO_USB_MODE == 1
#warning This sketch should be used when USB is in OTG mode
void setup(){}
void loop(){}
#else
#include "USB.h"
#include "USBHIDConsumerControl.h"
USBHIDConsumerControl ConsumerControl;

const int buttonPin = 0;
int previousButtonState = HIGH;

void setup() {
  pinMode(buttonPin, INPUT_PULLUP);
  ConsumerControl.begin();
  USB.begin();
}

void loop() {
  int buttonState = digitalRead(buttonPin);
  if ((buttonState != previousButtonState) && (buttonState == LOW)) {
    ConsumerControl.press(CONSUMER_CONTROL_VOLUME_INCREMENT);
    ConsumerControl.release();
  }
  previousButtonState = buttonState;
}
#endif /* ARDUINO_USB_MODE */
