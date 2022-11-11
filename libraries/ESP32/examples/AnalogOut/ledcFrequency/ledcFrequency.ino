/*
 * This sketch will map the maximum frequency depending on the bit resolution for the current SoC.
 * Run the sketch and wait for the Final report.
 * Ignore the error messages from incorrect settings such as these:
 *  "E (4190) ledc: requested frequency and duty resolution can not be achieved, try reducing freq_hz or duty_resolution. div_param=255"
 *
 * Date: 11 Nov 2022
 * Author: Tomas Pilny
 */

#include "soc/soc_caps.h"

#define PIN 2

void setup() {
  analogWrite(PIN,128);

  uint32_t min_frequency;
  uint32_t max_frequency;
  uint32_t frequency;
  uint32_t successful_frequency;
  uint32_t max_freq_array[SOC_LEDC_TIMER_BIT_WIDE_NUM];
  uint32_t min_freq_array[SOC_LEDC_TIMER_BIT_WIDE_NUM];

  // Find Max Frequency
  for(uint8_t resolution = 1; resolution <= SOC_LEDC_TIMER_BIT_WIDE_NUM; ++resolution){
    max_freq_array[resolution-1] = 0;
    min_frequency = 0;
    max_frequency = UINT32_MAX;
    successful_frequency = 0;
    while(min_frequency != max_frequency && min_frequency+1 != max_frequency){
      frequency = min_frequency + ((max_frequency-min_frequency)/2);
      if(ledcChangeFrequency(analogGetChannel(PIN), frequency, resolution)){
        min_frequency = frequency;
        successful_frequency = frequency;
      }else{
        max_frequency = frequency;
      }
    } // while not found the maximum
    max_freq_array[resolution-1] = successful_frequency;
  } // for all resolutions

  // Find Min Frequency
  for(uint8_t resolution = 1; resolution <= SOC_LEDC_TIMER_BIT_WIDE_NUM; ++resolution){
    min_freq_array[resolution-1] = 0;
    min_frequency = 0;
    max_frequency = max_freq_array[resolution-1];
    successful_frequency = max_frequency;
    while(min_frequency != max_frequency && min_frequency+1 != max_frequency){
      frequency = min_frequency + ((max_frequency-min_frequency)/2);
      if(ledcChangeFrequency(analogGetChannel(PIN), frequency, resolution)){
        max_frequency = frequency;
        successful_frequency = frequency;
      }else{
        min_frequency = frequency;
      }
    } // while not found the maximum
    min_freq_array[resolution-1] = successful_frequency;
  } // for all resolutions

  printf("Bit resolution | Min Frequency [Hz] | Max Frequency [Hz]\n");
  for(uint8_t r = 1; r <= SOC_LEDC_TIMER_BIT_WIDE_NUM; ++r){
    size_t max_len = std::to_string(UINT32_MAX).length();
    printf("            %s%d |         %s%u |         %s%u\n",
      std::string (2 - std::to_string(r).length(), ' ').c_str(), r,
      std::string (max_len - std::to_string(min_freq_array[r-1]).length(), ' ').c_str(),
      min_freq_array[r-1],
      std::string (max_len - std::to_string(max_freq_array[r-1]).length(), ' ').c_str(),
      max_freq_array[r-1]);
  }
}

void loop()
{
  delay(1000);
}
