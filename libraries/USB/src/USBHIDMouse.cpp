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

#include "esp32-hal.h"
#include "esp32-hal-tinyusb.h"
#include "USBHIDMouse.h"

#if CFG_TUD_HID

static uint16_t tinyusb_hid_device_descriptor_cb(uint8_t * dst, uint8_t report_id){
    uint8_t report_descriptor[] = {
        TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(report_id))
    };
    memcpy(dst, report_descriptor, sizeof(report_descriptor));
    return sizeof(report_descriptor);
}

USBHIDMouse::USBHIDMouse(): hid(), _buttons(0){
	static bool initialized = false;
	if(!initialized){
		initialized = true;
		uint8_t report_descriptor[] = {
		    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(0))
		};
		hid.addDevice(this, sizeof(report_descriptor), tinyusb_hid_device_descriptor_cb);
	} else {
		isr_log_e("Only one instance of USBHIDMouse is allowed!");
		abort();
	}
}

void USBHIDMouse::begin(){
    hid.begin();
}

void USBHIDMouse::end(){
}

void USBHIDMouse::click(uint8_t b){
    _buttons = b;
    move(0,0);
    _buttons = 0;
    move(0,0);
}

void USBHIDMouse::move(int8_t x, int8_t y, int8_t wheel, int8_t pan){
    hid_mouse_report_t report = {
        .buttons = _buttons,
        .x       = x,
        .y       = y,
        .wheel   = wheel,
        .pan     = pan
    };
    hid.SendReport(id, &report, sizeof(report));
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


#endif /* CFG_TUD_HID */
