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

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_attr.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_partition.h"
#include "esp_log.h"
#include "esp_timer.h"
#ifdef CONFIG_APP_ROLLBACK_ENABLE
#include "esp_ota_ops.h"
#endif //CONFIG_APP_ROLLBACK_ENABLE
#ifdef CONFIG_BT_ENABLED
#include "esp_bt.h"
#endif //CONFIG_BT_ENABLED
#include <sys/time.h>
#include "soc/rtc.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/apb_ctrl_reg.h"
#include "rom/rtc.h"
#include "esp_task_wdt.h"
#include "esp32-hal.h"

//Undocumented!!! Get chip temperature in Farenheit
//Source: https://github.com/pcbreflux/espressif/blob/master/esp32/arduino/sketchbook/ESP32_int_temp_sensor/ESP32_int_temp_sensor.ino
uint8_t temprature_sens_read();

float temperatureRead()
{
    return (temprature_sens_read() - 32) / 1.8;
}

void __yield()
{
    vPortYield();
}

void yield() __attribute__ ((weak, alias("__yield")));

#if CONFIG_AUTOSTART_ARDUINO

extern TaskHandle_t loopTaskHandle;
extern bool loopTaskWDTEnabled;

void enableLoopWDT(){
    if(loopTaskHandle != NULL){
        if(esp_task_wdt_add(loopTaskHandle) != ESP_OK){
            log_e("Failed to add loop task to WDT");
        } else {
            loopTaskWDTEnabled = true;
        }
    }
}

void disableLoopWDT(){
    if(loopTaskHandle != NULL && loopTaskWDTEnabled){
        loopTaskWDTEnabled = false;
        if(esp_task_wdt_delete(loopTaskHandle) != ESP_OK){
            log_e("Failed to remove loop task from WDT");
        }
    }
}

void feedLoopWDT(){
    esp_err_t err = esp_task_wdt_reset();
    if(err != ESP_OK){
        log_e("Failed to feed WDT! Error: %d", err);
    }
}
#endif

void enableCore0WDT(){
    TaskHandle_t idle_0 = xTaskGetIdleTaskHandleForCPU(0);
    if(idle_0 == NULL || esp_task_wdt_add(idle_0) != ESP_OK){
        log_e("Failed to add Core 0 IDLE task to WDT");
    }
}

void disableCore0WDT(){
    TaskHandle_t idle_0 = xTaskGetIdleTaskHandleForCPU(0);
    if(idle_0 == NULL || esp_task_wdt_delete(idle_0) != ESP_OK){
        log_e("Failed to remove Core 0 IDLE task from WDT");
    }
}

#ifndef CONFIG_FREERTOS_UNICORE
void enableCore1WDT(){
    TaskHandle_t idle_1 = xTaskGetIdleTaskHandleForCPU(1);
    if(idle_1 == NULL || esp_task_wdt_add(idle_1) != ESP_OK){
        log_e("Failed to add Core 1 IDLE task to WDT");
    }
}

void disableCore1WDT(){
    TaskHandle_t idle_1 = xTaskGetIdleTaskHandleForCPU(1);
    if(idle_1 == NULL || esp_task_wdt_delete(idle_1) != ESP_OK){
        log_e("Failed to remove Core 1 IDLE task from WDT");
    }
}
#endif

BaseType_t xTaskCreateUniversal( TaskFunction_t pxTaskCode,
                        const char * const pcName,
                        const uint32_t usStackDepth,
                        void * const pvParameters,
                        UBaseType_t uxPriority,
                        TaskHandle_t * const pxCreatedTask,
                        const BaseType_t xCoreID ){
#ifndef CONFIG_FREERTOS_UNICORE
    if(xCoreID >= 0 && xCoreID < 2) {
        return xTaskCreatePinnedToCore(pxTaskCode, pcName, usStackDepth, pvParameters, uxPriority, pxCreatedTask, xCoreID);
    } else {
#endif
    return xTaskCreate(pxTaskCode, pcName, usStackDepth, pvParameters, uxPriority, pxCreatedTask);
#ifndef CONFIG_FREERTOS_UNICORE
    }
#endif
}

unsigned long IRAM_ATTR micros()
{
    return (unsigned long) (esp_timer_get_time());
}

unsigned long IRAM_ATTR millis()
{
    return (unsigned long) (esp_timer_get_time() / 1000ULL);
}

void delay(uint32_t ms)
{
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

void IRAM_ATTR delayMicroseconds(uint32_t us)
{
    uint32_t m = micros();
    if(us){
        uint32_t e = (m + us);
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

bool verifyOta() __attribute__((weak));
bool verifyOta() { return true; }

#ifdef CONFIG_BT_ENABLED
//overwritten in esp32-hal-bt.c
bool btInUse() __attribute__((weak));
bool btInUse(){ return false; }
#endif

void initArduino()
{
#ifdef CONFIG_APP_ROLLBACK_ENABLE
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            if (verifyOta()) {
                esp_ota_mark_app_valid_cancel_rollback();
            } else {
                log_e("OTA verification failed! Start rollback to the previous version ...");
                esp_ota_mark_app_invalid_rollback_and_reboot();
            }
        }
    }
#endif
    //init proper ref tick value for PLL (uncomment if REF_TICK is different than 1MHz)
    //ESP_REG(APB_CTRL_PLL_TICK_CONF_REG) = APB_CLK_FREQ / REF_CLK_FREQ - 1;
#ifdef F_CPU
    setCpuFrequencyMhz(F_CPU/1000000);
#endif
#if CONFIG_SPIRAM_SUPPORT
    psramInit();
#endif
    esp_log_level_set("*", CONFIG_LOG_DEFAULT_LEVEL);
    esp_err_t err = nvs_flash_init();
    if(err == ESP_ERR_NVS_NO_FREE_PAGES){
        const esp_partition_t* partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, NULL);
        if (partition != NULL) {
            err = esp_partition_erase_range(partition, 0, partition->size);
            if(!err){
                err = nvs_flash_init();
            } else {
                log_e("Failed to format the broken NVS partition!");
            }
        }
    }
    if(err) {
        log_e("Failed to initialize NVS! Error: %u", err);
    }
#ifdef CONFIG_BT_ENABLED
    if(!btInUse()){
        esp_bt_controller_mem_release(ESP_BT_MODE_BTDM);
    }
#endif
    init();
    initVariant();
}

//used by hal log
const char * IRAM_ATTR pathToFileName(const char * path)
{
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

