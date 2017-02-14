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
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_attr.h"
#include "nvs_flash.h"

#if !CONFIG_ESP32_PHY_AUTO_INIT
#include "esp_phy_init.h"
#include "rom/rtc.h"
void arduino_phy_init()
{
    static bool initialized = false;
    if(initialized){
        return;
    }
    esp_phy_calibration_mode_t calibration_mode = PHY_RF_CAL_PARTIAL;
    if (rtc_get_reset_reason(0) == DEEPSLEEP_RESET) {
        calibration_mode = PHY_RF_CAL_NONE;
    }
    const esp_phy_init_data_t* init_data = esp_phy_get_init_data();
    if (init_data == NULL) {
        printf("failed to obtain PHY init data\n");
        abort();
    }
    esp_phy_calibration_data_t* cal_data =
            (esp_phy_calibration_data_t*) calloc(sizeof(esp_phy_calibration_data_t), 1);
    if (cal_data == NULL) {
        printf("failed to allocate memory for RF calibration data\n");
        abort();
    }
    esp_err_t err = esp_phy_load_cal_data_from_nvs(cal_data);
    if (err != ESP_OK) {
        printf("failed to load RF calibration data, falling back to full calibration\n");
        calibration_mode = PHY_RF_CAL_FULL;
    }

    esp_phy_init(init_data, calibration_mode, cal_data);

    if (calibration_mode != PHY_RF_CAL_NONE) {
        err = esp_phy_store_cal_data_to_nvs(cal_data);
    } else {
        err = ESP_OK;
    }
    esp_phy_release_init_data(init_data);
    free(cal_data); // PHY maintains a copy of calibration data, so we can free this
    initialized = true;
}
#endif

void yield()
{
    vPortYield();
}

uint32_t IRAM_ATTR micros()
{
    uint32_t ccount;
    __asm__ __volatile__ ( "rsr     %0, ccount" : "=a" (ccount) );
    return ccount / CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ;
}

uint32_t IRAM_ATTR millis()
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

void delay(uint32_t ms)
{
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

void IRAM_ATTR delayMicroseconds(uint32_t us)
{
    uint32_t m = micros();
    if(us){
        uint32_t e = (m + us) % ((0xFFFFFFFF / CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ) + 1);
        if(m > e){ //overflow
            while(micros() > e){
                NOP();
            }
        }
        while(micros() < e){
            NOP();
        }
    }
}

void initVariant() __attribute__((weak));
void initVariant() {}

void init() __attribute__((weak));
void init() {}

void initWiFi() __attribute__((weak));
void initWiFi() {}

void initBT() __attribute__((weak));
void initBT() {}

void initArduino(){
    nvs_flash_init();
    init();
    initVariant();
    initWiFi();
    initBT();
}

//used by hal log
const char * IRAM_ATTR pathToFileName(const char * path){
    size_t i = 0;
    size_t pos = 0;
    char * p = (char *)path;
    while(*p){
        i++;
        if(*p == '/' || *p == '\\'){
            pos = i;
        }
        p++;
    }
    return path+pos;
}

