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

#ifndef MAIN_ESP32_HAL_ADC_H_
#define MAIN_ESP32_HAL_ADC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "esp32-hal.h"

/*
 * Get ADC value for pin
 * */
uint16_t analogRead(uint8_t pin);

/*
 * Sets the sample bits (9 - 12)
 * */
void analogSetWidth(uint8_t bits);

/*
 * Set number of cycles per sample
 * Default is 8 and seems to do well
 * Range is 1 - 255
 * */
void analogSetCycles(uint8_t cycles);

/*
 * Set number of samples in the range.
 * Default is 1
 * Range is 1 - 255
 * This setting splits the range into
 * "samples" pieces, which could look
 * like the sensitivity has been multiplied
 * that many times
 * */
void analogSetSamples(uint8_t samples);

/*
 * Set the divider for the ADC clock.
 * Default is 1
 * Range is 1 - 255
 * */
void analogSetClockDiv(uint8_t clockDiv);

/*
 * Get value for HALL sensor (without LNA)
 * connected to pins 36(SVP) and 39(SVN)
 * */
int hallRead();

#ifdef __cplusplus
}
#endif

#endif /* MAIN_ESP32_HAL_ADC_H_ */
