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
// modified Nov 2017 by Chuck Todd <StickBreaker> to support Interrupt Driven I/O

#ifndef _ESP32_HAL_I2C_H_
#define _ESP32_HAL_I2C_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

// External Wire.h equivalent error Codes
typedef enum {
    I2C_ERROR_OK=0,
    I2C_ERROR_MEMORY,
    I2C_ERROR_ADDR_NACK,
    I2C_ERROR_DATA_NACK,
    I2C_ERROR_I2C_OTHER,
    I2C_ERROR_TIMEOUT,
 
    // extra error codes
    I2C_ERROR_NO_BEGIN,
} i2c_err_t;

struct i2c_struct_t;
typedef struct i2c_struct_t i2c_t;

bool i2c_is_driver_installed(i2c_t *i2c);
i2c_t* i2cMasterInit(int8_t i2c_num, int8_t sda, int8_t scl, uint32_t clk_speed);
void i2cRelease(i2c_t *i2c);
void i2cFlush(i2c_t *i2c);
void i2cSetTimeOut(i2c_t *i2c, uint16_t timeOutMillis);
int i2cWriteByte(i2c_t *i2c, uint8_t data);
int i2cWriteBuffer(i2c_t *i2c, uint8_t *write_buffer, size_t write_size);
int i2cRead(i2c_t *i2c, uint8_t* read_buffer, size_t read_size);
int i2cWriteSlaveAddr(i2c_t *i2c, uint16_t address, bool readOp);
int i2cStartTransaction(i2c_t *i2c, bool init_cmd_link);
int i2cCloseTransaction(i2c_t *i2c);
int i2cTranslateError(int err);

#ifdef __cplusplus
}
#endif

#endif /* _ESP32_HAL_I2C_H_ */
