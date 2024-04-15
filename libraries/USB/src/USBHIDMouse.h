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

#include "soc/soc_caps.h"
#if SOC_USB_OTG_SUPPORTED

#include "USBHID.h"
#if CONFIG_TINYUSB_HID_ENABLED

#define MOUSE_LEFT 0x01
#define MOUSE_RIGHT 0x02
#define MOUSE_MIDDLE 0x04
#define MOUSE_BACKWARD 0x08
#define MOUSE_FORWARD 0x10
#define MOUSE_ALL 0x1F

#include "./tusb_hid_mouse.h"

enum MousePositioning_t {
  HID_MOUSE_RELATIVE,
  HID_MOUSE_ABSOLUTE
};

struct HIDMouseType_t {
  MousePositioning_t positioning;
  const uint8_t* report_descriptor;
  size_t descriptor_size;
  size_t report_size;
};

extern HIDMouseType_t HIDMouseRel;
extern HIDMouseType_t HIDMouseAbs;


class USBHIDMouseBase : public USBHIDDevice {
public:
  USBHIDMouseBase(HIDMouseType_t* type);
  void begin(void);
  void end(void);
  void press(uint8_t b = MOUSE_LEFT);      // press LEFT by default
  void release(uint8_t b = MOUSE_LEFT);    // release LEFT by default
  bool isPressed(uint8_t b = MOUSE_LEFT);  // check LEFT by default
  template<typename T> bool sendReport(T report) {
    return hid.SendReport(HID_REPORT_ID_MOUSE, &report, _type->report_size);
  };
  // internal use
  uint16_t _onGetDescriptor(uint8_t* buffer);
  virtual void click(uint8_t b) = 0;
  virtual void buttons(uint8_t b) = 0;
protected:
  USBHID hid;
  uint8_t _buttons;
  HIDMouseType_t* _type;
};


class USBHIDRelativeMouse : public USBHIDMouseBase {
public:
  USBHIDRelativeMouse(void)
    : USBHIDMouseBase(&HIDMouseRel) {}
  void move(int8_t x, int8_t y, int8_t wheel = 0, int8_t pan = 0);
  void click(uint8_t b = MOUSE_LEFT) override;
  void buttons(uint8_t b) override;
};


class USBHIDAbsoluteMouse : public USBHIDMouseBase {
public:
  USBHIDAbsoluteMouse(void)
    : USBHIDMouseBase(&HIDMouseAbs) {}
  void move(int16_t x, int16_t y, int8_t wheel = 0, int8_t pan = 0);
  void click(uint8_t b = MOUSE_LEFT) override;
  void buttons(uint8_t b) override;
private:
  int16_t _lastx = 0;
  int16_t _lasty = 0;
};


// don't break examples and old sketches
typedef USBHIDRelativeMouse USBHIDMouse;

#endif /* CONFIG_TINYUSB_HID_ENABLED */
#endif /* SOC_USB_OTG_SUPPORTED */
