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
#include "USBHIDGamepad.h"

#if CFG_TUD_HID

static uint16_t tinyusb_hid_device_descriptor_cb(uint8_t * dst, uint8_t report_id){
    uint8_t report_descriptor[] = {
        TUD_HID_REPORT_DESC_GAMEPAD(HID_REPORT_ID(report_id))
    };
    memcpy(dst, report_descriptor, sizeof(report_descriptor));
    return sizeof(report_descriptor);
}

USBHIDGamepad::USBHIDGamepad(): hid(), tx_sem(NULL), _x(0), _y(0), _z(0), _rz(0), _rx(0), _ry(0), _hat(0), _buttons(0){
	static bool initialized = false;
	if(!initialized){
		initialized = true;
		uint8_t report_descriptor[] = {
		    TUD_HID_REPORT_DESC_GAMEPAD(HID_REPORT_ID(0))
		};
		hid.addDevice(this, sizeof(report_descriptor), tinyusb_hid_device_descriptor_cb);
	} else {
		isr_log_e("Only one instance of USBHIDGamepad is allowed!");
		abort();
	}
}

void USBHIDGamepad::begin(){
    if(tx_sem == NULL){
        tx_sem = xSemaphoreCreateBinary();
        xSemaphoreTake(tx_sem, 0);
    }
}

void USBHIDGamepad::end(){
    if (tx_sem != NULL) {
        vSemaphoreDelete(tx_sem);
        tx_sem = NULL;
    }
}

void USBHIDGamepad::_onInputDone(const uint8_t* buffer, uint16_t len){
    //log_i("len: %u", len);
    xSemaphoreGive(tx_sem);
}

bool USBHIDGamepad::write(){
    uint32_t timeout_ms = 100;
    if(!tud_hid_n_wait_ready(0, timeout_ms)){
        log_e("not ready");
        return false;
    }
    bool res = tud_hid_n_gamepad_report(0, id, _x, _y, _z, _rz, _rx, _ry, _hat, _buttons);
    if(!res){
        log_e("report failed");
        return false;
    } else if(xSemaphoreTake(tx_sem, timeout_ms / portTICK_PERIOD_MS) != pdTRUE){
        log_e("report wait failed");
        return false;
    }
    return true;
}

bool USBHIDGamepad::leftStick(int8_t x, int8_t y){
    _x = x;
    _y = y;
    return write();
}

bool USBHIDGamepad::rightStick(int8_t z, int8_t rz){
    _z = z;
    _rz = rz;
    return write();
}

bool USBHIDGamepad::leftTrigger(int8_t rx){
    _rx = rx;
    return write();
}

bool USBHIDGamepad::rightTrigger(int8_t ry){
    _ry = ry;
    return write();
}

bool USBHIDGamepad::hat(uint8_t hat){
    if(hat > 9){
        return false;
    }
    _hat = hat;
    return write();
}

bool USBHIDGamepad::pressButton(uint8_t button){
    if(button > 31){
        return false;
    }
    _buttons |= (1 << button);
    return write();
}

bool USBHIDGamepad::releaseButton(uint8_t button){
    if(button > 31){
        return false;
    }
    _buttons &= ~(1 << button);
    return write();
}

bool USBHIDGamepad::send(int8_t x, int8_t y, int8_t z, int8_t rz, int8_t rx, int8_t ry, uint8_t hat, uint32_t buttons){
    if(hat > 9){
        return false;
    }
    _x = x;
    _y = y;
    _z = z;
    _rz = rz;
    _rx = rx;
    _ry = ry;
    _hat = hat;
    _buttons = buttons;
    return write();
}


#endif /* CFG_TUD_HID */
