#include "esp32-hal.h"
#include "pins_arduino.h"

extern "C" {

// Reset the ESP32-C5 co-processor on every host boot. GPIO20 drives the C5 EN line through an
// NPN inverter, so the pin is active high: HIGH = C5 held in reset, LOW = C5 running.
void initVariant(void) {
  pinMode(C5_RESET_PIN, OUTPUT);
  digitalWrite(C5_RESET_PIN, HIGH);
  delay(10);
  digitalWrite(C5_RESET_PIN, LOW);
}
}
