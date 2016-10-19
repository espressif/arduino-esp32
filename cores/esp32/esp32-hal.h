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
#include <inttypes.h>
#include <string.h>
#include <math.h>

#define ESP_REG(addr) *((volatile uint32_t *)(addr))
#define NOP() asm volatile ("nop")

#include "esp32-hal-log.h"
#include "esp32-hal-matrix.h"
#include "esp32-hal-uart.h"
#include "esp32-hal-gpio.h"
#include "esp32-hal-spi.h"
#include "esp32-hal-i2c.h"
#include "esp32-hal-ledc.h"
#include "esp32-hal-sd.h"
#include "esp_system.h"

uint32_t micros();
uint32_t millis();
void delay(uint32_t);
void delayMicroseconds(uint32_t us);

#ifdef __cplusplus
}
#endif

#endif /* HAL_ESP32_HAL_H_ */
