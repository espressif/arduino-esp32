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

#ifndef MAIN_ESP32_HAL_UART_H_
#define MAIN_ESP32_HAL_UART_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define SERIAL_5N1 0x8000010
#define SERIAL_6N1 0x8000014
#define SERIAL_7N1 0x8000018
#define SERIAL_8N1 0x800001c
#define SERIAL_5N2 0x8000030
#define SERIAL_6N2 0x8000034
#define SERIAL_7N2 0x8000038
#define SERIAL_8N2 0x800003c
#define SERIAL_5E1 0x8000012
#define SERIAL_6E1 0x8000016
#define SERIAL_7E1 0x800001a
#define SERIAL_8E1 0x800001e
#define SERIAL_5E2 0x8000032
#define SERIAL_6E2 0x8000036
#define SERIAL_7E2 0x800003a
#define SERIAL_8E2 0x800003e
#define SERIAL_5O1 0x8000013
#define SERIAL_6O1 0x8000017
#define SERIAL_7O1 0x800001b
#define SERIAL_8O1 0x800001f
#define SERIAL_5O2 0x8000033
#define SERIAL_6O2 0x8000037
#define SERIAL_7O2 0x800003b
#define SERIAL_8O2 0x800003f

// These are Hardware Flow Contol possible usage
// equivalent to UDF enum uart_hw_flowcontrol_t from 
// https://github.com/espressif/esp-idf/blob/master/components/hal/include/hal/uart_types.h#L75-L81
#define HW_FLOWCTRL_DISABLE   0x0       // disable HW Flow Control
#define HW_FLOWCTRL_RTS       0x1       // use only RTS PIN for HW Flow Control
#define HW_FLOWCTRL_CTS       0x2       // use only CTS PIN for HW Flow Control
#define HW_FLOWCTRL_CTS_RTS   0x3       // use both CTS and RTS PIN for HW Flow Control

struct uart_struct_t;
typedef struct uart_struct_t uart_t;

uart_t* uartBegin(uint8_t uart_nr, uint32_t baudrate, uint32_t config, int8_t rxPin, int8_t txPin, uint16_t rx_buffer_size, uint16_t tx_buffer_size, bool inverted, uint8_t rxfifo_full_thrhd);
void uartEnd(uart_t* uart);

// This is used to retrieve the Event Queue pointer from a UART IDF Driver in order to allow user to deal with its events
void uartGetEventQueue(uart_t* uart, QueueHandle_t *q); 

uint32_t uartAvailable(uart_t* uart);
uint32_t uartAvailableForWrite(uart_t* uart);
uint8_t uartRead(uart_t* uart);
uint8_t uartPeek(uart_t* uart);

void uartWrite(uart_t* uart, uint8_t c);
void uartWriteBuf(uart_t* uart, const uint8_t * data, size_t len);

void uartFlush(uart_t* uart);
void uartFlushTxOnly(uart_t* uart, bool txOnly );

void uartSetBaudRate(uart_t* uart, uint32_t baud_rate);
uint32_t uartGetBaudRate(uart_t* uart);

void uartSetRxInvert(uart_t* uart, bool invert);

void uartSetDebug(uart_t* uart);
int uartGetDebug();

bool uartIsDriverInstalled(uart_t* uart);

// Negative Pin Number will keep it unmodified, thus this function can set individual pins
void uartSetPins(uart_t* uart, int8_t rxPin, int8_t txPin, int8_t ctsPin, int8_t rtsPin);

// Enables or disables HW Flow Control function -- needs also to set CTS and/or RTS pins
void uartSetHwFlowCtrlMode(uart_t *uart, uint8_t mode, uint8_t threshold);

void uartStartDetectBaudrate(uart_t *uart);
unsigned long uartDetectBaudrate(uart_t *uart);


#ifdef __cplusplus
}
#endif

#endif /* MAIN_ESP32_HAL_UART_H_ */
