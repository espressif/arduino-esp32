/*
*By setting the WIFI_ENABLE and WIFI_ANT_CONFIG pins,
*
*the XIAO_ESP32C6 will turn on the on-board antenna by default after power-on
*
*https://wiki.seeedstudio.com/xiao_esp32c6_getting_started/
*/

#include "esp32-hal-gpio.h"
#include "pins_arduino.h"

extern "C" {

void initVariant(void) {
  pinMode(WIFI_ENABLE, OUTPUT);
  digitalWrite(WIFI_ENABLE, LOW);  //turn on this function

  pinMode(WIFI_ANT_CONFIG, OUTPUT);
  digitalWrite(WIFI_ANT_CONFIG, LOW);  //use built-in antenna, set HIGH to use external antenna
}
}
