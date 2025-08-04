#include "esp32-hal-gpio.h"
#include "pins_arduino.h"

extern "C" {

void initVariant(void) {
  pinMode(PIN_MFP_PWR, OUTPUT);
  digitalWrite(PIN_MFP_PWR, HIGH);

  pinMode(PIN_OLED_EN, OUTPUT);
  digitalWrite(PIN_OLED_EN, HIGH);
}
}
