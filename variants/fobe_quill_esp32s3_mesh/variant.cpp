#include "esp32-hal-gpio.h"
#include "pins_arduino.h"

extern "C" {

void initVariant(void) {
  // Turn on the peripheral power
  pinMode(PIN_PERI_EN, OUTPUT);
  digitalWrite(PIN_PERI_EN, HIGH);

  // Turn on the OLED power
  pinMode(PIN_OLED_EN, OUTPUT);
  digitalWrite(PIN_OLED_EN, LOW);
}
}
