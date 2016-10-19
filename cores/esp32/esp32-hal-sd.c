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
#include "rom/ets_sys.h"
#include "esp32-hal-matrix.h"
#include "soc/gpio_sd_reg.h"
#include "soc/gpio_sd_struct.h"

xSemaphoreHandle _sd_sys_lock;

#define SD_MUTEX_LOCK()    do {} while (xSemaphoreTake(_sd_sys_lock, portMAX_DELAY) != pdPASS)
#define SD_MUTEX_UNLOCK()  xSemaphoreGive(_sd_sys_lock)


uint32_t sdSetup(uint8_t channel, uint32_t freq) //chan 0-7 freq 1220-312500
{
    if(channel > 7) {
        return 0;
    }
    static bool tHasStarted = false;
    if(!tHasStarted) {
        tHasStarted = true;
        _sd_sys_lock = xSemaphoreCreateMutex();
    }
    gpio_sd_dev_t * gpio_sd_dev = (volatile gpio_sd_dev_t *)(DR_REG_GPIO_SD_BASE);
    uint32_t prescale = (10000000/(freq*32)) - 1;
    if(prescale > 0xFF) {
        prescale = 0xFF;
    }
    SD_MUTEX_LOCK();
    gpio_sd_dev->channel[channel].prescale = prescale;
    gpio_sd_dev->cg.clk_en = 0;
    gpio_sd_dev->cg.clk_en = 1;
    SD_MUTEX_UNLOCK();
    return 10000000/((prescale + 1) * 32);
}

void sdWrite(uint8_t channel, uint8_t duty) //chan 0-7 duty 8 bit
{
    if(channel > 7) {
        return;
    }
    duty += 128;
    gpio_sd_dev_t * gpio_sd_dev = (volatile gpio_sd_dev_t *)(DR_REG_GPIO_SD_BASE);
    SD_MUTEX_LOCK();
    gpio_sd_dev->channel[channel].duty = duty;
    SD_MUTEX_UNLOCK();
}

uint8_t sdRead(uint8_t channel) //chan 0-7
{
    if(channel > 7) {
        return 0;
    }
    gpio_sd_dev_t * gpio_sd_dev = (volatile gpio_sd_dev_t *)(DR_REG_GPIO_SD_BASE);
    return gpio_sd_dev->channel[channel].duty - 128;
}

void sdAttachPin(uint8_t pin, uint8_t channel) //channel 0-7
{
    if(channel > 7) {
        return;
    }
    pinMode(pin, OUTPUT);
    pinMatrixOutAttach(pin, GPIO_SD0_OUT_IDX + channel, false, false);
}

void sdDetachPin(uint8_t pin)
{
    pinMatrixOutDetach(pin, false, false);
}
