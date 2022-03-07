#include <Arduino.h>
#include "driver/ledc.h"

#define BITS LEDC_TIMER_11_BIT // up to more than 25 kHz

void noTone(uint8_t _pin){
  ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
}

// duty in %
void tone(uint8_t _pin, unsigned int frequency, unsigned long duration, unsigned long duty){
  if(duty > 100){
    log_e("Invalid duty parameter (%u) - must be between 0 and 100; Duty is in percents!", duty);
    return;
  }
  ledc_timer_config_t ledc_timer = {
    .speed_mode = LEDC_LOW_SPEED_MODE,    // timer mode
    .duty_resolution = BITS,              // resolution of PWM duty
    .timer_num = LEDC_TIMER_0,            // timer index
    .freq_hz = frequency,                 // frequency of PWM signal
    .clk_cfg = LEDC_AUTO_CLK              // Auto select the source clock
  };
  esp_err_t ret = ledc_timer_config(&ledc_timer);
  if(ret){
    log_e("Could not start tone; ledc_timer_config returned %d", ret);
    return;
  }

  ledc_channel_config_t ledc_channel = {
    .gpio_num   = _pin,                                /*!< the LEDC output gpio_num, if you want to use gpio16, gpio_num = 16 */
    .speed_mode = LEDC_LOW_SPEED_MODE,                 /*!< LEDC speed speed_mode, high-speed mode or low-speed mode */
    .channel    = LEDC_CHANNEL_0,                      /*!< LEDC channel (0 - 7) */
    .intr_type  = LEDC_INTR_DISABLE,                   /*!< configure interrupt, Fade interrupt enable or Fade interrupt disable */
    .timer_sel  = LEDC_TIMER_0,                        /*!< Select the timer source of channel (0 - 3) */
    .duty       = duty * ((1<<BITS)/100),              /*!< LEDC channel duty, the range of duty setting is [0, (2**duty_resolution)] */
    .hpoint     = 0,                                   /*!< LEDC channel hpoint value, the max value is 0xfffff */
    .flags = {0}
  };
  ret = ledc_channel_config(&ledc_channel);
  if(ret){
    log_e("Could not start tone; ledc_channel_config returned %d", ret);
    return;
  }

  if(duration){
    vTaskDelay(pdMS_TO_TICKS(duration));
    noTone(_pin);
  }
}
