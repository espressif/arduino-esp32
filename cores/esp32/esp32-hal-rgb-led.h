#ifndef MAIN_ESP32_HAL_RGB_LED_H_
#define MAIN_ESP32_HAL_RGB_LED_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "esp32-hal.h"

#ifndef RGB_BRIGHTNESS
#define RGB_BRIGHTNESS 64
#endif

#ifndef RGB_BUILTIN_LED_COLOR_ORDER
#define RGB_BUILTIN_LED_COLOR_ORDER LED_COLOR_ORDER_GRB  // default WS2812B color order
#endif

typedef enum {
  LED_COLOR_ORDER_RGB,
  LED_COLOR_ORDER_BGR,
  LED_COLOR_ORDER_BRG,
  LED_COLOR_ORDER_RBG,
  LED_COLOR_ORDER_GBR,
  LED_COLOR_ORDER_GRB
} rgb_led_color_order_t;

void rgbLedWriteOrdered(uint8_t pin, rgb_led_color_order_t order, uint8_t red_val, uint8_t green_val, uint8_t blue_val);

// Will use RGB_BUILTIN_LED_COLOR_ORDER
void rgbLedWrite(uint8_t pin, uint8_t red_val, uint8_t green_val, uint8_t blue_val);

// Backward compatibility - Deprecated. It will be removed in future releases.
[[deprecated("Use rgbLedWrite() instead.")]]
void neopixelWrite(uint8_t p, uint8_t r, uint8_t g, uint8_t b);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_ESP32_HAL_RGB_LED_H_ */
