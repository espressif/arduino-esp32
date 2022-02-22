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


// Absolute Mouse

// We use a custom HID mouse report descriptor with absolute positioning
// where coordinates are expressed as an unsigned value between 0 and 32767
static const uint8_t abs_mouse_report_descriptor[] = {
  0x05, 0x01,                // USAGE_PAGE (Generic Desktop)
  0x09, 0x02,                // USAGE (Mouse)
  0xa1, 0x01,                // COLLECTION (Application)
  0x09, 0x01,                //   USAGE (Pointer)
  0xA1, 0x00,                //   COLLECTION (Physical)
  0x85, HID_REPORT_ID_MOUSE, //     REPORT_ID (1)
  0x05, 0x09,                //     USAGE_PAGE (Button)
  0x19, 0x01,                //     USAGE_MINIMUM (1)
  0x29, 0x03,                //     USAGE_MAXIMUM (3)
  0x15, 0x00,                //     LOGICAL_MINIMUM (0)
  0x25, 0x01,                //     LOGICAL_MAXIMUM (1)
  0x95, 0x03,                //     REPORT_COUNT (3)
  0x75, 0x01,                //     REPORT_SIZE (1)
  0x81, 0x02,                //     INPUT (Data,Var,Abs)
  0x95, 0x01,                //     REPORT_COUNT (1)
  0x75, 0x05,                //     REPORT_SIZE (5)
  0x81, 0x03,                //     INPUT (Const,Var,Abs)
  0x05, 0x01,                //     USAGE_PAGE (Generic Desktop)
  0x09, 0x30,                //     USAGE (X)
  0x09, 0x31,                //     USAGE (Y)
  0x16, 0x00, 0x00,          //     LOGICAL_MINIMUM(0)
  0x26, 0xff, 0x7f,          //     LOGICAL_MAXIMUM(32767)
  0x75, 0x10,                //     REPORT_SIZE (16)
  0x95, 0x02,                //     REPORT_COUNT (2)
  0x81, 0x02,                //     INPUT (Data,Var,Abs)
  0xC0,                      //   END_COLLECTION
  0xC0,                      // END COLLECTION
};



USBHIDAbsMouse::USBHIDAbsMouse(): hid() {
    static bool initialized = false;
    if(!initialized){
        initialized = true;
        hid.addDevice(this, sizeof(abs_mouse_report_descriptor));
    }
}

void USBHIDAbsMouse::begin(void)
{
    hid.begin();
}

uint16_t USBHIDAbsMouse::_onGetDescriptor(uint8_t* buffer)
{
    memcpy(buffer, abs_mouse_report_descriptor, sizeof(abs_mouse_report_descriptor));
    return sizeof(abs_mouse_report_descriptor);
}

bool USBHIDAbsMouse::sendReport(abs_mouse_report_t * report)
{
    return hid.SendReport( HID_REPORT_ID_MOUSE, report, sizeof(abs_mouse_report_t) );
}

void USBHIDAbsMouse::end()
{
    hid.end();
}




#endif /* CONFIG_TINYUSB_HID_ENABLED */
