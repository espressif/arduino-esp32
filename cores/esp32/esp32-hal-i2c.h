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
#include <esp_attr.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

// start from tools/sdk/include/soc/soc/i2c_struct.h

typedef union {
        struct {
            uint32_t byte_num:      8;              /*Byte_num represent the number of data need to be send or data need to be received.*/
            uint32_t ack_en:        1;              /*ack_check_en  ack_exp and ack value are used to control  the ack bit.*/
            uint32_t ack_exp:       1;              /*ack_check_en  ack_exp and ack value are used to control  the ack bit.*/
            uint32_t ack_val:       1;              /*ack_check_en  ack_exp and ack value are used to control  the ack bit.*/
            uint32_t op_code:       3;              /*op_code is the command  0：RSTART   1：WRITE  2：READ  3：STOP . 4:END.*/
            uint32_t reserved14:   17;
            uint32_t done:  1;                      /*When command0 is done in I2C Master mode  this bit changes to high level.*/
        };
        uint32_t val;
    } I2C_COMMAND_t;
    
typedef union {
        struct {
            uint32_t rx_fifo_full_thrhd: 5;
            uint32_t tx_fifo_empty_thrhd:5;         //Config tx_fifo empty threhd value when using apb fifo access * /
            uint32_t nonfifo_en:         1;         //Set this bit to enble apb nonfifo access. * /
            uint32_t fifo_addr_cfg_en:   1;         //When this bit is set to 1 then the byte after address represent the offset address of I2C Slave's ram. * /
            uint32_t rx_fifo_rst:        1;         //Set this bit to reset rx fifo when using apb fifo access. * /
						// chuck while this bit is 1, the RX fifo is held in REST, Toggle it * /
            uint32_t tx_fifo_rst:        1;         //Set this bit to reset tx fifo when using apb fifo access. * /
						// chuck while this bit is 1, the TX fifo is held in REST, Toggle it * /
            uint32_t nonfifo_rx_thres:   6;         //when I2C receives more than nonfifo_rx_thres data  it will produce rx_send_full_int_raw interrupt and update the current offset address of the receiving data.* /
            uint32_t nonfifo_tx_thres:   6;         //when I2C sends more than nonfifo_tx_thres data  it will produce tx_send_empty_int_raw interrupt and update the current offset address of the sending data. * /
            uint32_t reserved26:         6;
        };
        uint32_t val;
    } I2C_FIFO_CONF_t; 
	
// end from tools/sdk/include/soc/soc/i2c_struct.h
 
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

// sync between dispatch(i2cProcQueue) and worker(i2c_isr_handler_default) 
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

// internal Error condition
typedef enum {
//  I2C_NONE=0,	
	I2C_OK=1,
  I2C_ERROR,
	I2C_ADDR_NAK,
	I2C_DATA_NAK,
	I2C_ARBITRATION,
	I2C_TIMEOUT
}I2C_ERROR_t;

// i2c_event bits for EVENTGROUP bits
// needed to minimize change events, FreeRTOS Daemon overload, so ISR will only set values
// on Exit.  Dispatcher will set bits for each dq before/after ISR completion
#define EVENT_ERROR_NAK (BIT(0))
#define EVENT_ERROR     (BIT(1))
#define EVENT_ERROR_BUS_BUSY  (BIT(2))
#define EVENT_RUNNING   (BIT(3)) 
#define EVENT_DONE      (BIT(4))
#define EVENT_IN_END    (BIT(5))
#define EVENT_ERROR_PREV  (BIT(6))
#define EVENT_ERROR_TIMEOUT   (BIT(7))
#define EVENT_ERROR_ARBITRATION (BIT(8))
#define EVENT_ERROR_DATA_NAK  (BIT(9))
#define EVENT_MASK 0x3F	

// control record for each dq entry
typedef union{
  struct {
    uint32_t  addr: 16; // I2C address, if 10bit must have 0x7800 mask applied, else 8bit
    uint32_t  mode:         1; // transaction direction 0 write, 1 read
    uint32_t  stop:         1; // sendStop 0 no, 1 yes 
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

// individual dq element  
typedef struct {
  uint8_t *data;           // datapointer for read/write buffer
  uint16_t length;         // size of data buffer
  uint16_t position;       // current position for next char in buffer (<length)
  uint16_t cmdBytesNeeded; // used to control number of I2C_COMMAND_t blocks added to queu 
  uint16_t queueLength;    // number of data bytes needing moved, used to control 
                           // current queuePos for fifo fills  
  I2C_DATA_CTRL_t ctrl;
  EventGroupHandle_t queueEvent;  // optional user supplied for Async feedback EventBits 
  }I2C_DATA_QUEUE_t;  

struct i2c_struct_t;
typedef struct i2c_struct_t i2c_t;

i2c_t * i2cInit(uint8_t i2c_num);

//call this after you setup the bus and pins to send empty packet
//required because when pins are attached, they emit pulses that lock the bus
void i2cInitFix(i2c_t * i2c);

void i2cReset(i2c_t* i2c);

i2c_err_t i2cSetFrequency(i2c_t * i2c, uint32_t clk_speed);
uint32_t i2cGetFrequency(i2c_t * i2c);

i2c_err_t i2cAttachSCL(i2c_t * i2c, int8_t scl);
i2c_err_t i2cDetachSCL(i2c_t * i2c, int8_t scl);
i2c_err_t i2cAttachSDA(i2c_t * i2c, int8_t sda);
i2c_err_t i2cDetachSDA(i2c_t * i2c, int8_t sda);

//Stickbreakers ISR Support
i2c_err_t i2cProcQueue(i2c_t *i2c, uint32_t *readCount, uint16_t timeOutMillis);
i2c_err_t i2cAddQueueWrite(i2c_t *i2c, uint16_t i2cDeviceAddr, uint8_t *dataPtr, uint16_t dataLen, bool SendStop, EventGroupHandle_t event);
i2c_err_t i2cAddQueueRead(i2c_t *i2c, uint16_t i2cDeviceAddr, uint8_t *dataPtr, uint16_t dataLen, bool SendStop, EventGroupHandle_t event);
i2c_err_t i2cFreeQueue(i2c_t *i2c);
i2c_err_t i2cReleaseISR(i2c_t *i2c);
//stickbreaker debug support
void i2cDumpInts(uint8_t num);
void i2cDumpI2c(i2c_t *i2c);

#ifdef __cplusplus
}
#endif

#endif /* _ESP32_HAL_I2C_H_ */
