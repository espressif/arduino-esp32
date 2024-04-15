// Copyright 2015-2023 Espressif Systems (Shanghai) PTE LTD
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

#include "soc/soc_caps.h"
#if SOC_UART_SUPPORTED

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "hal/uart_types.h"

  struct uart_struct_t;
  typedef struct uart_struct_t uart_t;

  bool _testUartBegin(uint8_t uart_nr, uint32_t baudrate, uint32_t config, int8_t rxPin, int8_t txPin, uint16_t rx_buffer_size, uint16_t tx_buffer_size, bool inverted, uint8_t rxfifo_full_thrhd);
  uart_t* uartBegin(uint8_t uart_nr, uint32_t baudrate, uint32_t config, int8_t rxPin, int8_t txPin, uint16_t rx_buffer_size, uint16_t tx_buffer_size, bool inverted, uint8_t rxfifo_full_thrhd);
  void uartEnd(uint8_t uart_num);

  // This is used to retrieve the Event Queue pointer from a UART IDF Driver in order to allow user to deal with its events
  void uartGetEventQueue(uart_t* uart, QueueHandle_t* q);

  uint32_t uartAvailable(uart_t* uart);
  uint32_t uartAvailableForWrite(uart_t* uart);
  size_t uartReadBytes(uart_t* uart, uint8_t* buffer, size_t size, uint32_t timeout_ms);
  uint8_t uartRead(uart_t* uart);
  uint8_t uartPeek(uart_t* uart);

  void uartWrite(uart_t* uart, uint8_t c);
  void uartWriteBuf(uart_t* uart, const uint8_t* data, size_t len);

  void uartFlush(uart_t* uart);
  void uartFlushTxOnly(uart_t* uart, bool txOnly);

  void uartSetBaudRate(uart_t* uart, uint32_t baud_rate);
  uint32_t uartGetBaudRate(uart_t* uart);

  void uartSetRxInvert(uart_t* uart, bool invert);
  bool uartSetRxTimeout(uart_t* uart, uint8_t numSymbTimeout);
  bool uartSetRxFIFOFull(uart_t* uart, uint8_t numBytesFIFOFull);
  void uartSetFastReading(uart_t* uart);

  void uartSetDebug(uart_t* uart);
  int uartGetDebug();

  bool uartIsDriverInstalled(uart_t* uart);

  // Negative Pin Number will keep it unmodified, thus this function can set individual pins
  // When pins are changed, it will detach the previous ones
  // Can be called before or after begin()
  bool uartSetPins(uint8_t uart_num, int8_t rxPin, int8_t txPin, int8_t ctsPin, int8_t rtsPin);

  // helper functions
  int8_t uart_get_RxPin(uint8_t uart_num);
  int8_t uart_get_TxPin(uint8_t uart_num);
  void uart_init_PeriMan(void);


  // Enables or disables HW Flow Control function -- needs also to set CTS and/or RTS pins
  //    UART_HW_FLOWCTRL_DISABLE = 0x0   disable hardware flow control
  //    UART_HW_FLOWCTRL_RTS     = 0x1   enable RX hardware flow control (rts)
  //    UART_HW_FLOWCTRL_CTS     = 0x2   enable TX hardware flow control (cts)
  //    UART_HW_FLOWCTRL_CTS_RTS = 0x3   enable hardware flow control
  bool uartSetHwFlowCtrlMode(uart_t* uart, uart_hw_flowcontrol_t mode, uint8_t threshold);

  // Used to set RS485 function -- needs to disable HW Flow Control and set RTS pin to use
  // RTS pin becomes RS485 half duplex RE/DE
  //    UART_MODE_UART                   = 0x00    mode: regular UART mode
  //    UART_MODE_RS485_HALF_DUPLEX      = 0x01    mode: half duplex RS485 UART mode control by RTS pin
  //    UART_MODE_IRDA                   = 0x02    mode: IRDA  UART mode
  //    UART_MODE_RS485_COLLISION_DETECT = 0x03    mode: RS485 collision detection UART mode (used for test purposes)
  //    UART_MODE_RS485_APP_CTRL         = 0x04    mode: application control RS485 UART mode (used for test purposes)
  bool uartSetMode(uart_t* uart, uart_mode_t mode);

  void uartStartDetectBaudrate(uart_t* uart);
  unsigned long uartDetectBaudrate(uart_t* uart);

  /*
    These functions are for testing puspose only and can be used in Arduino Sketches
    Those are used in the UART examples
*/

  // Make sure UART's RX signal is connected to TX pin
  // This creates a loop that lets us receive anything we send on the UART
  void uart_internal_loopback(uint8_t uartNum, int8_t rxPin);

  // Routines that generate BREAK in the UART for testing purpose

  // Forces a BREAK in the line based on SERIAL_8N1 configuration at any baud rate
  void uart_send_break(uint8_t uartNum);
  // Sends a buffer and at the end of the stream, it generates BREAK in the line
  int uart_send_msg_with_break(uint8_t uartNum, uint8_t* msg, size_t msgSize);


#ifdef __cplusplus
}
#endif

#endif /* SOC_UART_SUPPORTED */
#endif /* MAIN_ESP32_HAL_UART_H_ */
