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

#ifndef HAL_ESP32_HAL_H_
#define HAL_ESP32_HAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include "sdkconfig.h"
#include "esp_system.h"

#ifndef F_CPU
#define F_CPU (CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ * 1000000U)
#endif

//forward declaration from freertos/portmacro.h
void vPortYield(void);
void yield(void);
#define optimistic_yield(u)

#define ESP_REG(addr) *((volatile uint32_t *)(addr))
#define NOP() asm volatile ("nop")

#include "esp32-hal-log.h"
#include "esp32-hal-matrix.h"
#include "esp32-hal-uart.h"
#include "esp32-hal-gpio.h"
#include "esp32-hal-touch.h"
#include "esp32-hal-dac.h"
#include "esp32-hal-adc.h"
#include "esp32-hal-spi.h"
#include "esp32-hal-i2c.h"
#include "esp32-hal-ledc.h"
#include "esp32-hal-rmt.h"
#include "esp32-hal-sigmadelta.h"
#include "esp32-hal-timer.h"
#include "esp32-hal-bt.h"
#include "esp32-hal-psram.h"
#include "esp32-hal-cpu.h"

#ifndef BOARD_HAS_PSRAM
#ifdef CONFIG_SPIRAM_SUPPORT
#undef CONFIG_SPIRAM_SUPPORT
#endif
#endif

//returns chip temperature in Celsius
float temperatureRead();

#if CONFIG_AUTOSTART_ARDUINO
//enable/disable WDT for Arduino's setup and loop functions
void enableLoopWDT();
void disableLoopWDT();
//feed WDT for the loop task
void feedLoopWDT();
#endif

//enable/disable WDT for the IDLE task on Core 0 (SYSTEM)
void enableCore0WDT();
void disableCore0WDT();
#ifndef CONFIG_FREERTOS_UNICORE
//enable/disable WDT for the IDLE task on Core 1 (Arduino)
void enableCore1WDT();
void disableCore1WDT();
#endif

//if xCoreID < 0 or CPU is unicore, it will use xTaskCreate, else xTaskCreatePinnedToCore
//allows to easily handle all possible situations without repetitive code
BaseType_t xTaskCreateUniversal( TaskFunction_t pxTaskCode,
                        const char * const pcName,
                        const uint32_t usStackDepth,
                        void * const pvParameters,
                        UBaseType_t uxPriority,
                        TaskHandle_t * const pxCreatedTask,
                        const BaseType_t xCoreID );

unsigned long micros();
unsigned long millis();
void delay(uint32_t);
void delayMicroseconds(uint32_t us);

#if !CONFIG_ESP32_PHY_AUTO_INIT
void arduino_phy_init();
#endif

#if !CONFIG_AUTOSTART_ARDUINO
void initArduino();
#endif

#ifdef __cplusplus
}
#endif

#endif /* HAL_ESP32_HAL_H_ */
