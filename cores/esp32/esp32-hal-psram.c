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

#include "esp32-hal.h"

#if CONFIG_SPIRAM_SUPPORT || CONFIG_SPIRAM
#include "soc/efuse_reg.h"
#include "esp_heap_caps.h"

#include "esp_system.h"
#ifdef ESP_IDF_VERSION_MAJOR // IDF 4+
#if CONFIG_IDF_TARGET_ESP32 // ESP32/PICO-D4
#include "esp32/spiram.h"
#elif CONFIG_IDF_TARGET_ESP32S2
#include "esp32s2/spiram.h"
#include "esp32s2/rom/cache.h"
#elif CONFIG_IDF_TARGET_ESP32S3
#include "esp32s3/spiram.h"
#include "esp32s3/rom/cache.h"
#else 
#error Target CONFIG_IDF_TARGET is not supported
#endif
#else // ESP32 Before IDF 4.0
#include "esp_spiram.h"
#endif

static volatile bool spiramDetected = false;
static volatile bool spiramFailed = false;

//allows user to bypass SPI RAM test routine
__attribute__((weak)) bool testSPIRAM(void) 
{ 
     return esp_spiram_test(); 
}


bool psramInit(){
    if (spiramDetected) {
        return true;
    }
#ifndef CONFIG_SPIRAM_BOOT_INIT
    if (spiramFailed) {
        return false;
    }
#if CONFIG_IDF_TARGET_ESP32
    uint32_t chip_ver = REG_GET_FIELD(EFUSE_BLK0_RDATA3_REG, EFUSE_RD_CHIP_VER_PKG);
    uint32_t pkg_ver = chip_ver & 0x7;
    if (pkg_ver == EFUSE_RD_CHIP_VER_PKG_ESP32D2WDQ5 || pkg_ver == EFUSE_RD_CHIP_VER_PKG_ESP32PICOD2) {
        spiramFailed = true;
        log_w("PSRAM not supported!");
        return false;
    }
#elif CONFIG_IDF_TARGET_ESP32S2
    extern void esp_config_data_cache_mode(void);
    esp_config_data_cache_mode();
    Cache_Enable_DCache(0);
#endif
    if (esp_spiram_init() != ESP_OK) {
        spiramFailed = true;
        log_w("PSRAM init failed!");
#if CONFIG_IDF_TARGET_ESP32
        if (pkg_ver != EFUSE_RD_CHIP_VER_PKG_ESP32PICOD4) {
            pinMatrixOutDetach(16, false, false);
            pinMatrixOutDetach(17, false, false);
        }
#endif
        return false;
    }
    esp_spiram_init_cache();
    //testSPIRAM() allows user to bypass SPI RAM test routine
    if (!testSPIRAM()) {
        spiramFailed = true;
        log_e("PSRAM test failed!");
        return false;
    }
    if (esp_spiram_add_to_heapalloc() != ESP_OK) {
        spiramFailed = true;
        log_e("PSRAM could not be added to the heap!");
        return false;
    }
#if CONFIG_SPIRAM_USE_MALLOC && !CONFIG_ARDUINO_ISR_IRAM
    heap_caps_malloc_extmem_enable(CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL);
#endif
#endif /* CONFIG_SPIRAM_BOOT_INIT */
    log_i("PSRAM enabled");
    spiramDetected = true;
    return true;
}

bool ARDUINO_ISR_ATTR psramFound(){
    return spiramDetected;
}

void ARDUINO_ISR_ATTR *ps_malloc(size_t size){
    if(!spiramDetected){
        return NULL;
    }
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

void ARDUINO_ISR_ATTR *ps_calloc(size_t n, size_t size){
    if(!spiramDetected){
        return NULL;
    }
    return heap_caps_calloc(n, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

void ARDUINO_ISR_ATTR *ps_realloc(void *ptr, size_t size){
    if(!spiramDetected){
        return NULL;
    }
    return heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

#else

bool psramInit(){
    return false;
}

bool ARDUINO_ISR_ATTR psramFound(){
    return false;
}

void ARDUINO_ISR_ATTR *ps_malloc(size_t size){
    return NULL;
}

void ARDUINO_ISR_ATTR *ps_calloc(size_t n, size_t size){
    return NULL;
}

void ARDUINO_ISR_ATTR *ps_realloc(void *ptr, size_t size){
    return NULL;
}

#endif
