/*
 Arduino.h - Main include file for the Arduino SDK
 Copyright (c) 2005-2013 Arduino Team.  All right reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include "soc/soc_caps.h"
#if SOC_ADC_SUPPORTED

#ifdef __cplusplus
extern "C" {
#endif

#include "esp32-hal.h"

typedef enum {
  ADC_0db,
  ADC_2_5db,
  ADC_6db,
  ADC_11db,
  ADC_ATTENDB_MAX
} adc_attenuation_t;

/*
 * Get ADC value for pin
 * */
uint16_t analogRead(uint8_t pin);

/*
 * Get MilliVolts value for pin
 * */
uint32_t analogReadMilliVolts(uint8_t pin);

/*
 * Set the resolution of analogRead return values. Default is 12 bits (range from 0 to 4096).
 * If between 9 and 12, it will equal the set hardware resolution, else value will be shifted.
 * Range is 1 - 16
 *
 * Note: compatibility with Arduino SAM
 */
void analogReadResolution(uint8_t bits);

/*
 * Set the attenuation for all channels
 * Default is 11db
 * */
void analogSetAttenuation(adc_attenuation_t attenuation);

/*
 * Set the attenuation for particular pin
 * Default is 11db
 * */
void analogSetPinAttenuation(uint8_t pin, adc_attenuation_t attenuation);

#if CONFIG_IDF_TARGET_ESP32
/*
 * Sets the sample bits and read resolution
 * Default is 12bit (0 - 4095)
 * Range is 9 - 12
 * */
void analogSetWidth(uint8_t bits);

#endif

/*
 * Analog Continuous mode
 * */

typedef struct {
  uint8_t pin;         /*!<ADC pin */
  uint8_t channel;     /*!<ADC channel */
  int avg_read_raw;    /*!<ADC average raw data */
  int avg_read_mvolts; /*!<ADC average voltage in mV */
} adc_continuous_data_t;

/*
 * Setup ADC continuous peripheral
 * */
bool analogContinuous(const uint8_t pins[], size_t pins_count, uint32_t conversions_per_pin, uint32_t sampling_freq_hz, void (*userFunc)(void));

/*
 * Read ADC continuous conversion data
 * */
bool analogContinuousRead(adc_continuous_data_t **buffer, uint32_t timeout_ms);

/*
 * Start ADC continuous conversions
 * */
bool analogContinuousStart();

/*
 * Stop ADC continuous conversions
 * */
bool analogContinuousStop();

/*
 * Deinitialize ADC continuous peripheral
 * */
bool analogContinuousDeinit();

/*
 * Sets the attenuation for continuous mode reading
 * Default is 11db
 * */
void analogContinuousSetAtten(adc_attenuation_t attenuation);

/*
 * Sets the read resolution for continuous mode
 * Default is 12bit (0 - 4095)
 * Range is 9 - 12
 * */
void analogContinuousSetWidth(uint8_t bits);

#ifdef __cplusplus
}
#endif

#endif /* SOC_ADC_SUPPORTED */
