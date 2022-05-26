#ifndef MAIN_ESP32_HAL_RGB_LED_H_
#define MAIN_ESP32_HAL_RGB_LED_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "esp32-hal.h"
/*
#include "soc/soc_caps.h"
#include "pins_arduino.h"
*/

//#ifdef BOARD_HAS_NEOPIXEL
  void RGBLedWrite(uint8_t pin, uint8_t red_val, uint8_t green_val, uint8_t blue_val);
//#endif

#ifdef __cplusplus
}
#endif

#endif /* MAIN_ESP32_HAL_RGB_LED_H_ */