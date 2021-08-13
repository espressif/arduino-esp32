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
#include <stdint.h>
#include <stdbool.h>
#include "esp32-hal.h"
#include "USB.h"
#if CONFIG_TINYUSB_HID_ENABLED

#include "class/hid/hid.h"
#include "esp_event.h"

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

typedef uint16_t (*tinyusb_hid_device_descriptor_cb_t)(uint8_t * dst, uint8_t report_id);

class USBHIDDevice
{
public:
    uint8_t id;
    USBHIDDevice():id(0){}
    virtual uint16_t _onGetFeature(uint8_t* buffer, uint16_t len){return 0;}
    virtual void _onSetFeature(const uint8_t* buffer, uint16_t len){}
    virtual void _onOutput(const uint8_t* buffer, uint16_t len){}
    virtual void _onInputDone(const uint8_t* buffer, uint16_t len){}
};

class USBHID
{
public:
    USBHID(void);
    void begin(void);
    void end(void);
    void onEvent(esp_event_handler_t callback);
    void onEvent(arduino_usb_hid_event_t event, esp_event_handler_t callback);
    static bool addDevice(USBHIDDevice * device, uint16_t descriptor_len, tinyusb_hid_device_descriptor_cb_t cb);
};

bool tud_hid_n_wait_ready(uint8_t instance, uint32_t timeout_ms);

#endif
