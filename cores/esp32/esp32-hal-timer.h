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

#include "esp32-hal.h"
#include "driver/gptimer_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Keep previous compatibility with 2.X
 */
typedef gptimer_handle_t hw_timer_t;

hw_timer_t timerBegin(uint32_t resolution, bool countUp);
void timerEnd(hw_timer_t timer);

void timerAttachInterrupt(hw_timer_t timer, void (*fn)(void));
void timerDetachInterrupt(hw_timer_t timer);

void timerStart(hw_timer_t timer);
void timerStop(hw_timer_t timer);
void timerRestart(hw_timer_t timer);
void timerWrite(hw_timer_t timer, uint64_t val);

uint64_t timerRead(hw_timer_t timer);

uint64_t timerReadMicros(hw_timer_t timer);
uint64_t timerReadMilis(hw_timer_t timer);
double timerReadSeconds(hw_timer_t timer);

uint32_t timerGetResolution(hw_timer_t timer);

void timerAlarmWrite(hw_timer_t timer, uint64_t alarm_value, bool autoreload, uint64_t reload_count);

#ifdef __cplusplus
}
#endif
