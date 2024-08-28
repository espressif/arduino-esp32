
#include "esp32-hal-gpio.h"
#include "pins_arduino.h"

extern "C" {
// Initialize variant/board, called before setup()
void initVariant(void) {
  pinMode(CYD_LED_RED, OUTPUT);
  pinMode(CYD_LED_GREEN, OUTPUT);
  pinMode(CYD_LED_BLUE, OUTPUT);
  CYD_LED_RGB_OFF();
}
}
