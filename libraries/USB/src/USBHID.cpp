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
#include "USBHID.h"

#if CONFIG_TINYUSB_HID_ENABLED

#include "esp32-hal-tinyusb.h"
#include "USB.h"
#include "esp_hid_common.h"

#define USB_HID_DEVICES_MAX 10

ESP_EVENT_DEFINE_BASE(ARDUINO_USB_HID_EVENTS);
esp_err_t arduino_usb_event_post(esp_event_base_t event_base, int32_t event_id, void *event_data, size_t event_data_size, TickType_t ticks_to_wait);
esp_err_t arduino_usb_event_handler_register_with(esp_event_base_t event_base, int32_t event_id, esp_event_handler_t event_handler, void *event_handler_arg);

typedef struct {
    USBHIDDevice * device;
    uint8_t reports_num;
    uint8_t * report_ids;
} tinyusb_hid_device_t;

static tinyusb_hid_device_t tinyusb_hid_devices[USB_HID_DEVICES_MAX];

static uint8_t tinyusb_hid_devices_num = 0;
static bool tinyusb_hid_devices_is_initialized = false;
static xSemaphoreHandle tinyusb_hid_device_input_sem = NULL;
static xSemaphoreHandle tinyusb_hid_device_input_mutex = NULL;

static bool tinyusb_hid_is_initialized = false;
static uint8_t tinyusb_loaded_hid_devices_num = 0;
static uint16_t tinyusb_hid_device_descriptor_len = 0;
static uint8_t * tinyusb_hid_device_descriptor = NULL;
static const char * tinyusb_hid_device_report_types[4] = {"INVALID", "INPUT", "OUTPUT", "FEATURE"};

static bool tinyusb_enable_hid_device(uint16_t descriptor_len, USBHIDDevice * device){
    if(tinyusb_hid_is_initialized){
        log_e("TinyUSB HID has already started! Device not enabled");
        return false;
    }
    if(tinyusb_loaded_hid_devices_num >= USB_HID_DEVICES_MAX){
        log_e("Maximum devices already enabled! Device not enabled");
        return false;
    }
    tinyusb_hid_device_descriptor_len += descriptor_len;
    tinyusb_hid_devices[tinyusb_loaded_hid_devices_num++].device = device;

    log_d("Device[%u] len: %u", tinyusb_loaded_hid_devices_num-1, descriptor_len);
    return true;
}

USBHIDDevice * tinyusb_get_device_by_report_id(uint8_t report_id){
    for(uint8_t i=0; i<tinyusb_loaded_hid_devices_num; i++){
        tinyusb_hid_device_t * device = &tinyusb_hid_devices[i];
        if(device->device && device->reports_num){
            for(uint8_t r=0; r<device->reports_num; r++){
                if(report_id == device->report_ids[r]){
                    return device->device;
                }
            }
        }
    }
    return NULL;
}

static uint16_t tinyusb_on_get_feature(uint8_t report_id, uint8_t* buffer, uint16_t reqlen){
    USBHIDDevice * device = tinyusb_get_device_by_report_id(report_id);
    if(device){
        return device->_onGetFeature(report_id, buffer, reqlen);
    }
    return 0;
}

static bool tinyusb_on_set_feature(uint8_t report_id, const uint8_t* buffer, uint16_t reqlen){
    USBHIDDevice * device = tinyusb_get_device_by_report_id(report_id);
    if(device){
        device->_onSetFeature(report_id, buffer, reqlen);
        return true;
    }
    return false;
}

static bool tinyusb_on_set_output(uint8_t report_id, const uint8_t* buffer, uint16_t reqlen){
    USBHIDDevice * device = tinyusb_get_device_by_report_id(report_id);
    if(device){
        device->_onOutput(report_id, buffer, reqlen);
        return true;
    }
    return false;
}

static uint16_t tinyusb_on_add_descriptor(uint8_t device_index, uint8_t * dst){
    uint16_t res = 0;
    uint8_t report_id = 0, reports_num = 0;
    tinyusb_hid_device_t * device = &tinyusb_hid_devices[device_index];
    if(device->device){
        res = device->device->_onGetDescriptor(dst);
        if(res){

            esp_hid_report_map_t *hid_report_map = esp_hid_parse_report_map(dst, res);
            if(hid_report_map){
                if(device->report_ids){
                    free(device->report_ids);
                }
                device->reports_num = hid_report_map->reports_len;
                device->report_ids = (uint8_t*)malloc(device->reports_num);
                memset(device->report_ids, 0, device->reports_num);
                reports_num = device->reports_num;

                for(uint8_t i=0; i<device->reports_num; i++){
                    if(hid_report_map->reports[i].protocol_mode == ESP_HID_PROTOCOL_MODE_REPORT){
                        report_id = hid_report_map->reports[i].report_id;
                        for(uint8_t r=0; r<device->reports_num; r++){
                            if(!report_id){
                                //todo: handle better when device has no report ID set
                                break;
                            } else if(report_id == device->report_ids[r]){
                                //already added
                                reports_num--;
                                break;
                            } else if(!device->report_ids[r]){
                                //empty slot
                                device->report_ids[r] = report_id;
                                break;
                            }
                        }
                    } else {
                        reports_num--;
                    }
                }
                device->reports_num = reports_num;
                esp_hid_free_report_map(hid_report_map);
            }

        }
    }
    return res;
}


static bool tinyusb_load_enabled_hid_devices(){
    if(tinyusb_hid_device_descriptor != NULL){
        return true;
    }
    tinyusb_hid_device_descriptor = (uint8_t *)malloc(tinyusb_hid_device_descriptor_len);
    if (tinyusb_hid_device_descriptor == NULL) {
        log_e("HID Descriptor Malloc Failed");
        return false;
    }
    uint8_t * dst = tinyusb_hid_device_descriptor;

    for(uint8_t i=0; i<tinyusb_loaded_hid_devices_num; i++){
        uint16_t len = tinyusb_on_add_descriptor(i, dst);
        if (!len) {
            break;
        } else {
            dst += len;
        }
    }

    esp_hid_report_map_t *hid_report_map = esp_hid_parse_report_map(tinyusb_hid_device_descriptor, tinyusb_hid_device_descriptor_len);
    if(hid_report_map){
        log_d("Loaded HID Desriptor with the following reports:");
        for(uint8_t i=0; i<hid_report_map->reports_len; i++){
            if(hid_report_map->reports[i].protocol_mode == ESP_HID_PROTOCOL_MODE_REPORT){
                log_d("  ID: %3u, Type: %7s, Size: %2u, Usage: %8s",
                    hid_report_map->reports[i].report_id,
                    esp_hid_report_type_str(hid_report_map->reports[i].report_type),
                    hid_report_map->reports[i].value_len,
                    esp_hid_usage_str(hid_report_map->reports[i].usage)
                );
            }
        }
        esp_hid_free_report_map(hid_report_map);
    } else {
        log_e("Failed to parse the hid report descriptor!");
        return false;
    }

    return true;
}

extern "C" uint16_t tusb_hid_load_descriptor(uint8_t * dst, uint8_t * itf)
{
    if(tinyusb_hid_is_initialized){
        return 0;
    }
    tinyusb_hid_is_initialized = true;

    uint8_t str_index = tinyusb_add_string_descriptor("TinyUSB HID");
    uint8_t ep_in = tinyusb_get_free_in_endpoint();
    TU_VERIFY (ep_in != 0);
    uint8_t ep_out = tinyusb_get_free_out_endpoint();
    TU_VERIFY (ep_out != 0);
    uint8_t descriptor[TUD_HID_INOUT_DESC_LEN] = {
        // HID Input & Output descriptor
        // Interface number, string index, protocol, report descriptor len, EP OUT & IN address, size & polling interval
        TUD_HID_INOUT_DESCRIPTOR(*itf, str_index, HID_ITF_PROTOCOL_NONE, tinyusb_hid_device_descriptor_len, ep_out, (uint8_t)(0x80 | ep_in), 64, 1)
    };
    *itf+=1;
    memcpy(dst, descriptor, TUD_HID_INOUT_DESC_LEN);
    return TUD_HID_INOUT_DESC_LEN;
}




// Invoked when received GET HID REPORT DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint8_t const * tud_hid_descriptor_report_cb(uint8_t instance){
    log_v("instance: %u", instance);
    if(!tinyusb_load_enabled_hid_devices()){
        return NULL;
    }
    return tinyusb_hid_device_descriptor;
}

// Invoked when received SET_PROTOCOL request
// protocol is either HID_PROTOCOL_BOOT (0) or HID_PROTOCOL_REPORT (1)
void tud_hid_set_protocol_cb(uint8_t instance, uint8_t protocol){
    log_v("instance: %u, protocol:%u", instance, protocol);
    arduino_usb_hid_event_data_t p = {0};
    p.instance = instance;
    p.set_protocol.protocol = protocol;
    arduino_usb_event_post(ARDUINO_USB_HID_EVENTS, ARDUINO_USB_HID_SET_PROTOCOL_EVENT, &p, sizeof(arduino_usb_hid_event_data_t), portMAX_DELAY);
}

// Invoked when received SET_IDLE request. return false will stall the request
// - Idle Rate = 0 : only send report if there is changes, i.e skip duplication
// - Idle Rate > 0 : skip duplication, but send at least 1 report every idle rate (in unit of 4 ms).
bool tud_hid_set_idle_cb(uint8_t instance, uint8_t idle_rate){
    log_v("instance: %u, idle_rate:%u", instance, idle_rate);
    arduino_usb_hid_event_data_t p = {0};
    p.instance = instance;
    p.set_idle.idle_rate = idle_rate;
    arduino_usb_event_post(ARDUINO_USB_HID_EVENTS, ARDUINO_USB_HID_SET_IDLE_EVENT, &p, sizeof(arduino_usb_hid_event_data_t), portMAX_DELAY);
    return true;
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen){
    uint16_t res = tinyusb_on_get_feature(report_id, buffer, reqlen);
    if(!res){
        log_d("instance: %u, report_id: %u, report_type: %s, reqlen: %u", instance, report_id, tinyusb_hid_device_report_types[report_type], reqlen);
    }
    return res;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize){
    if(!report_id && !report_type){
        if(!tinyusb_on_set_output(0, buffer, bufsize) && !tinyusb_on_set_output(buffer[0], buffer+1, bufsize-1)){
            log_d("instance: %u, report_id: %u, report_type: %s, bufsize: %u", instance, buffer[0], tinyusb_hid_device_report_types[HID_REPORT_TYPE_OUTPUT], bufsize-1);
        }
    } else {
        if(!tinyusb_on_set_feature(report_id, buffer, bufsize)){
            log_d("instance: %u, report_id: %u, report_type: %s, bufsize: %u", instance, report_id, tinyusb_hid_device_report_types[report_type], bufsize);
        }
    }
}

USBHID::USBHID(){
    if(!tinyusb_hid_devices_is_initialized){
        tinyusb_hid_devices_is_initialized = true;
        for(uint8_t i=0; i<USB_HID_DEVICES_MAX; i++){
            memset(&tinyusb_hid_devices[i], 0, sizeof(tinyusb_hid_device_t));
        }
        tinyusb_hid_devices_num = 0;
        tinyusb_enable_interface(USB_INTERFACE_HID, TUD_HID_INOUT_DESC_LEN, tusb_hid_load_descriptor);
    }
}

void USBHID::begin(){
    if(tinyusb_hid_device_input_sem == NULL){
        tinyusb_hid_device_input_sem = xSemaphoreCreateBinary();
    }
    if(tinyusb_hid_device_input_mutex == NULL){
        tinyusb_hid_device_input_mutex = xSemaphoreCreateMutex();
    }
}

void USBHID::end(){
    if (tinyusb_hid_device_input_sem != NULL) {
        vSemaphoreDelete(tinyusb_hid_device_input_sem);
        tinyusb_hid_device_input_sem = NULL;
    }
    if (tinyusb_hid_device_input_mutex != NULL) {
        vSemaphoreDelete(tinyusb_hid_device_input_mutex);
        tinyusb_hid_device_input_mutex = NULL;
    }
}

bool USBHID::ready(void){
    return tud_hid_n_ready(0);
}

void tud_hid_report_complete_cb(uint8_t instance, uint8_t const* report, uint8_t len){
    if (tinyusb_hid_device_input_sem) {
        xSemaphoreGive(tinyusb_hid_device_input_sem);
    }
}

bool USBHID::SendReport(uint8_t id, const void* data, size_t len, uint32_t timeout_ms){
    if(!tinyusb_hid_device_input_sem || !tinyusb_hid_device_input_mutex){
        log_e("TX Semaphore is NULL. You must call USBHID::begin() before you can send reports");
        return false;
    }

    if(xSemaphoreTake(tinyusb_hid_device_input_mutex, timeout_ms / portTICK_PERIOD_MS) != pdTRUE){
        log_e("report %u mutex failed", id);
        return false;
    }

    bool res = ready();
    if(!res){
        log_e("not ready");
    } else {
        res = tud_hid_n_report(0, id, data, len);
        if(!res){
            log_e("report %u failed", id);
        } else {
            xSemaphoreTake(tinyusb_hid_device_input_sem, 0);
            if(xSemaphoreTake(tinyusb_hid_device_input_sem, timeout_ms / portTICK_PERIOD_MS) != pdTRUE){
                log_e("report %u wait failed", id);
                res = false;
            }
        }
    }

    xSemaphoreGive(tinyusb_hid_device_input_mutex);
    return res;
}

bool USBHID::addDevice(USBHIDDevice * device, uint16_t descriptor_len){
    if(device && tinyusb_loaded_hid_devices_num < USB_HID_DEVICES_MAX){
        if(!tinyusb_enable_hid_device(descriptor_len, device)){
            return false;
        }
        return true;
    }
    return false;
}

void USBHID::onEvent(esp_event_handler_t callback){
    onEvent(ARDUINO_USB_HID_ANY_EVENT, callback);
}
void USBHID::onEvent(arduino_usb_hid_event_t event, esp_event_handler_t callback){
    arduino_usb_event_handler_register_with(ARDUINO_USB_HID_EVENTS, event, callback, this);
}

#endif /* CONFIG_TINYUSB_HID_ENABLED */
