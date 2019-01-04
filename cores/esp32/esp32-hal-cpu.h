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

#ifndef _ESP32_HAL_CPU_H_
#define _ESP32_HAL_CPU_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

typedef enum { APB_BEFORE_CHANGE, APB_AFTER_CHANGE, APB_ABORT_CHANGE} apb_change_ev_t;

typedef bool (* apb_change_cb_t)(void * arg, apb_change_ev_t ev_type, uint32_t old_apb, uint32_t new_apb);

bool addApbChangeCallback(void * arg, apb_change_cb_t cb);
bool removeApbChangeCallback(void * arg, apb_change_cb_t cb);
bool runCallbackList(void);

//function takes the following frequencies as valid values:
//  240, 160, 80                     <<< For all XTAL types
//  40, 20, 13, 10, 8, 5, 4, 3, 2, 1 <<< For 40MHz XTAL
//  26, 13, 5, 4, 3, 2, 1            <<< For 26MHz XTAL
//  24, 12, 8, 6, 4, 3, 2, 1         <<< For 24MHz XTAL
bool setCpuFrequency(uint32_t cpu_freq_mhz);

uint32_t getCpuFrequencyMHz(); // In MHz
uint32_t getApbFrequency(); // In Hz

#define D_A 18
#define D_B 19
 
void twiddle( const char* pattern);

#ifdef __cplusplus
}
#endif

#endif /* _ESP32_HAL_CPU_H_ */
