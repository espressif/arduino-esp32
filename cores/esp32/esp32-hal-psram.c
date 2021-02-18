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

#include "esp32-hal.h"

#if CONFIG_SPIRAM_SUPPORT
#include "esp_spiram.h"
#include "soc/efuse_reg.h"
#include "esp_heap_caps.h"

static volatile bool spiramDetected = false;
static volatile bool spiramFailed = false;

bool psramInit(){
    if (spiramDetected) {
        return true;
    }
#ifndef CONFIG_SPIRAM_BOOT_INIT
    if (spiramFailed) {
        return false;
    }
    uint32_t chip_ver = REG_GET_FIELD(EFUSE_BLK0_RDATA3_REG, EFUSE_RD_CHIP_VER_PKG);
    uint32_t pkg_ver = chip_ver & 0x7;
    if (pkg_ver == EFUSE_RD_CHIP_VER_PKG_ESP32D2WDQ5 || pkg_ver == EFUSE_RD_CHIP_VER_PKG_ESP32PICOD2) {
        spiramFailed = true;
        log_w("PSRAM not supported!");
        return false;
    }
    esp_spiram_init_cache();
    if (esp_spiram_init() != ESP_OK) {
        spiramFailed = true;
        log_w("PSRAM init failed!");
        pinMatrixOutDetach(16, false, false);
        pinMatrixOutDetach(17, false, false);
        return false;
    }
    if (!esp_spiram_test()) {
        spiramFailed = true;
        log_e("PSRAM test failed!");
        return false;
    }
    if (esp_spiram_add_to_heapalloc() != ESP_OK) {
        spiramFailed = true;
        log_e("PSRAM could not be added to the heap!");
        return false;
    }
#endif
    spiramDetected = true;
    log_d("PSRAM enabled");
    return true;
}

bool IRAM_ATTR psramFound(){
    return spiramDetected;
}

void IRAM_ATTR *ps_malloc(size_t size){
    if(!spiramDetected){
        return NULL;
    }
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

void IRAM_ATTR *ps_calloc(size_t n, size_t size){
    if(!spiramDetected){
        return NULL;
    }
    return heap_caps_calloc(n, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

void IRAM_ATTR *ps_realloc(void *ptr, size_t size){
    if(!spiramDetected){
        return NULL;
    }
    return heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

#else

bool psramInit(){
    return false;
}

bool IRAM_ATTR psramFound(){
    return false;
}

void IRAM_ATTR *ps_malloc(size_t size){
    return NULL;
}

void IRAM_ATTR *ps_calloc(size_t n, size_t size){
    return NULL;
}

void IRAM_ATTR *ps_realloc(void *ptr, size_t size){
    return NULL;
}

#endif
