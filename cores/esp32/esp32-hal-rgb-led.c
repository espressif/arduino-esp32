#include "soc/soc_caps.h"

#include "esp32-hal-rgb-led.h"

void neopixelWrite(uint8_t pin, uint8_t green_val, uint8_t red_val, uint8_t blue_val) {
  neopixelWriteOrdered(pin, GRB, red_val, green_val, blue_val);
}

void neopixelWriteOrdered(uint8_t pin, color_order_t color_order, uint8_t red_val, uint8_t green_val, uint8_t blue_val) {
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

  int color[3];
  switch (color_order) {
    case RGB:
      color[0] = red_val;
      color[1] = green_val;
      color[2] = blue_val;
      break;
    case BGR:
      color[0] = blue_val;
      color[1] = green_val;
      color[2] = red_val;
      break;
    case BRG:
      color[0] = blue_val;
      color[1] = red_val;
      color[2] = green_val;
      break;
    case RBG:
      color[0] = red_val;
      color[1] = blue_val;
      color[2] = green_val;
      break;
    case GBR:
      color[0] = green_val;
      color[1] = blue_val;
      color[2] = red_val;
      break;
    default:
      color[0] = green_val;
      color[1] = red_val;
      color[2] = blue_val;
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
