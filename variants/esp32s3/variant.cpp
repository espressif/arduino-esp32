#include "esp32-hal-rgb-led.h"

extern void ARDUINO_ISR_ATTR __pinMode(uint8_t pin, uint8_t mode)
{
  log_d("foo");
#ifdef BOARD_HAS_NEOPIXEL
    if (pin == LED_BUILTIN){
        __pinMode(LED_BUILTIN-SOC_GPIO_PIN_COUNT, mode);
        return;
    }
#endif
  __pinMode(pin, mode);
}