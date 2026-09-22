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

#if !SOC_RMT_SUPPORTED
#include "esp_err.h"
#include "led_strip.h"

// SPI owns a bus for the lifetime of the strip - cache one handle per pin/order.
static led_strip_handle_t s_led_strip = NULL;
static int8_t s_led_pin = -1;
static rgb_led_color_order_t s_led_order = LED_COLOR_ORDER_GRB;

static led_color_component_format_t rgbLedColorFormat(rgb_led_color_order_t order) {
  led_color_component_format_t fmt = {};
  fmt.format.bytes_per_color = 1;
  fmt.format.num_components = 3;
  fmt.format.w_pos = 3;
  switch (order) {
    case LED_COLOR_ORDER_RGB:
      fmt.format.r_pos = 0;
      fmt.format.g_pos = 1;
      fmt.format.b_pos = 2;
      break;
    case LED_COLOR_ORDER_RBG:
      fmt.format.r_pos = 0;
      fmt.format.g_pos = 2;
      fmt.format.b_pos = 1;
      break;
    case LED_COLOR_ORDER_BGR:
      fmt.format.r_pos = 2;
      fmt.format.g_pos = 1;
      fmt.format.b_pos = 0;
      break;
    case LED_COLOR_ORDER_BRG:
      fmt.format.r_pos = 1;
      fmt.format.g_pos = 2;
      fmt.format.b_pos = 0;
      break;
    case LED_COLOR_ORDER_GBR:
      fmt.format.r_pos = 2;
      fmt.format.g_pos = 0;
      fmt.format.b_pos = 1;
      break;
    default:  // GRB (WS2812 default)
      return LED_STRIP_COLOR_COMPONENT_FMT_GRB;
  }
  return fmt;
}

static bool rgbLedEnsureSpiStrip(uint8_t pin, rgb_led_color_order_t order) {
  if (s_led_strip != NULL && s_led_pin == (int8_t)pin && s_led_order == order) {
    return true;
  }

  if (s_led_strip != NULL) {
    led_strip_del(s_led_strip);
    s_led_strip = NULL;
    s_led_pin = -1;
  }

  led_strip_config_t strip_config = {
    .strip_gpio_num = pin,
    .max_leds = 1,
    .led_model = LED_MODEL_WS2812,
    .color_component_format = rgbLedColorFormat(order),
    .flags = {.invert_out = false},
  };

  // Same backend as ESP-IDF get-started/blink when SOC_RMT_SUPPORTED is unset (e.g. ESP32-C61).
  led_strip_spi_config_t spi_config = {
    .clk_src = SPI_CLK_SRC_DEFAULT,
    .spi_bus = SPI2_HOST,
    .flags = {.with_dma = true},
  };

  esp_err_t err = led_strip_new_spi_device(&strip_config, &spi_config, &s_led_strip);
  if (err != ESP_OK) {
    log_e("RGB LED SPI init failed for GPIO%u (%s)", pin, esp_err_to_name(err));
    s_led_strip = NULL;
    return false;
  }

  s_led_pin = (int8_t)pin;
  s_led_order = order;
  return true;
}
#endif /* !SOC_RMT_SUPPORTED */

// Backward compatibility - Deprecated. It will be removed in future releases.
void neopixelWrite(uint8_t pin, uint8_t red_val, uint8_t green_val, uint8_t blue_val) {
  log_w("neopixelWrite() is deprecated. Use rgbLedWrite().");
  rgbLedWrite(pin, red_val, green_val, blue_val);
}

void rgbLedWrite(uint8_t pin, uint8_t red_val, uint8_t green_val, uint8_t blue_val) {
  rgbLedWriteOrdered(pin, RGB_BUILTIN_LED_COLOR_ORDER, red_val, green_val, blue_val);
}

void rgbLedWriteOrdered(uint8_t pin, rgb_led_color_order_t order, uint8_t red_val, uint8_t green_val, uint8_t blue_val) {
  // Verify if the pin used is RGB_BUILTIN and fix GPIO number
#ifdef RGB_BUILTIN
  pin = pin == RGB_BUILTIN ? pin - SOC_GPIO_PIN_COUNT : pin;
#endif

#if SOC_RMT_SUPPORTED
  rmt_data_t led_data[24];

  if (!rmtInit(pin, RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_1, 10000000)) {
    log_e("RGB LED driver initialization failed for GPIO%u", pin);
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
  if (!rgbLedEnsureSpiStrip(pin, order)) {
    return;
  }

  if (red_val == 0 && green_val == 0 && blue_val == 0) {
    led_strip_clear(s_led_strip);
    return;
  }

  if (led_strip_set_pixel(s_led_strip, 0, red_val, green_val, blue_val) != ESP_OK) {
    log_e("RGB LED set_pixel failed for GPIO%u", pin);
    return;
  }
  if (led_strip_refresh(s_led_strip) != ESP_OK) {
    log_e("RGB LED refresh failed for GPIO%u", pin);
  }
#endif /* SOC_RMT_SUPPORTED */
}
