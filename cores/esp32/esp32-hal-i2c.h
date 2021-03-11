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
    I2C_ERROR_DEV,
    I2C_ERROR_ACK,
    I2C_ERROR_TIMEOUT,
    I2C_ERROR_BUS,
    I2C_ERROR_BUSY,
    I2C_ERROR_MEMORY,
    I2C_ERROR_CONTINUE,
    I2C_ERROR_NO_BEGIN
} i2c_err_t;

struct i2c_struct_t;
typedef struct i2c_struct_t i2c_t;

i2c_t * i2cInit(uint8_t i2c_num, int8_t sda, int8_t scl, uint32_t clk_speed);
void i2cRelease(i2c_t *i2c); // free ISR, Free DQ, Power off peripheral clock.  Must call i2cInit() to recover
i2c_err_t i2cWrite(i2c_t * i2c, uint16_t address, uint8_t* buff, uint16_t size, bool sendStop, uint16_t timeOutMillis);
i2c_err_t i2cRead(i2c_t * i2c, uint16_t address, uint8_t* buff, uint16_t size, bool sendStop, uint16_t timeOutMillis, uint32_t *readCount);
i2c_err_t i2cFlush(i2c_t *i2c);
i2c_err_t i2cSetFrequency(i2c_t * i2c, uint32_t clk_speed);
uint32_t i2cGetFrequency(i2c_t * i2c);
uint32_t i2cGetStatus(i2c_t * i2c); // Status register of peripheral

//Functions below should be used only if well understood
//Might be deprecated and removed in future
i2c_err_t i2cAttachSCL(i2c_t * i2c, int8_t scl);
i2c_err_t i2cDetachSCL(i2c_t * i2c, int8_t scl);
i2c_err_t i2cAttachSDA(i2c_t * i2c, int8_t sda);
i2c_err_t i2cDetachSDA(i2c_t * i2c, int8_t sda);

//Stickbreakers ISR Support
i2c_err_t i2cProcQueue(i2c_t *i2c, uint32_t *readCount, uint16_t timeOutMillis);
i2c_err_t i2cAddQueueWrite(i2c_t *i2c, uint16_t i2cDeviceAddr, uint8_t *dataPtr, uint16_t dataLen, bool SendStop, EventGroupHandle_t event);
i2c_err_t i2cAddQueueRead(i2c_t *i2c, uint16_t i2cDeviceAddr, uint8_t *dataPtr, uint16_t dataLen, bool SendStop, EventGroupHandle_t event);

//stickbreaker debug support
uint32_t i2cDebug(i2c_t *, uint32_t setBits, uint32_t resetBits);
//  Debug actions have 3 currently defined locus 
// 0xXX------ : at entry of ProcQueue 
// 0x--XX---- : at exit of ProcQueue
// 0x------XX : at entry of Flush
// 
// bit 0 causes DumpI2c to execute 
// bit 1 causes DumpInts to execute
// bit 2 causes DumpCmdqueue to execute
// bit 3 causes DumpStatus to execute
// bit 4 causes DumpFifo to execute

#ifdef __cplusplus
}
#endif

#endif /* _ESP32_HAL_I2C_H_ */
