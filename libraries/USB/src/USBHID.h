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

#include <stdint.h>
#include <stdbool.h>
#include "sdkconfig.h"

#if CONFIG_TINYUSB_HID_ENABLED
#include "esp_event.h"
#include "class/hid/hid.h"
#include "class/hid/hid_device.h"

// Used by the included TinyUSB drivers
enum {
  HID_REPORT_ID_NONE,
  HID_REPORT_ID_KEYBOARD,
  HID_REPORT_ID_MOUSE,
  HID_REPORT_ID_GAMEPAD,
  HID_REPORT_ID_CONSUMER_CONTROL,
  HID_REPORT_ID_SYSTEM_CONTROL,
  HID_REPORT_ID_VENDOR
};

ESP_EVENT_DECLARE_BASE(ARDUINO_USB_HID_EVENTS);

typedef enum {
  ARDUINO_USB_HID_ANY_EVENT = ESP_EVENT_ANY_ID,
  ARDUINO_USB_HID_SET_PROTOCOL_EVENT = 0,
  ARDUINO_USB_HID_SET_IDLE_EVENT,
  ARDUINO_USB_HID_MAX_EVENT,
} arduino_usb_hid_event_t;

typedef struct {
  uint8_t instance;
  union {
    struct {
      uint8_t protocol;
    } set_protocol;
    struct {
      uint8_t idle_rate;
    } set_idle;
  };
} arduino_usb_hid_event_data_t;

class USBHIDDevice {
public:
  virtual uint16_t _onGetDescriptor(uint8_t *buffer) {
    return 0;
  }
  virtual uint16_t _onGetFeature(uint8_t report_id, uint8_t *buffer, uint16_t len) {
    return 0;
  }
  virtual void _onSetFeature(uint8_t report_id, const uint8_t *buffer, uint16_t len) {}
  virtual void _onOutput(uint8_t report_id, const uint8_t *buffer, uint16_t len) {}
};

class USBHID {
public:
  USBHID(hid_interface_protocol_enum_t itf_protocol = HID_ITF_PROTOCOL_NONE);
  void begin(void);
  void end(void);
  bool ready(void);
  bool SendReport(uint8_t report_id, const void *data, size_t len, uint32_t timeout_ms = 100);
  void onEvent(esp_event_handler_t callback);
  void onEvent(arduino_usb_hid_event_t event, esp_event_handler_t callback);
  static bool addDevice(USBHIDDevice *device, uint16_t descriptor_len);
};

#endif /* CONFIG_TINYUSB_HID_ENABLED */
#endif /* SOC_USB_OTG_SUPPORTED */
