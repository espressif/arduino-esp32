// Copyright 2024 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "soc/soc_caps.h"

#include "esp32-hal-rgb-led.h"

// Backward compatibility - Deprecated. It will be removed in future releases.
void neopixelWrite(uint8_t pin, uint8_t red_val, uint8_t green_val, uint8_t blue_val) {
  log_w("neopixelWrite() is deprecated. Use rgbLedWrite().");
  rgbLedWrite(pin, red_val, green_val, blue_val);
}

void rgbLedWrite(uint8_t pin, uint8_t red_val, uint8_t green_val, uint8_t blue_val) {
  rgbLedWriteOrdered(pin, RGB_BUILTIN_LED_COLOR_ORDER, red_val, green_val, blue_val);
}

void rgbLedWriteOrdered(uint8_t pin, rgb_led_color_order_t order, uint8_t red_val, uint8_t green_val, uint8_t blue_val) {
#if SOC_RMT_SUPPORTED
  rmt_data_t led_data[24];

  // Verify if the pin used is RGB_BUILTIN and fix GPIO number
#ifdef RGB_BUILTIN
  pin = pin == RGB_BUILTIN ? pin - SOC_GPIO_PIN_COUNT : pin;
#endif
  if (!rmtInit(pin, RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_1, 10000000)) {
    log_e("RGB LED driver initialization failed for GPIO%d!", pin);
    return;
  }

  // default WS2812B color order is G, R, B
  int color[3] = {green_val, red_val, blue_val};

  switch (order) {
    case LED_COLOR_ORDER_RGB:
      color[0] = red_val;
      color[1] = green_val;
      color[2] = blue_val;
      break;
    case LED_COLOR_ORDER_BGR:
      color[0] = blue_val;
      color[1] = green_val;
      color[2] = red_val;
      break;
    case LED_COLOR_ORDER_BRG:
      color[0] = blue_val;
      color[1] = red_val;
      color[2] = green_val;
      break;
    case LED_COLOR_ORDER_RBG:
      color[0] = red_val;
      color[1] = blue_val;
      color[2] = green_val;
      break;
    case LED_COLOR_ORDER_GBR:
      color[0] = green_val;
      color[1] = blue_val;
      color[2] = red_val;
      break;
    default:  // GRB
      break;
  }

  int i = 0;
  for (int col = 0; col < 3; col++) {
    for (int bit = 0; bit < 8; bit++) {
      if ((color[col] & (1 << (7 - bit)))) {
        // HIGH bit
        led_data[i].level0 = 1;     // T1H
        led_data[i].duration0 = 8;  // 0.8us
        led_data[i].level1 = 0;     // T1L
        led_data[i].duration1 = 4;  // 0.4us
      } else {
        // LOW bit
        led_data[i].level0 = 1;     // T0H
        led_data[i].duration0 = 4;  // 0.4us
        led_data[i].level1 = 0;     // T0L
        led_data[i].duration1 = 8;  // 0.8us
      }
      i++;
    }
  }
  rmtWrite(pin, led_data, RMT_SYMBOLS_OF(led_data), RMT_WAIT_FOR_EVER);
#else
  log_e("RMT is not supported on " CONFIG_IDF_TARGET);
#endif /* SOC_RMT_SUPPORTED */
}
