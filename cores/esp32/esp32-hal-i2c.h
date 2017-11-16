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

#include <stdint.h>
#include <stdbool.h>
#include <esp_attr.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"


typedef enum {
    I2C_ERROR_OK=0,
    I2C_ERROR_DEV,
    I2C_ERROR_ACK,
    I2C_ERROR_TIMEOUT,
    I2C_ERROR_BUS,
    I2C_ERROR_BUSY,
    I2C_ERROR_MEMORY,
    I2C_ERROR_CONTINUE,
    I2C_ERROR_MISSING_WRITE,
    I2C_ERROR_NO_BEGIN
} i2c_err_t;

typedef enum {
  //I2C_NONE=0,
  I2C_STARTUP=1,
  I2C_RUNNING,
  I2C_DONE
} I2C_STAGE_t;

typedef enum {
	I2C_NONE=0,
	I2C_MASTER,
	I2C_SLAVE,
	I2C_MASTERSLAVE
}I2C_MODE_t;


typedef enum {
//  I2C_NONE=0,	
	I2C_OK=1,
  I2C_ERROR,
	I2C_ADDR_NAK,
	I2C_DATA_NAK,
	I2C_ARBITRATION,
	I2C_TIMEOUT
}I2C_ERROR_t;

// i2c_event bits
#define EVENT_ERROR_NAK (BIT(0))
#define EVENT_ERROR     (BIT(1))
#define EVENT_RUNNING   (BIT(3))
#define EVENT_DONE      (BIT(4))
#define EVENT_IN_END    (BIT(5))
#define EVENT_ERROR_PREV  (BIT(6))
#define EVENT_ERROR_TIMEOUT   (BIT(7))
#define EVENT_ERROR_ARBITRATION (BIT(8))
#define EVENT_ERROR_DATA_NAK  (BIT(9))
	
typedef union{
  struct {
    uint32_t  addr: 16; // I2C address, if 10bit must have 0x7800 mask applied, else 8bit
    uint32_t  mode:         1; // 0 write, 1 read
    uint32_t  stop:         1; // 0 no, 1 yes 
    uint32_t  startCmdSent: 1; // START cmd has been added to command[]
    uint32_t  addrCmdSent:  1; // addr WRITE cmd has been added to command[]
    uint32_t  dataCmdSent:  1; // all necessary DATA(READ/WRITE) cmds added to command[]
    uint32_t  stopCmdSent:  1; // completed all necessary commands
    uint32_t  addrReq:      2; // number of addr bytes need to send address
    uint32_t  addrSent:     2; // number of addr bytes added to FIFO
    uint32_t  reserved_31:  6;
    };
  uint32_t val;
  }I2C_DATA_CTRL_t;
  
typedef struct {
  uint8_t *data;           // datapointer for read/write buffer
  uint16_t length;         // size of data buffer
  uint16_t position;       // current position for next char in buffer (<length)
  uint16_t cmdBytesNeeded; // number of data bytes needing (READ/WRITE)Command[]
  uint16_t queueLength;
  I2C_DATA_CTRL_t ctrl;
  EventGroupHandle_t queueEvent;
  }I2C_DATA_QUEUE_t;  

struct i2c_struct_t;
typedef struct i2c_struct_t i2c_t;

i2c_t * i2cInit(uint8_t i2c_num, uint16_t slave_addr, bool addr_10bit_en);

//call this after you setup the bus and pins to send empty packet
//required because when pins are attached, they emit pulses that lock the bus
void i2cInitFix(i2c_t * i2c);

i2c_err_t i2cSetFrequency(i2c_t * i2c, uint32_t clk_speed);
uint32_t i2cGetFrequency(i2c_t * i2c);

i2c_err_t i2cAttachSCL(i2c_t * i2c, int8_t scl);
i2c_err_t i2cDetachSCL(i2c_t * i2c, int8_t scl);
i2c_err_t i2cAttachSDA(i2c_t * i2c, int8_t sda);
i2c_err_t i2cDetachSDA(i2c_t * i2c, int8_t sda);

i2c_err_t i2cWrite(i2c_t * i2c, uint16_t address, bool addr_10bit, uint8_t * data, uint8_t len, bool sendStop);
i2c_err_t i2cRead(i2c_t * i2c, uint16_t address, bool addr_10bit, uint8_t * data, uint8_t len, bool sendStop);
//Stickbreakers attempt to read big blocks
i2c_err_t pollI2cRead(i2c_t * i2c, uint16_t address, bool addr_10bit, uint8_t * data, uint16_t len, bool sendStop);
i2c_err_t i2cProcQueue(i2c_t *i2c);
i2c_err_t i2cAddQueueWrite(i2c_t *i2c, uint16_t i2cDeviceAddr, uint8_t *dataPtr, uint16_t dataLen, bool SendStop, EventGroupHandle_t event);
i2c_err_t i2cAddQueueRead(i2c_t *i2c, uint16_t i2cDeviceAddr, uint8_t *dataPtr, uint16_t dataLen, bool SendStop, EventGroupHandle_t event);
i2c_err_t i2cFreeQueue(i2c_t *i2c);
i2c_err_t i2cReleaseISR(i2c_t *i2c);
uint16_t i2cQueueReadPendingCount(i2c_t *i2c);
uint16_t i2cQueueReadCount(i2c_t *i2c);
i2c_err_t i2cGetReadQueue(i2c_t *i2c, uint8_t** buffPtr, uint16_t* lenPtr,uint8_t *savePtr);
void i2cDumpInts();


static void IRAM_ATTR i2c_isr_handler_default(void* arg); //ISR


void i2cReset(i2c_t* i2c);

#ifdef __cplusplus
}
#endif

#endif /* _ESP32_HAL_I2C_H_ */
