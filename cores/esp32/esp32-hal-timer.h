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
