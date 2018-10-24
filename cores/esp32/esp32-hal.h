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

#ifndef BOARD_HAS_PSRAM
#ifdef CONFIG_SPIRAM_SUPPORT
#undef CONFIG_SPIRAM_SUPPORT
#endif
#endif

//returns chip temperature in Celsius
float temperatureRead();

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
