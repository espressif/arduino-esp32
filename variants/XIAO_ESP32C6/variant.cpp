#include "esp32-hal-gpio.h"
#include "pins_arduino.h"

extern "C" {

// Initialize variant/board, called before setup()
void initVariant(void) {
  pinMode(3, OUTPUT);
  digitalWrite(3, LOW);//turn on this function

  pinMode(14, OUTPUT); 
  digitalWrite(14, HIGH);//use external antenna
}
}
