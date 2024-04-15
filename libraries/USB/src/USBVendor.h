// Copyright 2015-2020 Espressif Systems (Shanghai) PTE LTD
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

#include "soc/soc_caps.h"
#if SOC_USB_OTG_SUPPORTED

#include "Stream.h"
#include "sdkconfig.h"

#if CONFIG_TINYUSB_VENDOR_ENABLED
#include "esp_event.h"

ESP_EVENT_DECLARE_BASE(ARDUINO_USB_VENDOR_EVENTS);

#define REQUEST_STAGE_SETUP 0
#define REQUEST_STAGE_DATA 1
#define REQUEST_STAGE_ACK 2

#define REQUEST_TYPE_STANDARD 0
#define REQUEST_TYPE_CLASS 1
#define REQUEST_TYPE_VENDOR 2
#define REQUEST_TYPE_INVALID 3

#define REQUEST_RECIPIENT_DEVICE 0
#define REQUEST_RECIPIENT_INTERFACE 1
#define REQUEST_RECIPIENT_ENDPOINT 2
#define REQUEST_RECIPIENT_OTHER 3

#define REQUEST_DIRECTION_OUT 0
#define REQUEST_DIRECTION_IN 1

typedef struct __attribute__((packed)) {
  struct __attribute__((packed)) {
    uint8_t bmRequestRecipient : 5;
    uint8_t bmRequestType : 2;
    uint8_t bmRequestDirection : 1;
  };
  uint8_t bRequest;
  uint16_t wValue;
  uint16_t wIndex;
  uint16_t wLength;
} arduino_usb_control_request_t;

typedef enum {
  ARDUINO_USB_VENDOR_ANY_EVENT = ESP_EVENT_ANY_ID,
  ARDUINO_USB_VENDOR_DATA_EVENT,
  ARDUINO_USB_VENDOR_MAX_EVENT,
} arduino_usb_vendor_event_t;

typedef union {
  struct {
    uint16_t len;
  } data;
} arduino_usb_vendor_event_data_t;

typedef bool (*arduino_usb_vendor_control_request_handler_t)(uint8_t rhport, uint8_t stage, arduino_usb_control_request_t const* request);

class USBVendor : public Stream {
private:
  uint8_t itf;
  arduino_usb_vendor_control_request_handler_t cb;
public:
  USBVendor(uint8_t endpoint_size = 64);
  void begin(void);
  void end(void);
  size_t setRxBufferSize(size_t);
  bool mounted(void);
  size_t write(const uint8_t* buffer, size_t len);
  size_t write(uint8_t);
  int available(void);
  int peek(void);
  int read(void);
  size_t read(uint8_t* buffer, size_t size);
  void flush(void);

  void onEvent(esp_event_handler_t callback);
  void onEvent(arduino_usb_vendor_event_t event, esp_event_handler_t callback);
  void onRequest(arduino_usb_vendor_control_request_handler_t handler);
  bool sendResponse(uint8_t rhport, arduino_usb_control_request_t const* request, void* data = NULL, size_t len = 0);

  bool _onRequest(uint8_t rhport, uint8_t stage, arduino_usb_control_request_t const* request);
  void _onRX(const uint8_t* buffer, size_t len);
};

#endif /* CONFIG_TINYUSB_VENDOR_ENABLED */
#endif /* SOC_USB_OTG_SUPPORTED */
