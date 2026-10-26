// Copyright 2026 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <sdkconfig.h>
#ifdef CONFIG_ESP_MATTER_ENABLE_DATA_MODEL

#include <stddef.h>
#include <stdlib.h>

#include <esp32-hal-log.h>
#include <esp_attr.h>
#include <esp_heap_caps.h>

// Harvested libespressif__esp_matter.a exports C++ (not extern "C") names:
//   _Z21esp_matter_mem_callocjj
//   _Z22esp_matter_mem_reallocPvj
//   _Z19esp_matter_mem_freePv
// Callers are other TUs, so --wrap in platform.txt / CMakeLists.txt redirects them.
// Do not call __real_: that is DEFAULT calloc() and keeps <=4096-byte blocks in DRAM.

static bool matterMemUseSpiram() {
  static bool cached = false;
  static bool haveSpiram = false;
  if (!cached) {
    haveSpiram = heap_caps_get_total_size(MALLOC_CAP_SPIRAM) > 0;
    cached = true;
    log_d("Matter data-model heap: %s", haveSpiram ? "SPIRAM (fallback INTERNAL)" : "INTERNAL");
  }
  return haveSpiram;
}

extern "C" IRAM_ATTR void *__wrap__Z21esp_matter_mem_callocjj(size_t n, size_t size) {
  if (matterMemUseSpiram()) {
    void *p = heap_caps_calloc(n, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (p) {
      return p;
    }
  }
  return heap_caps_calloc(n, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

extern "C" IRAM_ATTR void *__wrap__Z22esp_matter_mem_reallocPvj(void *ptr, size_t size) {
  if (matterMemUseSpiram()) {
    void *p = heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (p) {
      return p;
    }
  }
  return heap_caps_realloc(ptr, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

extern "C" IRAM_ATTR void __wrap__Z19esp_matter_mem_freePv(void *ptr) {
  free(ptr);
}

#endif  // CONFIG_ESP_MATTER_ENABLE_DATA_MODEL
