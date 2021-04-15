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

#ifndef MAIN_ESP32_HAL_TIMER_H_
#define MAIN_ESP32_HAL_TIMER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "esp32-hal.h"
#include "freertos/FreeRTOS.h"

struct hw_timer_s;
typedef struct hw_timer_s hw_timer_t;

hw_timer_t * timerBegin(uint8_t timer, uint16_t divider, bool countUp);
void timerEnd(hw_timer_t *timer);

void timerSetConfig(hw_timer_t *timer, uint32_t config);
uint32_t timerGetConfig(hw_timer_t *timer);

void timerAttachInterrupt(hw_timer_t *timer, void (*fn)(void), bool edge);
void timerDetachInterrupt(hw_timer_t *timer);

void timerStart(hw_timer_t *timer);
void timerStop(hw_timer_t *timer);
void timerRestart(hw_timer_t *timer);
void timerWrite(hw_timer_t *timer, uint64_t val);
void timerSetDivider(hw_timer_t *timer, uint16_t divider);
void timerSetCountUp(hw_timer_t *timer, bool countUp);
void timerSetAutoReload(hw_timer_t *timer, bool autoreload);

bool timerStarted(hw_timer_t *timer);
uint64_t timerRead(hw_timer_t *timer);
uint64_t timerReadMicros(hw_timer_t *timer);
double timerReadSeconds(hw_timer_t *timer);
uint16_t timerGetDivider(hw_timer_t *timer);
bool timerGetCountUp(hw_timer_t *timer);
bool timerGetAutoReload(hw_timer_t *timer);

void timerAlarmEnable(hw_timer_t *timer);
void timerAlarmDisable(hw_timer_t *timer);
void timerAlarmWrite(hw_timer_t *timer, uint64_t interruptAt, bool autoreload);

bool timerAlarmEnabled(hw_timer_t *timer);
uint64_t timerAlarmRead(hw_timer_t *timer);
uint64_t timerAlarmReadMicros(hw_timer_t *timer);
double timerAlarmReadSeconds(hw_timer_t *timer);


#ifdef __cplusplus
}
#endif

#endif /* MAIN_ESP32_HAL_TIMER_H_ */
