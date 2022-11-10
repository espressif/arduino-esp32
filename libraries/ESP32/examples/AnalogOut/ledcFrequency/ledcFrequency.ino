/*
 * This sketch will map the maximum frequency depending on the bit resolution for the current SoC.
 * Run the sketch and wait for the Final report.
 * Ignore the error messages from incorrect settings such as these:
 *  "E (4190) ledc: requested frequency and duty resolution can not be achieved, try reducing freq_hz or duty_resolution. div_param=255"
 *
 * Date: 10 Nov 2022
 * Author: Tomas Pilny
 */

#include "soc/soc_caps.h"

#define PIN 2

void setup() {
  ledcAttachPin(PIN,analogGetChannel(PIN));
  analogWrite(PIN,128);

  uint32_t min_frequency;
  uint32_t max_frequency;
  uint32_t frequency;
  uint32_t freq_array[SOC_LEDC_TIMER_BIT_WIDE_NUM];
  delay(500);

  for(uint8_t resolution = 1; resolution <= SOC_LEDC_TIMER_BIT_WIDE_NUM; ++resolution){
    freq_array[resolution-1] = 0;
    min_frequency = 0;
    max_frequency = UINT32_MAX;
    while(min_frequency != max_frequency && min_frequency+1 != max_frequency){
      frequency = min_frequency + ((max_frequency-min_frequency)/2);
      if(ledcChangeFrequency(analogGetChannel(PIN), frequency, resolution)){
        min_frequency = frequency;
      }else{
        max_frequency = frequency;
        if(frequency == 1){ // Cannot go lower, report max frequency "0"
          frequency = 0;
        }
      }
    } // while not found the maximum
    freq_array[resolution-1] = frequency;
  } // for all resolutions

  printf("Bit resolution | Max Frequency [Hz]\n");
  for(uint8_t r = 1; r <= SOC_LEDC_TIMER_BIT_WIDE_NUM; ++r){
    printf("             %d | %u\n", r, freq_array[r-1]);
  }
}

void loop()
{
  delay(1000);
}
