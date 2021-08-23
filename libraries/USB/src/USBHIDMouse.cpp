/*
  Mouse.cpp

  Copyright (c) 2015, Arduino LLC
  Original code (pre-library): Copyright (c) 2011, Peter Barrett

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "USBHID.h"

#if CONFIG_TINYUSB_HID_ENABLED

#include "USBHIDMouse.h"

static const uint8_t report_descriptor[] = {
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(HID_REPORT_ID_MOUSE))
};

USBHIDMouse::USBHIDMouse(): hid(), _buttons(0){
    static bool initialized = false;
    if(!initialized){
        initialized = true;
        hid.addDevice(this, sizeof(report_descriptor));
    }
}

uint16_t USBHIDMouse::_onGetDescriptor(uint8_t* dst){
    memcpy(dst, report_descriptor, sizeof(report_descriptor));
    return sizeof(report_descriptor);
}

void USBHIDMouse::begin(){
    hid.begin();
}

void USBHIDMouse::end(){
}

void USBHIDMouse::move(int8_t x, int8_t y, int8_t wheel, int8_t pan){
    hid_mouse_report_t report = {
        .buttons = _buttons,
        .x       = x,
        .y       = y,
        .wheel   = wheel,
        .pan     = pan
    };
    hid.SendReport(HID_REPORT_ID_MOUSE, &report, sizeof(report));
}

void USBHIDMouse::click(uint8_t b){
    _buttons = b;
    move(0,0);
    _buttons = 0;
    move(0,0);
}

void USBHIDMouse::buttons(uint8_t b){
    if (b != _buttons){
        _buttons = b;
        move(0,0);
    }
}

void USBHIDMouse::press(uint8_t b){
    buttons(_buttons | b);
}

void USBHIDMouse::release(uint8_t b){
    buttons(_buttons & ~b);
}

bool USBHIDMouse::isPressed(uint8_t b){
    if ((b & _buttons) > 0) {
        return true;
    }
    return false;
}


#endif /* CONFIG_TINYUSB_HID_ENABLED */
