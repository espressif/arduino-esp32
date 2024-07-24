#include "esp32-hal-gpio.h"
#include "pins_arduino.h"

extern "C" {

void initVariant(void) {
  pinMode(WIFI_ENABLE, OUTPUT);
  digitalWrite(WIFI_ENABLE, LOW);//turn on this function

  pinMode(WIFI_ANT_CONFIG, OUTPUT); 
  digitalWrite(WIFI_ANT_CONFIG, LOW);//use external antenna
}
}
