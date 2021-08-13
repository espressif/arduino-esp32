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
#include "Stream.h"
#include "USBHID.h"
#if CONFIG_TINYUSB_HID_ENABLED

ESP_EVENT_DECLARE_BASE(ARDUINO_USB_HID_VENDOR_EVENTS);

typedef enum {
    ARDUINO_USB_HID_VENDOR_ANY_EVENT = ESP_EVENT_ANY_ID,
    ARDUINO_USB_HID_VENDOR_SET_FEATURE_EVENT = 0,
    ARDUINO_USB_HID_VENDOR_GET_FEATURE_EVENT,
    ARDUINO_USB_HID_VENDOR_OUTPUT_EVENT,
    ARDUINO_USB_HID_VENDOR_MAX_EVENT,
} arduino_usb_hid_vendor_event_t;

typedef struct {
    const uint8_t* buffer;
    uint16_t len;
} arduino_usb_hid_vendor_event_data_t;

class USBHIDVendor: public USBHIDDevice, public Stream {
private:
    USBHID hid;
    uint8_t feature[63];
    xSemaphoreHandle tx_sem;
    xQueueHandle rx_queue;
public:
    USBHIDVendor(void);
    void begin(void);
    void end(void);
    size_t setRxBufferSize(size_t);
    size_t write(const uint8_t* buffer, uint16_t len);
    size_t write(uint8_t);
    int available(void);
    int peek(void);
    int read(void);
    size_t read(uint8_t *buffer, size_t size);
    void flush(void);
    
    void onEvent(esp_event_handler_t callback);
    void onEvent(arduino_usb_hid_vendor_event_t event, esp_event_handler_t callback);

    //internal use
    uint16_t _onGetFeature(uint8_t* buffer, uint16_t len);
    void _onSetFeature(const uint8_t* buffer, uint16_t len);
    void _onOutput(const uint8_t* buffer, uint16_t len);
    void _onInputDone(const uint8_t* buffer, uint16_t len);
};

#endif
