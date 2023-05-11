/*
 Arduino.h - Main include file for the Arduino SDK
 Copyright (c) 2005-2013 Arduino Team.  All right reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef HAL_ESP32_HAL_H_
#define HAL_ESP32_HAL_H_

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
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef F_CPU
#if CONFIG_IDF_TARGET_ESP32 // ESP32/PICO-D4
#define F_CPU (CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ * 1000000U)
#elif CONFIG_IDF_TARGET_ESP32C3
#define F_CPU (CONFIG_ESP32C3_DEFAULT_CPU_FREQ_MHZ * 1000000U)
#elif CONFIG_IDF_TARGET_ESP32S2
#define F_CPU (CONFIG_ESP32S2_DEFAULT_CPU_FREQ_MHZ * 1000000U)
#elif CONFIG_IDF_TARGET_ESP32S3
#define F_CPU (CONFIG_ESP32S3_DEFAULT_CPU_FREQ_MHZ * 1000000U)
#endif
#endif

#if CONFIG_ARDUINO_ISR_IRAM
#define ARDUINO_ISR_ATTR IRAM_ATTR
#define ARDUINO_ISR_FLAG ESP_INTR_FLAG_IRAM
#else
#define ARDUINO_ISR_ATTR
#define ARDUINO_ISR_FLAG (0)
#endif

#ifndef ARDUINO_RUNNING_CORE
#define ARDUINO_RUNNING_CORE CONFIG_ARDUINO_RUNNING_CORE
#endif

#ifndef ARDUINO_EVENT_RUNNING_CORE
#define ARDUINO_EVENT_RUNNING_CORE CONFIG_ARDUINO_EVENT_RUNNING_CORE
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
#include "esp32-hal-rgb-led.h"
#include "esp32-hal-cpu.h"

void analogWrite(uint8_t pin, int value);
int8_t analogGetChannel(uint8_t pin);
void analogWriteFrequency(uint32_t freq);
void analogWriteResolution(uint8_t bits);

//returns chip temperature in Celsius
float temperatureRead();

//allows user to bypass SPI RAM test routine
bool testSPIRAM(void);

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
