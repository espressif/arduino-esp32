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

#ifndef _ESP32_HAL_PSRAM_H_
#define _ESP32_HAL_PSRAM_H_

#ifdef __cplusplus
extern "C" {
#endif

bool psramInit();
bool psramFound();

void *ps_malloc(size_t size);
void *ps_calloc(size_t n, size_t size);
void *ps_realloc(void *ptr, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* _ESP32_HAL_PSRAM_H_ */
