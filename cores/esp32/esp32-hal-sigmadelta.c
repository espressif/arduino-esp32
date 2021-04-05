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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp32-hal-matrix.h"
#include "soc/gpio_sd_reg.h"
#include "soc/gpio_sd_struct.h"

#include "esp_system.h"
#ifdef ESP_IDF_VERSION_MAJOR // IDF 4+
#if CONFIG_IDF_TARGET_ESP32 // ESP32/PICO-D4
#include "esp32/rom/ets_sys.h"
#elif CONFIG_IDF_TARGET_ESP32S2
#include "esp32s2/rom/ets_sys.h"
#else 
#error Target CONFIG_IDF_TARGET is not supported
#endif
#else // ESP32 Before IDF 4.0
#include "rom/ets_sys.h"
#endif


#if CONFIG_DISABLE_HAL_LOCKS
#define SD_MUTEX_LOCK()
#define SD_MUTEX_UNLOCK()
#else
#define SD_MUTEX_LOCK()    do {} while (xSemaphoreTake(_sd_sys_lock, portMAX_DELAY) != pdPASS)
#define SD_MUTEX_UNLOCK()  xSemaphoreGive(_sd_sys_lock)
xSemaphoreHandle _sd_sys_lock;
#endif

static void _on_apb_change(void * arg, apb_change_ev_t ev_type, uint32_t old_apb, uint32_t new_apb){
    if(old_apb == new_apb){
        return;
    }
    uint32_t iarg = (uint32_t)arg;
    uint8_t channel = iarg;
    if(ev_type == APB_BEFORE_CHANGE){
        SIGMADELTA.cg.clk_en = 0;
    } else {
        old_apb /= 1000000;
        new_apb /= 1000000;
        SD_MUTEX_LOCK();
        uint32_t old_prescale = SIGMADELTA.channel[channel].prescale + 1;
        SIGMADELTA.channel[channel].prescale = ((new_apb * old_prescale) / old_apb) - 1;
        SIGMADELTA.cg.clk_en = 0;
        SIGMADELTA.cg.clk_en = 1;
        SD_MUTEX_UNLOCK();
    }
}

uint32_t sigmaDeltaSetup(uint8_t channel, uint32_t freq) //chan 0-7 freq 1220-312500
{
    if(channel > 7) {
        return 0;
    }
#if !CONFIG_DISABLE_HAL_LOCKS
    static bool tHasStarted = false;
    if(!tHasStarted) {
        tHasStarted = true;
        _sd_sys_lock = xSemaphoreCreateMutex();
    }
#endif
    uint32_t apb_freq = getApbFrequency();
    uint32_t prescale = (apb_freq/(freq*256)) - 1;
    if(prescale > 0xFF) {
        prescale = 0xFF;
    }
    SD_MUTEX_LOCK();
#ifndef CONFIG_IDF_TARGET_ESP32
    SIGMADELTA.misc.function_clk_en = 1;
#endif
    SIGMADELTA.channel[channel].prescale = prescale;
    SIGMADELTA.cg.clk_en = 0;
    SIGMADELTA.cg.clk_en = 1;
    SD_MUTEX_UNLOCK();
    uint32_t iarg = channel;
    addApbChangeCallback((void*)iarg, _on_apb_change);
    return apb_freq/((prescale + 1) * 256);
}

void sigmaDeltaWrite(uint8_t channel, uint8_t duty) //chan 0-7 duty 8 bit
{
    if(channel > 7) {
        return;
    }
    duty -= 128;
    SD_MUTEX_LOCK();
    SIGMADELTA.channel[channel].duty = duty;
    SD_MUTEX_UNLOCK();
}

uint8_t sigmaDeltaRead(uint8_t channel) //chan 0-7
{
    if(channel > 7) {
        return 0;
    }
    SD_MUTEX_LOCK();
    uint8_t duty = SIGMADELTA.channel[channel].duty + 128;
    SD_MUTEX_UNLOCK();
    return duty;
}

void sigmaDeltaAttachPin(uint8_t pin, uint8_t channel) //channel 0-7
{
    if(channel > 7) {
        return;
    }
    pinMode(pin, OUTPUT);
    pinMatrixOutAttach(pin, GPIO_SD0_OUT_IDX + channel, false, false);
}

void sigmaDeltaDetachPin(uint8_t pin)
{
    pinMatrixOutDetach(pin, false, false);
}
