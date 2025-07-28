#include "esp32-hal-gpio.h"
#include "pins_arduino.h"

extern "C" {

void initVariant(void) {
  // blink the RGB LED
  rgbLedWrite(PIN_LED, 0x00, 0x10, 0x00);  // green
  delay(20);
  rgbLedWrite(PIN_LED, 0x00, 0x00, 0x00);  // off
}
}
