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

#include "sdkconfig.h"
#if CONFIG_TINYUSB_ENABLED

#include "esp_event.h"
#include "USBCDC.h"

#define ARDUINO_USB_ON_BOOT (ARDUINO_USB_CDC_ON_BOOT|ARDUINO_USB_MSC_ON_BOOT|ARDUINO_USB_DFU_ON_BOOT)

ESP_EVENT_DECLARE_BASE(ARDUINO_USB_EVENTS);

typedef enum {
    ARDUINO_USB_ANY_EVENT = ESP_EVENT_ANY_ID,
    ARDUINO_USB_STARTED_EVENT = 0,
    ARDUINO_USB_STOPPED_EVENT,
    ARDUINO_USB_SUSPEND_EVENT,
    ARDUINO_USB_RESUME_EVENT,
    ARDUINO_USB_MAX_EVENT,
} arduino_usb_event_t;

typedef union {
    struct {
        bool remote_wakeup_en;
    } suspend;
} arduino_usb_event_data_t;

class ESPUSB {
    public:
        ESPUSB(size_t event_task_stack_size=2048, uint8_t event_task_priority=5);
        ~ESPUSB();
        
        void onEvent(esp_event_handler_t callback);
        void onEvent(arduino_usb_event_t event, esp_event_handler_t callback);
        
        bool VID(uint16_t v);
        uint16_t VID(void);

        bool PID(uint16_t p);
        uint16_t PID(void);

        bool firmwareVersion(uint16_t version);
        uint16_t firmwareVersion(void);

        bool usbVersion(uint16_t version);
        uint16_t usbVersion(void);

        bool usbPower(uint16_t mA);
        uint16_t usbPower(void);

        bool usbClass(uint8_t _class);
        uint8_t usbClass(void);

        bool usbSubClass(uint8_t subClass);
        uint8_t usbSubClass(void);

        bool usbProtocol(uint8_t protocol);
        uint8_t usbProtocol(void);

        bool usbAttributes(uint8_t attr);
        uint8_t usbAttributes(void);

        bool webUSB(bool enabled);
        bool webUSB(void);
        
        bool productName(const char * name);
        const char * productName(void);

        bool manufacturerName(const char * name);
        const char * manufacturerName(void);

        bool serialNumber(const char * name);
        const char * serialNumber(void);

        bool webUSBURL(const char * name);
        const char * webUSBURL(void);

        bool enableDFU();
        bool begin();
        operator bool() const;
        
    private:
        uint16_t vid;
        uint16_t pid;
        String product_name;
        String manufacturer_name;
        String serial_number;
        uint16_t fw_version;
        uint16_t usb_version;
        uint8_t usb_class;
        uint8_t usb_subclass;
        uint8_t usb_protocol;
        uint8_t usb_attributes;
        uint16_t usb_power_ma;
        bool webusb_enabled;
        String webusb_url;
        
        bool _started;
        size_t _task_stack_size;
        uint8_t _event_task_priority;
};

extern ESPUSB USB;


#endif /* CONFIG_TINYUSB_ENABLED */
#endif /* SOC_USB_OTG_SUPPORTED */
