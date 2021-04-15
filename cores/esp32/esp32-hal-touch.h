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

#ifndef MAIN_ESP32_HAL_TOUCH_H_
#define MAIN_ESP32_HAL_TOUCH_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "esp32-hal.h"

/*
 * Set cycles that measurement operation takes
 * The result from touchRead, threshold and detection
 * accuracy depend on these values. Defaults are
 * 0x1000 for measure and 0x1000 for sleep.
 * With default values touchRead takes 0.5ms
 * */
void touchSetCycles(uint16_t measure, uint16_t sleep);

/*
 * Read touch pad (values close to 0 mean touch detected)
 * You can use this method to chose a good threshold value
 * to use as value for touchAttachInterrupt
 * */
uint16_t touchRead(uint8_t pin);

/*
 * Set function to be called if touch pad value falls
 * below the given threshold. Use touchRead to determine
 * a proper threshold between touched and untouched state
 * */
void touchAttachInterrupt(uint8_t pin, void (*userFunc)(void), uint16_t threshold);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_ESP32_HAL_TOUCH_H_ */
