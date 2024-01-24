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

#include "USB.h"

#if SOC_USB_OTG_SUPPORTED
#if CONFIG_TINYUSB_ENABLED

#include "pins_arduino.h"
#include "esp32-hal.h"
#include "esp32-hal-tinyusb.h"
#include "common/tusb_common.h"
#include "StreamString.h"
#include "rom/ets_sys.h"
#include "esp_mac.h"

#ifndef USB_VID
#define USB_VID USB_ESPRESSIF_VID
#endif
#ifndef USB_PID
#define USB_PID 0x0002
#endif
#ifndef USB_MANUFACTURER
#define USB_MANUFACTURER "Espressif Systems"
#endif
#ifndef USB_PRODUCT
#define USB_PRODUCT ARDUINO_BOARD
#endif
#ifndef USB_SERIAL
#if CONFIG_IDF_TARGET_ESP32S3
#define USB_SERIAL "__MAC__"
#else
#define USB_SERIAL "0"
#endif
#endif
#ifndef USB_WEBUSB_ENABLED
#define USB_WEBUSB_ENABLED false
#endif
#ifndef USB_WEBUSB_URL
#define USB_WEBUSB_URL "https://espressif.github.io/arduino-esp32/webusb.html"
#endif

#if CFG_TUD_DFU
__attribute__((weak)) uint16_t load_dfu_ota_descriptor(uint8_t * dst, uint8_t * itf) {
    return 0;
}
#elif CFG_TUD_DFU_RUNTIME
static uint16_t load_dfu_descriptor(uint8_t * dst, uint8_t * itf)
{
#define DFU_ATTRS (DFU_ATTR_CAN_DOWNLOAD | DFU_ATTR_CAN_UPLOAD | DFU_ATTR_MANIFESTATION_TOLERANT)

    uint8_t str_index = tinyusb_add_string_descriptor("TinyUSB DFU_RT");
    uint8_t descriptor[TUD_DFU_RT_DESC_LEN] = {
            // Interface number, string index, attributes, detach timeout, transfer size */
            TUD_DFU_RT_DESCRIPTOR(*itf, str_index, DFU_ATTRS, 700, 64)
    };
    *itf+=1;
    memcpy(dst, descriptor, TUD_DFU_RT_DESC_LEN);
    return TUD_DFU_RT_DESC_LEN;
}
#endif /* CFG_TUD_DFU_RUNTIME */

#if CFG_TUD_DFU_RUNTIME
// Invoked on DFU_DETACH request to reboot to the bootloader
void tud_dfu_runtime_reboot_to_dfu_cb(void)
{
    usb_persist_restart(RESTART_BOOTLOADER_DFU);
}
#endif /* CFG_TUD_DFU_RUNTIME */

ESP_EVENT_DEFINE_BASE(ARDUINO_USB_EVENTS);

static esp_event_loop_handle_t arduino_usb_event_loop_handle = NULL;

esp_err_t arduino_usb_event_post(esp_event_base_t event_base, int32_t event_id, void *event_data, size_t event_data_size, TickType_t ticks_to_wait){
    if(arduino_usb_event_loop_handle == NULL){
        return ESP_FAIL;
    }
    return esp_event_post_to(arduino_usb_event_loop_handle, event_base, event_id, event_data, event_data_size, ticks_to_wait);
}
esp_err_t arduino_usb_event_handler_register_with(esp_event_base_t event_base, int32_t event_id, esp_event_handler_t event_handler, void *event_handler_arg){
    if(arduino_usb_event_loop_handle == NULL){
        return ESP_FAIL;
    }
    return esp_event_handler_register_with(arduino_usb_event_loop_handle, event_base, event_id, event_handler, event_handler_arg);
}

static bool tinyusb_device_mounted = false;
static bool tinyusb_device_suspended = false;

// Invoked when device is mounted (configured)
void tud_mount_cb(void){
    tinyusb_device_mounted = true;
    arduino_usb_event_data_t p;
    arduino_usb_event_post(ARDUINO_USB_EVENTS, ARDUINO_USB_STARTED_EVENT, &p, sizeof(arduino_usb_event_data_t), portMAX_DELAY);
}

// Invoked when device is unmounted
void tud_umount_cb(void){
    tinyusb_device_mounted = false;
    arduino_usb_event_data_t p;
    arduino_usb_event_post(ARDUINO_USB_EVENTS, ARDUINO_USB_STOPPED_EVENT, &p, sizeof(arduino_usb_event_data_t), portMAX_DELAY);
}

// Invoked when usb bus is suspended
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en){
    tinyusb_device_suspended = true;
    arduino_usb_event_data_t p;
    p.suspend.remote_wakeup_en = remote_wakeup_en;
    arduino_usb_event_post(ARDUINO_USB_EVENTS, ARDUINO_USB_SUSPEND_EVENT, &p, sizeof(arduino_usb_event_data_t), portMAX_DELAY);
}

// Invoked when usb bus is resumed
void tud_resume_cb(void){
    tinyusb_device_suspended = false;
    arduino_usb_event_data_t p;
    arduino_usb_event_post(ARDUINO_USB_EVENTS, ARDUINO_USB_RESUME_EVENT, &p, sizeof(arduino_usb_event_data_t), portMAX_DELAY);
}

ESPUSB::ESPUSB(size_t task_stack_size, uint8_t event_task_priority)
:vid(USB_VID)
,pid(USB_PID)
,product_name(USB_PRODUCT)
,manufacturer_name(USB_MANUFACTURER)
,serial_number(USB_SERIAL)
,fw_version(0x0100)
,usb_version(0x0200)// at least 2.1 or 3.x for BOS & webUSB
,usb_class(TUSB_CLASS_MISC)
,usb_subclass(MISC_SUBCLASS_COMMON)
,usb_protocol(MISC_PROTOCOL_IAD)
,usb_attributes(TUSB_DESC_CONFIG_ATT_SELF_POWERED)
,usb_power_ma(500)
,webusb_enabled(USB_WEBUSB_ENABLED)
,webusb_url(USB_WEBUSB_URL)
,_started(false)
,_task_stack_size(task_stack_size)
,_event_task_priority(event_task_priority)
{
    if (!arduino_usb_event_loop_handle) {
        esp_event_loop_args_t event_task_args = {
            .queue_size = 5,
            .task_name = "arduino_usb_events",
            .task_priority = _event_task_priority,
            .task_stack_size = _task_stack_size,
            .task_core_id = tskNO_AFFINITY
        };
        if (esp_event_loop_create(&event_task_args, &arduino_usb_event_loop_handle) != ESP_OK) {
            log_e("esp_event_loop_create failed");
        }
    }
}

ESPUSB::~ESPUSB(){
    if (arduino_usb_event_loop_handle) {
        esp_event_loop_delete(arduino_usb_event_loop_handle);
        arduino_usb_event_loop_handle = NULL;
    }
}

bool ESPUSB::begin(){
    if(!_started){
#if CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3
        if(serial_number == "__MAC__"){
            StreamString s;
            uint8_t m[6];
            esp_efuse_mac_get_default(m);
            s.printf("%02X%02X%02X%02X%02X%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
            serial_number = s;
        }
#endif
        tinyusb_device_config_t tinyusb_device_config = {
                .vid = vid,
                .pid = pid,
                .product_name = product_name.c_str(),
                .manufacturer_name = manufacturer_name.c_str(),
                .serial_number = serial_number.c_str(),
                .fw_version = fw_version,
                .usb_version = usb_version,
                .usb_class = usb_class,
                .usb_subclass = usb_subclass,
                .usb_protocol = usb_protocol,
                .usb_attributes = usb_attributes,
                .usb_power_ma = usb_power_ma,
                .webusb_enabled = webusb_enabled,
                .webusb_url = webusb_url.c_str()
        };
        _started = tinyusb_init(&tinyusb_device_config) == ESP_OK; 
    }
    return _started;
}

void ESPUSB::onEvent(esp_event_handler_t callback){
    onEvent(ARDUINO_USB_ANY_EVENT, callback);
}
void ESPUSB::onEvent(arduino_usb_event_t event, esp_event_handler_t callback){
    arduino_usb_event_handler_register_with(ARDUINO_USB_EVENTS, event, callback, this);
}

ESPUSB::operator bool() const
{
    return _started && tinyusb_device_mounted;
}

bool ESPUSB::enableDFU(){
#if CFG_TUD_DFU
    return tinyusb_enable_interface(USB_INTERFACE_DFU, TUD_DFU_DESC_LEN(1), load_dfu_ota_descriptor) == ESP_OK;
#elif CFG_TUD_DFU_RUNTIME
    return tinyusb_enable_interface(USB_INTERFACE_DFU, TUD_DFU_RT_DESC_LEN, load_dfu_descriptor) == ESP_OK;
#endif /* CFG_TUD_DFU_RUNTIME */
    return false;
}

bool ESPUSB::VID(uint16_t v){
    if(!_started){
        vid = v;
    }
    return !_started;
}
uint16_t ESPUSB::VID(void){
    return vid;
}

bool ESPUSB::PID(uint16_t p){
    if(!_started){
        pid = p;
    }
    return !_started;
}
uint16_t ESPUSB::PID(void){
    return pid;
}

bool ESPUSB::firmwareVersion(uint16_t version){
    if(!_started){
        fw_version = version;
    }
    return !_started;
}
uint16_t ESPUSB::firmwareVersion(void){
    return fw_version;
}

bool ESPUSB::usbVersion(uint16_t version){
    if(!_started){
        usb_version = version;
    }
    return !_started;
}
uint16_t ESPUSB::usbVersion(void){
    return usb_version;
}

bool ESPUSB::usbPower(uint16_t mA){
    if(!_started){
        usb_power_ma = mA;
    }
    return !_started;
}
uint16_t ESPUSB::usbPower(void){
    return usb_power_ma;
}

bool ESPUSB::usbClass(uint8_t _class){
    if(!_started){
        usb_class = _class;
    }
    return !_started;
}
uint8_t ESPUSB::usbClass(void){
    return usb_class;
}

bool ESPUSB::usbSubClass(uint8_t subClass){
    if(!_started){
        usb_subclass = subClass;
    }
    return !_started;
}
uint8_t ESPUSB::usbSubClass(void){
    return usb_subclass;
}

bool ESPUSB::usbProtocol(uint8_t protocol){
    if(!_started){
        usb_protocol = protocol;
    }
    return !_started;
}
uint8_t ESPUSB::usbProtocol(void){
    return usb_protocol;
}

bool ESPUSB::usbAttributes(uint8_t attr){
    if(!_started){
        usb_attributes = attr;
    }
    return !_started;
}
uint8_t ESPUSB::usbAttributes(void){
    return usb_attributes;
}

bool ESPUSB::webUSB(bool enabled){
    if(!_started){
        webusb_enabled = enabled;
        if(enabled && usb_version < 0x0210){
            usb_version = 0x0210;
        }
    }
    return !_started;
}
bool ESPUSB::webUSB(void){
    return webusb_enabled;
}

bool ESPUSB::productName(const char * name){
    if(!_started){
        product_name = name;
    }
    return !_started;
}
const char * ESPUSB::productName(void){
    return product_name.c_str();
}

bool ESPUSB::manufacturerName(const char * name){
    if(!_started){
        manufacturer_name = name;
    }
    return !_started;
}
const char * ESPUSB::manufacturerName(void){
    return manufacturer_name.c_str();
}

bool ESPUSB::serialNumber(const char * name){
    if(!_started){
        serial_number = name;
    }
    return !_started;
}
const char * ESPUSB::serialNumber(void){
    return serial_number.c_str();
}

bool ESPUSB::webUSBURL(const char * name){
    if(!_started){
        webusb_url = name;
    }
    return !_started;
}
const char * ESPUSB::webUSBURL(void){
    return webusb_url.c_str();
}

ESPUSB USB;

#endif /* CONFIG_TINYUSB_ENABLED */
#endif /* SOC_USB_OTG_SUPPORTED */
