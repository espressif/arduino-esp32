// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
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

#ifndef _ESP32_HAL_SD_H_
#define _ESP32_HAL_SD_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

//channel 0-7 freq 1220-312500 duty 0-255
uint32_t    sigmaDeltaSetup(uint8_t channel, uint32_t freq);
void        sigmaDeltaWrite(uint8_t channel, uint8_t duty);
uint8_t     sigmaDeltaRead(uint8_t channel);
void        sigmaDeltaAttachPin(uint8_t pin, uint8_t channel);
void        sigmaDeltaDetachPin(uint8_t pin);


#ifdef __cplusplus
}
#endif

#endif /* _ESP32_HAL_SD_H_ */
