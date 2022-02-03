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

#include "soc/soc_caps.h"
#include "esp32-hal.h"

#if SOC_TOUCH_SENSOR_NUM > 0

#if !defined(SOC_TOUCH_VERSION_1) && !defined(SOC_TOUCH_VERSION_2)
#error Touch IDF driver Not supported!
#endif

#if SOC_TOUCH_VERSION_1 // ESP32
typedef uint16_t touch_value_t;
#elif SOC_TOUCH_VERSION_2 // ESP32S2 ESP32S3
typedef uint32_t touch_value_t;
#endif

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
touch_value_t touchRead(uint8_t pin);

/*
 * Set function to be called if touch pad value falls (ESP32)
 * below the given threshold / rises (ESP32-S2/S3) by given increment (threshold). 
 * Use touchRead to determine a proper threshold between touched and untouched state
 * */
void touchAttachInterrupt(uint8_t pin, void (*userFunc)(void), touch_value_t threshold);
void touchAttachInterruptArg(uint8_t pin, void (*userFunc)(void*), void *arg, touch_value_t threshold);
void touchDetachInterrupt(uint8_t pin);

/*
 * Specific functions to ESP32 
 * Tells the driver if it shall activate the ISR if the sensor is Lower or Higher than the Threshold
 * Default if Lower.
 **/

#if SOC_TOUCH_VERSION_1     // Only for ESP32 SoC
void touchInterruptSetThresholdDirection(bool mustbeLower);
#endif


/*
 * Specific functions to ESP32-S2 and ESP32-S3
 * Returns true when the latest ISR status for the Touchpad is that it is touched (Active)
 * and false when the Touchpad is untoouched (Inactive)
 * This function can be used in conjunction with ISR User callback in order to take action 
 * as soon as the touchpad is touched and/or released
 **/

#if SOC_TOUCH_VERSION_2     // Only for ESP32S2 and ESP32S3
// returns true if touch pad has been and continues pressed and false otherwise 
bool touchInterruptGetLastStatus(uint8_t pin);
#endif

#endif // SOC_TOUCH_SENSOR_NUM > 0

#ifdef __cplusplus
}
#endif
#endif /* MAIN_ESP32_HAL_TOUCH_H_ */
