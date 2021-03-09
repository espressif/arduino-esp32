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
