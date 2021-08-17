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
#include "USBHIDVendor.h"

#if CFG_TUD_HID

ESP_EVENT_DEFINE_BASE(ARDUINO_USB_HID_VENDOR_EVENTS);
esp_err_t arduino_usb_event_post(esp_event_base_t event_base, int32_t event_id, void *event_data, size_t event_data_size, TickType_t ticks_to_wait);
esp_err_t arduino_usb_event_handler_register_with(esp_event_base_t event_base, int32_t event_id, esp_event_handler_t event_handler, void *event_handler_arg);

// HID Generic Input, Output & Feature
// - 1st parameter is report size (mandatory)
// - 2nd parameter is report id HID_REPORT_ID(n) (optional)
#define TUD_HID_REPORT_DESC_GENERIC_INOUT_FEATURE(report_size, ...) \
    HID_USAGE_PAGE_N ( HID_USAGE_PAGE_VENDOR, 2   ),\
    HID_USAGE        ( 0x01                       ),\
    HID_COLLECTION   ( HID_COLLECTION_APPLICATION ),\
      /* Report ID if any */\
      __VA_ARGS__ \
      /* Input */ \
      HID_USAGE       ( 0x02                                   ),\
      HID_LOGICAL_MIN ( 0x00                                   ),\
      HID_LOGICAL_MAX ( 0xff                                   ),\
      HID_REPORT_SIZE ( 8                                      ),\
      HID_REPORT_COUNT( report_size                            ),\
      HID_INPUT       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),\
      /* Output */ \
      HID_USAGE       ( 0x03                                    ),\
      HID_LOGICAL_MIN ( 0x00                                    ),\
      HID_LOGICAL_MAX ( 0xff                                    ),\
      HID_REPORT_SIZE ( 8                                       ),\
      HID_REPORT_COUNT( report_size                             ),\
      HID_OUTPUT      ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE  ),\
      /* Feature */ \
      HID_USAGE       ( 0x04                                    ),\
      HID_LOGICAL_MIN ( 0x00                                    ),\
      HID_LOGICAL_MAX ( 0xff                                    ),\
      HID_REPORT_SIZE ( 8                                       ),\
      HID_REPORT_COUNT( report_size                             ),\
      HID_FEATURE     ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE  ),\
    HID_COLLECTION_END \

#define TUD_HID_REPORT_DESC_GENERIC_INOUT_FEATURE_LEN 46

// max size is 64 and we need one byte for the report ID
static const uint8_t HID_VENDOR_REPORT_SIZE = 63;

static const uint8_t report_descriptor[] = {
    TUD_HID_REPORT_DESC_GENERIC_INOUT_FEATURE(HID_VENDOR_REPORT_SIZE, HID_REPORT_ID(HID_REPORT_ID_VENDOR))
};

USBHIDVendor::USBHIDVendor(): hid(){
	static bool initialized = false;
	if(!initialized){
		initialized = true;
		hid.addDevice(this, sizeof(report_descriptor));
        memset(feature, 0, HID_VENDOR_REPORT_SIZE);
	} else {
		isr_log_e("Only one instance of USBHIDVendor is allowed!");
		abort();
	}
}

uint16_t USBHIDVendor::_onGetDescriptor(uint8_t* dst){
    memcpy(dst, report_descriptor, sizeof(report_descriptor));
    return sizeof(report_descriptor);
}

size_t USBHIDVendor::setRxBufferSize(size_t rx_queue_len){
    if(rx_queue){
        if(!rx_queue_len){
            vQueueDelete(rx_queue);
            rx_queue = NULL;
        }
        return 0;
    }
    rx_queue = xQueueCreate(rx_queue_len, sizeof(uint8_t));
    if(!rx_queue){
        return 0;
    }
    return rx_queue_len;
}

void USBHIDVendor::begin(){
    hid.begin();
    setRxBufferSize(256);//default if not preset
}

void USBHIDVendor::end(){
    setRxBufferSize(0);
}

void USBHIDVendor::onEvent(esp_event_handler_t callback){
    onEvent(ARDUINO_USB_HID_VENDOR_ANY_EVENT, callback);
}

void USBHIDVendor::onEvent(arduino_usb_hid_vendor_event_t event, esp_event_handler_t callback){
    arduino_usb_event_handler_register_with(ARDUINO_USB_HID_VENDOR_EVENTS, event, callback, this);
}

uint16_t USBHIDVendor::_onGetFeature(uint8_t report_id, uint8_t* buffer, uint16_t len){
    log_v("len: %u", len);
    memcpy(buffer, feature, len);
    arduino_usb_hid_vendor_event_data_t p = {0};
    p.buffer = feature;
    p.len = len;
    arduino_usb_event_post(ARDUINO_USB_HID_VENDOR_EVENTS, ARDUINO_USB_HID_VENDOR_GET_FEATURE_EVENT, &p, sizeof(arduino_usb_hid_vendor_event_data_t), portMAX_DELAY);
    return len;
}

void USBHIDVendor::_onSetFeature(uint8_t report_id, const uint8_t* buffer, uint16_t len){
    log_v("len: %u", len);
    memcpy(feature, buffer, len);
    arduino_usb_hid_vendor_event_data_t p = {0};
    p.buffer = feature;
    p.len = len;
    arduino_usb_event_post(ARDUINO_USB_HID_VENDOR_EVENTS, ARDUINO_USB_HID_VENDOR_SET_FEATURE_EVENT, &p, sizeof(arduino_usb_hid_vendor_event_data_t), portMAX_DELAY);
}

void USBHIDVendor::_onOutput(uint8_t report_id, const uint8_t* buffer, uint16_t len){
    log_v("len: %u", len);
    for(uint32_t i=0; i<len; i++){
        if(rx_queue == NULL || !xQueueSend(rx_queue, buffer+i, 0)){
            len = i+1;
            log_e("RX Queue Overflow");
            break;
        }
    }
    arduino_usb_hid_vendor_event_data_t p = {0};
    p.buffer = buffer;
    p.len = len;
    arduino_usb_event_post(ARDUINO_USB_HID_VENDOR_EVENTS, ARDUINO_USB_HID_VENDOR_OUTPUT_EVENT, &p, sizeof(arduino_usb_hid_vendor_event_data_t), portMAX_DELAY);
}

size_t USBHIDVendor::write(const uint8_t* buffer, uint16_t len){
    const uint8_t * data = (const uint8_t *)buffer;
    size_t to_send = len, max_send=HID_VENDOR_REPORT_SIZE, will_send=0;
    while(to_send){
        will_send = to_send;
        if(will_send > max_send){
            will_send = max_send;
        }
        // On Mac, I can get INPUT only when data length equals the input report size
        // To be tested on other platforms
        uint8_t hid_in[HID_VENDOR_REPORT_SIZE];
        memcpy(hid_in, data, will_send);
        memset(hid_in + will_send, 0, HID_VENDOR_REPORT_SIZE - will_send);
        if(!hid.SendReport(HID_REPORT_ID_VENDOR, hid_in, HID_VENDOR_REPORT_SIZE)){
        //if(!hid.SendReport(HID_REPORT_ID_VENDOR, buffer + (len - to_send), will_send)){
            return len - to_send;
        }
        to_send -= will_send;
        data += will_send;
    }
    return len;
}

size_t USBHIDVendor::write(uint8_t c){
    return write(&c, 1);
}

int USBHIDVendor::available(void){
    if(rx_queue == NULL){
        return -1;
    }
    return uxQueueMessagesWaiting(rx_queue);
}

int USBHIDVendor::peek(void){
    if(rx_queue == NULL){
        return -1;
    }
    uint8_t c;
    if(xQueuePeek(rx_queue, &c, 0)) {
        return c;
    }
    return -1;
}

int USBHIDVendor::read(void){
    if(rx_queue == NULL){
        return -1;
    }
    uint8_t c = 0;
    if(xQueueReceive(rx_queue, &c, 0)) {
        return c;
    }
    return -1;
}

size_t USBHIDVendor::read(uint8_t *buffer, size_t size){
    if(rx_queue == NULL){
        return -1;
    }
    uint8_t c = 0;
    size_t count = 0;
    while(count < size && xQueueReceive(rx_queue, &c, 0)){
        buffer[count++] = c;
    }
    return count;
}

void USBHIDVendor::flush(void){}


#endif /* CFG_TUD_HID */
