#include "esp32-hal-gpio.h"
#include "pins_arduino.h"
#include "driver/rmt_tx.h"

extern "C" {

void blinkLED(uint8_t color[3], rmt_channel_handle_t led_chan, rmt_encoder_handle_t ws2812_encoder, rmt_transmit_config_t tx_config) {
  ESP_ERROR_CHECK(rmt_transmit(led_chan, ws2812_encoder, color, sizeof(color), &tx_config));
  rmt_tx_wait_all_done(led_chan, portMAX_DELAY);

  // Wait a moment
  delay(50);

  // Turn LED off
  uint8_t pixel_off[3] = { 0x00, 0x00, 0x00 };
  ESP_ERROR_CHECK(rmt_transmit(led_chan, ws2812_encoder, pixel_off, sizeof(pixel_off), &tx_config));
  rmt_tx_wait_all_done(led_chan, portMAX_DELAY);
}

void initVariant(void) {
  rmt_channel_handle_t led_chan = NULL;
  rmt_tx_channel_config_t tx_chan_config = {};
  tx_chan_config.clk_src = RMT_CLK_SRC_DEFAULT;
  tx_chan_config.resolution_hz = 10 * 1000 * 1000;
  tx_chan_config.mem_block_symbols = 64;
  tx_chan_config.trans_queue_depth = 4;
  tx_chan_config.gpio_num = (gpio_num_t)PIN_LED;
  tx_chan_config.flags.invert_out = false;
  tx_chan_config.flags.with_dma = false;
  ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &led_chan));

  // WS2812 encoder config (available in `esp-rmt`)
  rmt_encoder_handle_t ws2812_encoder = NULL;
  rmt_bytes_encoder_config_t bytes_encoder_config = {
    .bit0 = { .duration0 = 4, .level0 = 1, .duration1 = 9, .level1 = 0 },
    .bit1 = { .duration0 = 8, .level0 = 1, .duration1 = 5, .level1 = 0 },
    .flags = { .msb_first = true }
  };
  ESP_ERROR_CHECK(rmt_new_bytes_encoder(&bytes_encoder_config, &ws2812_encoder));

  ESP_ERROR_CHECK(rmt_enable(led_chan));

  rmt_transmit_config_t tx_config = {
    .loop_count = 0
  };

  uint8_t pixel[3] = { 0x10, 0x00, 0x00 }; // green
  blinkLED(pixel, led_chan, ws2812_encoder, tx_config);
}
}
