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

#include "USBHIDGamepad.h"

static const uint8_t report_descriptor[] = {
    TUD_HID_REPORT_DESC_GAMEPAD(HID_REPORT_ID(HID_REPORT_ID_GAMEPAD))
};

USBHIDGamepad::USBHIDGamepad(): hid(), _x(0), _y(0), _z(0), _rz(0), _rx(0), _ry(0), _hat(0), _buttons(0){
    static bool initialized = false;
    if(!initialized){
        initialized = true;
        hid.addDevice(this, sizeof(report_descriptor));
    }
}

uint16_t USBHIDGamepad::_onGetDescriptor(uint8_t* dst){
    memcpy(dst, report_descriptor, sizeof(report_descriptor));
    return sizeof(report_descriptor);
}

void USBHIDGamepad::begin(){
    hid.begin();
}

void USBHIDGamepad::end(){

}

bool USBHIDGamepad::write(){
    hid_gamepad_report_t report = {
        .x       = _x,
        .y       = _y,
        .z       = _z,
        .rz      = _rz,
        .rx      = _rx,
        .ry      = _ry,
        .hat     = _hat,
        .buttons = _buttons
    };
    return hid.SendReport(HID_REPORT_ID_GAMEPAD, &report, sizeof(report));
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


#endif /* CONFIG_TINYUSB_HID_ENABLED */
