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

#ifndef _ESP32_HAL_I2C_H_
#define _ESP32_HAL_I2C_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "esp32-hal.h"
#include "soc/i2c_struct.h"

typedef struct {
    i2c_dev_t * dev;
    uint8_t num;
} i2c_t;

i2c_t * i2cInit(uint8_t i2c_num, uint16_t slave_addr, bool addr_10bit_en);

void i2cSetFrequency(i2c_t * i2c, uint32_t clk_speed);
uint32_t i2cGetFrequency(i2c_t * i2c);

void i2cResetFiFo(i2c_t * i2c);

void i2cAttachSCL(i2c_t * i2c, int8_t scl);
void i2cDetachSCL(i2c_t * i2c, int8_t scl);
void i2cAttachSDA(i2c_t * i2c, int8_t sda);
void i2cDetachSDA(i2c_t * i2c, int8_t sda);

int i2cWrite(i2c_t * i2c, uint16_t address, bool addr_10bit, uint8_t * data, uint8_t len, bool sendStop);
int i2cRead(i2c_t * i2c, uint16_t address, bool addr_10bit, uint8_t * data, uint8_t len, bool sendStop);


#ifdef __cplusplus
}
#endif

#endif /* _ESP32_HAL_I2C_H_ */
