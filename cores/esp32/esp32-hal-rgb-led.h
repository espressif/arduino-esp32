#ifndef MAIN_ESP32_HAL_RGB_LED_H_
#define MAIN_ESP32_HAL_RGB_LED_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "esp32-hal.h"

#ifndef RGB_BRIGHTNESS
  #define RGB_BRIGHTNESS 64
#endif

void neopixelWrite(uint8_t pin, uint8_t red_val, uint8_t green_val, uint8_t blue_val);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_ESP32_HAL_RGB_LED_H_ */