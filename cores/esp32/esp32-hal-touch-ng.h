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

#ifndef MAIN_ESP32_HAL_TOUCH_NEW_H_
#define MAIN_ESP32_HAL_TOUCH_NEW_H_

#include "soc/soc_caps.h"
#if SOC_TOUCH_SENSOR_SUPPORTED
#if SOC_TOUCH_SENSOR_VERSION == 3  // ESP32P4

#ifdef __cplusplus
extern "C" {
#endif

#include "esp32-hal.h"

typedef uint32_t touch_value_t;

/*
 * Set time in us that measurement operation takes
 * The result from touchRead, threshold and detection
 * accuracy depend on these values.
 * Note: must be called before setting up touch pads
 **/
void touchSetTiming(float measure, uint32_t sleep);

/*
 * Tune the touch pad frequency.
 * Note: Must be called before setting up touch pads
*/
void touchSetConfig(uint32_t _div_num, uint8_t coarse_freq_tune, uint8_t fine_freq_tune);

/*
 * Read touch pad value.
 * You can use this method to chose a good threshold value
 * to use as value for touchAttachInterrupt.
 * */
touch_value_t touchRead(uint8_t pin);

/*
 * Set function to be called if touch pad value rises by given increment (threshold).
 * Use touchRead to determine a proper threshold between touched and untouched state.
 * */
void touchAttachInterrupt(uint8_t pin, void (*userFunc)(void), touch_value_t threshold);
void touchAttachInterruptArg(uint8_t pin, void (*userFunc)(void *), void *arg, touch_value_t threshold);
void touchDetachInterrupt(uint8_t pin);

/*
 * Returns true when the latest ISR status for the Touchpad is that it is touched (Active)
 * and false when the Touchpad is untoouched (Inactive).
 * This function can be used in conjunction with ISR User callback in order to take action
 * as soon as the touchpad is touched and/or released.
 **/
bool touchInterruptGetLastStatus(uint8_t pin);

/*
 * Set the default threshold for touch pads.
 * The threshold is a percentage of the benchmark value.
 * The default value is 1.5%.
 **/
void touchSetDefaultThreshold(float percentage);

/*
 * Setup touch pad wake up from deep sleep /light sleep with given threshold.
 * When light sleep is used, all used touch pads will be able to wake up the chip.
 **/
void touchSleepWakeUpEnable(uint8_t pin, touch_value_t threshold);

#ifdef __cplusplus
}
#endif

#endif /* SOC_TOUCH_SENSOR_VERSION == 3 */
#endif /* SOC_TOUCH_SENSOR_SUPPORTED */
#endif /* MAIN_ESP32_HAL_TOUCH_H_ */
