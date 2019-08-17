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

#ifndef MAIN_ESP32_HAL_PWM_H_
#define MAIN_ESP32_HAL_PWM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "esp32-hal.h"



/*
 * Set PWM valut to a pin
 * */
void analogWrite(uint8_t pin,uint16_t val);

/*
 * set pwm value to pin
 *  
 */
void analogWriteResolution(uint8_t bits);

/*
 * set pwm resolution
 * */
void analogWriteFrequency(uint16_t feq);

/*
 * Set pwm frequency

 * */


#ifdef __cplusplus
}
#endif

#endif /* MAIN_ESP32_HAL_ADC_H_ */
