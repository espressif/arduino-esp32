// Copyright 2015-2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MAIN_ESP32_HAL_ADC_H_
#define MAIN_ESP32_HAL_ADC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "esp32-hal.h"

typedef enum {
    ADC_0db,
    ADC_2_5db,
    ADC_6db,
    ADC_11db
} adc_attenuation_t;

/*
 * Get ADC value for pin
 * */
uint16_t analogRead(uint8_t pin);

/*
 * Set the resolution of analogRead return values. Default is 12 bits (range from 0 to 4096).
 * If between 9 and 12, it will equal the set hardware resolution, else value will be shifted.
 * Range is 1 - 16
 *
 * Note: compatibility with Arduino SAM
 */
void analogReadResolution(uint8_t bits);

/*
 * Sets the sample bits and read resolution
 * Default is 12bit (0 - 4095)
 * Range is 9 - 12
 * */
void analogSetWidth(uint8_t bits);

/*
 * Set the divider for the ADC clock.
 * Default is 1
 * Range is 1 - 255
 * */
void analogSetClockDiv(uint8_t clockDiv);

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

/*
 * Get value for HALL sensor (without LNA)
 * connected to pins 36(SVP) and 39(SVN)
 * */
int hallRead();

/*
 * Attach pin to ADC (will also clear any other analog mode that could be on)
 * */
bool adcAttachPin(uint8_t pin);

/*
 * Set pin to use for ADC calibration if the esp is not already calibrated (25, 26 or 27)
 * */
void analogSetVRefPin(uint8_t pin);

/*
 * Get MilliVolts value for pin
 * */
uint32_t analogReadMilliVolts(uint8_t pin);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_ESP32_HAL_ADC_H_ */
