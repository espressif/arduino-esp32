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
#include "esp32-hal.h"
#include "esp32-hal-tinyusb.h"
#include "USBHID.h"

#if CFG_TUD_HID
#define USB_HID_DEVICES_MAX 10

ESP_EVENT_DEFINE_BASE(ARDUINO_USB_HID_EVENTS);
esp_err_t arduino_usb_event_post(esp_event_base_t event_base, int32_t event_id, void *event_data, size_t event_data_size, TickType_t ticks_to_wait);
esp_err_t arduino_usb_event_handler_register_with(esp_event_base_t event_base, int32_t event_id, esp_event_handler_t event_handler, void *event_handler_arg);

static void log_print_buf_line(const uint8_t *b, size_t len){
    for(size_t i = 0; i<len; i++){
        log_printf("%s0x%02x,",i?" ":"", b[i]);
    }
    for(size_t i = len; i<16; i++){
        log_printf("      ");
    }
    log_printf("    // ");
    for(size_t i = 0; i<len; i++){
        log_printf("%c",((b[i] >= 0x20) && (b[i] < 0x80))?b[i]:'.');
    }
    log_printf("\n");
}

void log_print_buf(const uint8_t *b, size_t len){
    if(!len || !b){
        return;
    }
    for(size_t i = 0; i<len; i+=16){
        log_printf("/* 0x%04X */ ", i);
        log_print_buf_line(b+i, ((len-i)<16)?(len - i):16);
    }
}


static bool tinyusb_hid_is_initialized = false;
static tinyusb_hid_device_descriptor_cb_t tinyusb_loaded_hid_devices_callbacks[USB_HID_DEVICES_MAX];
static uint8_t tinyusb_loaded_hid_devices_num = 0;
static uint8_t tinyusb_loaded_hid_devices_report_id = 1;
static uint16_t tinyusb_hid_device_descriptor_len = 0;
static uint8_t * tinyusb_hid_device_descriptor = NULL;
static const char * tinyusb_hid_device_report_types[4] = {"INVALID", "INPUT", "OUTPUT", "FEATURE"};

static bool tinyusb_enable_hid_device(uint16_t descriptor_len, tinyusb_hid_device_descriptor_cb_t cb){
    if(tinyusb_hid_is_initialized){
        log_e("TinyUSB HID has already started! Device not enabled");
        return false;
    }
    if(tinyusb_loaded_hid_devices_num >= USB_HID_DEVICES_MAX){
        log_e("Maximum devices already enabled! Device not enabled");
        return false;
    }
    tinyusb_hid_device_descriptor_len += descriptor_len;
    tinyusb_loaded_hid_devices_callbacks[tinyusb_loaded_hid_devices_num++] = cb;
    log_d("Device enabled");
    return true;
}

static uint16_t tinyusb_load_hid_descriptor(uint8_t interface, uint8_t * dst, uint8_t report_id)
{
    if(interface < USB_HID_DEVICES_MAX && tinyusb_loaded_hid_devices_callbacks[interface] != NULL){
        return tinyusb_loaded_hid_devices_callbacks[interface](dst, report_id);
    }
    return 0;
}

static bool tinyusb_load_enabled_hid_devices(){
    tinyusb_hid_device_descriptor = (uint8_t *)malloc(tinyusb_hid_device_descriptor_len);
    if (tinyusb_hid_device_descriptor == NULL) {
        log_e("HID Descriptor Malloc Failed");
        return false;
    }
    uint8_t * dst = tinyusb_hid_device_descriptor;

    for(uint8_t i=0; i<USB_HID_DEVICES_MAX; i++){
            uint16_t len = tinyusb_load_hid_descriptor(i, dst, i+1);
            if (!len) {
                break;
            } else {
                dst += len;
            }
    }
    log_d("Load Done: reports: %u, descr_len: %u", tinyusb_loaded_hid_devices_report_id - 1, tinyusb_hid_device_descriptor_len);
    return true;
}

extern "C" uint16_t tusb_hid_load_descriptor(uint8_t * dst, uint8_t * itf)
{
    if(tinyusb_hid_is_initialized){
        return 0;
    }
    tinyusb_hid_is_initialized = true;
    if(!tinyusb_load_enabled_hid_devices()){
        return 0;
    }

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



static USBHIDDevice * tinyusb_hid_devices[USB_HID_DEVICES_MAX+1];
static uint8_t tinyusb_hid_devices_num = 0;
static bool tinyusb_hid_devices_is_initialized = false;


// Invoked when received GET HID REPORT DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint8_t const * tud_hid_descriptor_report_cb(uint8_t instance){
    log_d("instance: %u", instance);
    return tinyusb_hid_device_descriptor;
}

// Invoked when received SET_PROTOCOL request
// protocol is either HID_PROTOCOL_BOOT (0) or HID_PROTOCOL_REPORT (1)
void tud_hid_set_protocol_cb(uint8_t instance, uint8_t protocol){
    log_d("instance: %u, protocol:%u", instance, protocol);
    arduino_usb_hid_event_data_t p = {0};
    p.instance = instance;
    p.set_protocol.protocol = protocol;
    arduino_usb_event_post(ARDUINO_USB_HID_EVENTS, ARDUINO_USB_HID_SET_PROTOCOL_EVENT, &p, sizeof(arduino_usb_hid_event_data_t), portMAX_DELAY);
}

// Invoked when received SET_IDLE request. return false will stall the request
// - Idle Rate = 0 : only send report if there is changes, i.e skip duplication
// - Idle Rate > 0 : skip duplication, but send at least 1 report every idle rate (in unit of 4 ms).
bool tud_hid_set_idle_cb(uint8_t instance, uint8_t idle_rate){
    log_d("instance: %u, idle_rate:%u", instance, idle_rate);
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
    if(report_id < USB_HID_DEVICES_MAX && tinyusb_hid_devices[report_id]){
        return tinyusb_hid_devices[report_id]->_onGetFeature(buffer, reqlen);
    }
    log_d("instance: %u, report_id: %u, report_type: %s, reqlen: %u", instance, report_id, tinyusb_hid_device_report_types[report_type], reqlen);
    return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize){
    if(!report_id && !report_type){
        report_id = buffer[0];
        if(report_id < USB_HID_DEVICES_MAX && tinyusb_hid_devices[report_id]){
            tinyusb_hid_devices[report_id]->_onOutput(buffer+1, bufsize-1);
        } else {
            log_d("instance: %u, report_id: %u, report_type: %s, bufsize: %u", instance, *buffer, tinyusb_hid_device_report_types[HID_REPORT_TYPE_OUTPUT], bufsize-1);
            log_print_buf(buffer+1, bufsize-1);
        }
    } else {
        if(report_id < USB_HID_DEVICES_MAX && tinyusb_hid_devices[report_id]){
            tinyusb_hid_devices[report_id]->_onSetFeature(buffer, bufsize);
        } else {
            log_d("instance: %u, report_id: %u, report_type: %s, bufsize: %u", instance, report_id, tinyusb_hid_device_report_types[report_type], bufsize);
            log_print_buf(buffer, bufsize);
        }
    }
}

// Invoked when sent REPORT successfully to host
// Application can use this to send the next report
// Note: For composite reports, report[0] is report ID
void tud_hid_report_complete_cb(uint8_t instance, uint8_t const* report, uint8_t len){
    if(report[0] < USB_HID_DEVICES_MAX && tinyusb_hid_devices[report[0]]){
        tinyusb_hid_devices[report[0]]->_onInputDone(report+1, len-1);
    } else {
        log_i("instance: %u, report_id: %u, report_type: %s, bufsize:%u", instance, report[0], tinyusb_hid_device_report_types[HID_REPORT_TYPE_INPUT], len-1);
        log_print_buf(report+1, len-1);
    }
}

bool tud_hid_n_wait_ready(uint8_t instance, uint32_t timeout_ms){
    if(tud_hid_n_ready(instance)){
        return true;
    }
    uint32_t start_ms = millis();
    while(!tud_hid_n_ready(instance)){
        if((millis() - start_ms) > timeout_ms){
            return false;
        }
        delay(1);
    }
    return true;
}

USBHID::USBHID(){
    if(!tinyusb_hid_devices_is_initialized){
        tinyusb_hid_devices_is_initialized = true;
        memset(tinyusb_hid_devices, 0, sizeof(tinyusb_hid_devices));
        tinyusb_hid_devices_num = 0;
        tinyusb_enable_interface(USB_INTERFACE_HID, TUD_HID_INOUT_DESC_LEN, tusb_hid_load_descriptor);
    }

}

void USBHID::begin(){

}

void USBHID::end(){

}

bool USBHID::addDevice(USBHIDDevice * device, uint16_t descriptor_len, tinyusb_hid_device_descriptor_cb_t cb){
    if(device && tinyusb_loaded_hid_devices_num < USB_HID_DEVICES_MAX){
        if(!tinyusb_enable_hid_device(descriptor_len, cb)){
            return false;
        }
        device->id = tinyusb_loaded_hid_devices_num;
        tinyusb_hid_devices[tinyusb_loaded_hid_devices_num] = device;
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

#endif /* CONFIG_USB_HID_ENABLED */
