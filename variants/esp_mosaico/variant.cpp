#include "pins_arduino.h"
#include "driver/gpio.h"
#include "esp32-hal.h"
#include "esp_err.h"

extern "C" void initVariant(void) {
  const gpio_num_t powerPin = (gpio_num_t)MOSAICO_PERIPHERAL_POWER;
  gpio_config_t config = {};
  config.pin_bit_mask = 1ULL << MOSAICO_PERIPHERAL_POWER;
  config.mode = GPIO_MODE_OUTPUT;
  config.pull_up_en = GPIO_PULLUP_DISABLE;
  config.pull_down_en = GPIO_PULLDOWN_DISABLE;
  config.intr_type = GPIO_INTR_DISABLE;
  ESP_ERROR_CHECK(gpio_set_level(powerPin, MOSAICO_POWER_OFF_LEVEL));
  ESP_ERROR_CHECK(gpio_config(&config));
  ESP_ERROR_CHECK(gpio_hold_dis(powerPin));

  // Ramp the active-low supply over 99 ms to limit peripheral inrush.
  // Software PWM avoids reserving an LEDC timer/channel for board startup.
  for (uint32_t onTime = 1; onTime < 100; ++onTime) {
    for (uint32_t pulse = 0; pulse < 10; ++pulse) {
      gpio_set_level(powerPin, MOSAICO_POWER_ON_LEVEL);
      delayMicroseconds(onTime);
      gpio_set_level(powerPin, MOSAICO_POWER_OFF_LEVEL);
      delayMicroseconds(100 - onTime);
    }
  }
  ESP_ERROR_CHECK(gpio_set_level(powerPin, MOSAICO_POWER_ON_LEVEL));
  delay(100);
}
