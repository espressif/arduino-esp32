#include "pins_arduino.h"
#include "driver/gpio.h"
#include "esp32-hal.h"
#include "esp_err.h"

extern "C" void initVariant(void) {
  const gpio_num_t powerPin = (gpio_num_t)MOSAICO_PERIPHERAL_POWER;
  gpio_set_level(powerPin, MOSAICO_POWER_OFF_LEVEL);
  gpio_hold_dis(powerPin);
  pinMode(MOSAICO_PERIPHERAL_POWER, OUTPUT);
  for (uint32_t onTime = 1; onTime < 100; ++onTime) {
    for (uint32_t pulse = 0; pulse < 10; ++pulse) {
      gpio_set_level(powerPin, MOSAICO_POWER_ON_LEVEL);
      delayMicroseconds(onTime);
      gpio_set_level(powerPin, MOSAICO_POWER_OFF_LEVEL);
      delayMicroseconds(100 - onTime);
    }
  }
  digitalWrite(MOSAICO_PERIPHERAL_POWER, MOSAICO_POWER_ON_LEVEL);
  delay(100);
}
