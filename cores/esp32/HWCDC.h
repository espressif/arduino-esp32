// Copyright 2015-2024 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once

#include "sdkconfig.h"
#include "soc/soc_caps.h"

#if SOC_USB_SERIAL_JTAG_SUPPORTED

#include <inttypes.h>
#include "esp_event.h"
#include "Stream.h"
#include "driver/usb_serial_jtag.h"

ESP_EVENT_DECLARE_BASE(ARDUINO_HW_CDC_EVENTS);

typedef enum {
  ARDUINO_HW_CDC_ANY_EVENT = ESP_EVENT_ANY_ID,
  ARDUINO_HW_CDC_CONNECTED_EVENT = 0,
  ARDUINO_HW_CDC_BUS_RESET_EVENT,
  ARDUINO_HW_CDC_RX_EVENT,
  ARDUINO_HW_CDC_TX_EVENT,
  ARDUINO_HW_CDC_MAX_EVENT,
} arduino_hw_cdc_event_t;

typedef union {
  struct {
    size_t len;
  } rx;
  struct {
    size_t len;
  } tx;
} arduino_hw_cdc_event_data_t;

class HWCDC : public Stream {
private:
  static bool deinit(void *busptr);
  static bool isCDC_Connected();

public:
  HWCDC();
  ~HWCDC();

  void onEvent(esp_event_handler_t callback);
  void onEvent(arduino_hw_cdc_event_t event, esp_event_handler_t callback);

  size_t setRxBufferSize(size_t);
  size_t setTxBufferSize(size_t);
  void setTxTimeoutMs(uint32_t timeout);
  void begin(unsigned long baud = 0);
  void end();

  int available(void);
  int availableForWrite(void);
  int peek(void);
  int read(void);
  size_t read(uint8_t *buffer, size_t size);
  size_t write(uint8_t);
  size_t write(const uint8_t *buffer, size_t size);
  void flush(void);

  inline static bool isPlugged(void) {
    // SOF ISR is causing esptool to be unable to upload firmware to the board
    // Using IDF 5.1 helper function because it is based on Timer check instead of ISR
    return usb_serial_jtag_is_connected();
  }

  inline static bool isConnected(void) {
    return isCDC_Connected();
  }

  inline size_t read(char *buffer, size_t size) {
    return read((uint8_t *)buffer, size);
  }
  inline size_t write(const char *buffer, size_t size) {
    return write((uint8_t *)buffer, size);
  }
  inline size_t write(const char *s) {
    return write((uint8_t *)s, strlen(s));
  }
  inline size_t write(unsigned long n) {
    return write((uint8_t)n);
  }
  inline size_t write(long n) {
    return write((uint8_t)n);
  }
  inline size_t write(unsigned int n) {
    return write((uint8_t)n);
  }
  inline size_t write(int n) {
    return write((uint8_t)n);
  }
  operator bool() const;
  void setDebugOutput(bool);
  uint32_t baudRate() {
    return 115200;
  }
};
#if ARDUINO_USB_MODE && ARDUINO_USB_CDC_ON_BOOT  // Hardware JTAG CDC selected
#ifndef HWCDC_SERIAL_IS_DEFINED
#define HWCDC_SERIAL_IS_DEFINED 1
#endif
// HWCDCSerial is always available to be used
extern HWCDC HWCDCSerial;
#endif

#endif /* SOC_USB_SERIAL_JTAG_SUPPORTED */
