/*
  Mouse.h

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

#pragma once
#include "USBHID.h"
#if CONFIG_TINYUSB_HID_ENABLED

#define MOUSE_LEFT      0x01
#define MOUSE_RIGHT     0x02
#define MOUSE_MIDDLE    0x04
#define MOUSE_BACKWARD  0x08
#define MOUSE_FORWARD   0x10
#define MOUSE_ALL       0x1F

class USBHIDMouse: public USBHIDDevice {
private:
    USBHID hid;
    uint8_t _buttons;
    void buttons(uint8_t b);
    bool write(int8_t x, int8_t y, int8_t vertical, int8_t horizontal);
public:
    USBHIDMouse(void);
    void begin(void);
    void end(void);

    void click(uint8_t b = MOUSE_LEFT);
    void move(int8_t x, int8_t y, int8_t wheel = 0, int8_t pan = 0); 
    void press(uint8_t b = MOUSE_LEFT);   // press LEFT by default
    void release(uint8_t b = MOUSE_LEFT); // release LEFT by default
    bool isPressed(uint8_t b = MOUSE_LEFT); // check LEFT by default

    // internal use
    uint16_t _onGetDescriptor(uint8_t* buffer);
};

#endif
