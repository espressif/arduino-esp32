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

#ifndef _ESP32_HAL_PSRAM_H_
#define _ESP32_HAL_PSRAM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "sdkconfig.h"

#if defined(BOARD_HAS_PSRAM) && BOARD_HAS_PSRAM == 0 // Arduino Build with PSRAM Off
#ifdef CONFIG_SPIRAM_SUPPORT
#undef CONFIG_SPIRAM_SUPPORT
#endif
#ifdef CONFIG_SPIRAM
#undef CONFIG_SPIRAM
#endif
#elif !defined(BOARD_HAS_PSRAM) // ESP-IDF Build with undefined BOARD_HAS_PSRAM
#if CONFIG_SPIRAM_SUPPORT || CONFIG_SPIRAM
#define BOARD_HAS_PSRAM 1
#else
#define BOARD_HAS_PSRAM 0
#endif
#endif

bool psramInit();
bool psramAddToHeap();
bool psramFound();

void *ps_malloc(size_t size);
void *ps_calloc(size_t n, size_t size);
void *ps_realloc(void *ptr, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* _ESP32_HAL_PSRAM_H_ */
