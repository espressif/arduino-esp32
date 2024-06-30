// Copyright 2015-2021 Espressif Systems (Shanghai) PTE LTD
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

#pragma once
#include <stdbool.h>
#include "USBMSC.h"

#if CONFIG_TINYUSB_MSC_ENABLED

#include "esp_event.h"

ESP_EVENT_DECLARE_BASE(ARDUINO_FIRMWARE_MSC_EVENTS);

typedef enum {
  ARDUINO_FIRMWARE_MSC_ANY_EVENT = ESP_EVENT_ANY_ID,
  ARDUINO_FIRMWARE_MSC_START_EVENT = 0,
  ARDUINO_FIRMWARE_MSC_WRITE_EVENT,
  ARDUINO_FIRMWARE_MSC_END_EVENT,
  ARDUINO_FIRMWARE_MSC_ERROR_EVENT,
  ARDUINO_FIRMWARE_MSC_POWER_EVENT,
  ARDUINO_FIRMWARE_MSC_MAX_EVENT,
} arduino_firmware_msc_event_t;

typedef union {
  struct {
    size_t offset;
    size_t size;
  } write;
  struct {
    uint8_t power_condition;
    bool start;
    bool load_eject;
  } power;
  struct {
    size_t size;
  } end;
  struct {
    size_t size;
  } error;
} arduino_firmware_msc_event_data_t;

class FirmwareMSC {
private:
  USBMSC msc;

public:
  FirmwareMSC();
  ~FirmwareMSC();
  bool begin();
  void end();
  void onEvent(esp_event_handler_t callback);
  void onEvent(arduino_firmware_msc_event_t event, esp_event_handler_t callback);
};

#if ARDUINO_USB_MSC_ON_BOOT
extern FirmwareMSC MSC_Update;
#endif

#endif /* CONFIG_TINYUSB_MSC_ENABLED */
