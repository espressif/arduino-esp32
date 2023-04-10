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
  Serial.begin(115200);
  Serial.println("\n==================\nUSB Gamepad Testing\n==================\n");
  Serial.println("Press BOOT Button to activate the USB gamepad.");
  Serial.println("Longer press will change the affected button and controls.");
  Serial.println("Shorter press/release just activates the button and controls.");
}

void loop() {
  static uint8_t padID = 0;
  static long lastPress = 0;
  
  int buttonState = digitalRead(buttonPin);
  if (buttonState != previousButtonState) {
    if (buttonState == LOW) { // BOOT Button pressed
      Gamepad.pressButton(padID);                    // Buttons 1 to 32
      Gamepad.leftStick(padID << 3, padID << 3);     // X Axis, Y Axis
      Gamepad.rightStick(-(padID << 2), padID << 2); // Z Axis, Z Rotation
      Gamepad.leftTrigger(padID << 4);               // X Rotation
      Gamepad.rightTrigger(-(padID << 4));           // Y Rotation
      Gamepad.hat((padID & 0x7) + 1);                // Point of View Hat
      log_d("Pressed PadID [%d]", padID);
      lastPress = millis();
    } else {
      Gamepad.releaseButton(padID);
      Gamepad.leftStick(0, 0);
      Gamepad.rightStick(0, 0);
      Gamepad.leftTrigger(0);
      Gamepad.rightTrigger(0);
      Gamepad.hat(HAT_CENTER);      
      log_d("Released PadID [%d]\n", padID);
      if (millis() - lastPress > 300) {
        padID = (padID + 1) & 0x1F;
        log_d("Changed padID to %d\n", padID);
      }
    }
  }
  previousButtonState = buttonState;
}
#endif /* ARDUINO_USB_MODE */
