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

#include "sdkconfig.h"
#include "soc/soc_caps.h"

// When adding a new target, update the appropriate group(s) below

// Targets that support XTAL frequency queries via rtc_clk_xtal_freq_get()
#if (!defined(CONFIG_IDF_TARGET_ESP32C5) && !defined(CONFIG_IDF_TARGET_ESP32P4))
#define TARGET_HAS_XTAL_FREQ 1
#else
#define TARGET_HAS_XTAL_FREQ 0
#endif

// Targets that need dynamic APB frequency updates via rtc_clk_apb_freq_update()
#if (defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3))
#define TARGET_HAS_DYNAMIC_APB 1
#else
#define TARGET_HAS_DYNAMIC_APB 0
#endif

// Xtensa architecture targets that need FreeRTOS tick divisor updates
#if (defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2))
#define TARGET_HAS_XTENSA_TICK 1
#else
#define TARGET_HAS_XTENSA_TICK 0
#endif

// Targets with APLL support (uses IDF SOC capability macro)
// Note: ESP32-P4 APLL support is not yet fully implemented in IDF
#if (defined(SOC_CLK_APLL_SUPPORTED) && !defined(CONFIG_IDF_TARGET_ESP32P4))
#define TARGET_HAS_APLL 1
#else
#define TARGET_HAS_APLL 0
#endif

// Targets grouped by maximum CPU frequency support

#if (defined(CONFIG_IDF_TARGET_ESP32P4))
#define TARGET_CPU_FREQ_MAX_400 1
#else
#define TARGET_CPU_FREQ_MAX_400 0
#endif

#if (defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3))
#define TARGET_CPU_FREQ_MAX_240 1
#else
#define TARGET_CPU_FREQ_MAX_240 0
#endif

#if (defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32C61))
#define TARGET_CPU_FREQ_MAX_160 1
#else
#define TARGET_CPU_FREQ_MAX_160 0
#endif

#if (defined(CONFIG_IDF_TARGET_ESP32C2))
#define TARGET_CPU_FREQ_MAX_120 1
#else
#define TARGET_CPU_FREQ_MAX_120 0
#endif

#if (defined(CONFIG_IDF_TARGET_ESP32H2))
#define TARGET_CPU_FREQ_MAX_96 1
#else
#define TARGET_CPU_FREQ_MAX_96 0
#endif

typedef enum {
  APB_BEFORE_CHANGE,
  APB_AFTER_CHANGE
} apb_change_ev_t;

typedef void (*apb_change_cb_t)(void *arg, apb_change_ev_t ev_type, uint32_t old_apb, uint32_t new_apb);

bool addApbChangeCallback(void *arg, apb_change_cb_t cb);
bool removeApbChangeCallback(void *arg, apb_change_cb_t cb);

//function takes the following frequencies as valid values:
//  240, 160, 80    <<< For all XTAL types
//  40, 20, 10      <<< For 40MHz XTAL
//  26, 13          <<< For 26MHz XTAL
//  24, 12          <<< For 24MHz XTAL
bool setCpuFrequencyMhz(uint32_t cpu_freq_mhz);

const char *getSupportedCpuFrequencyMhz(uint8_t xtal);
const char *getClockSourceName(uint8_t source);
uint32_t getCpuFrequencyMhz();   // In MHz
uint32_t getXtalFrequencyMhz();  // In MHz
uint32_t getApbFrequency();      // In Hz

#ifdef __cplusplus
}
#endif

#endif /* _ESP32_HAL_CPU_H_ */
