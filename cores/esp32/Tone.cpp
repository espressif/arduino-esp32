#include <Arduino.h>
#include "esp32-hal-ledc.h"

void noTone(uint8_t _pin){
  ledcDetachPin(_pin);
}

// parameters:
// _pin - pin number which will output the signal
// frequency - PWM frequency in Hz
// duration - time in ms - how long will the signal be outputted.
//   If not provided, or 0 you must manually call noTone to end output
void tone(uint8_t _pin, unsigned int frequency, unsigned long duration){
  ledcSetup(0, frequency, 11);
  ledcAttachPin(_pin, 0);

  ledcWrite(0, 1024);
  if(duration){
    vTaskDelay(pdMS_TO_TICKS(duration));
    noTone(_pin);
  }
}
