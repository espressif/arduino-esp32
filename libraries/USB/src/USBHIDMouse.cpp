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
#if SOC_USB_OTG_SUPPORTED

#if CONFIG_TINYUSB_HID_ENABLED

#include "USBHIDMouse.h"

USBHIDMouseBase::USBHIDMouseBase(HIDMouseType_t *type) : hid(), _buttons(0), _type(type) {
  static bool initialized = false;
  if (!initialized) {
    initialized = true;
    hid.addDevice(this, _type->descriptor_size);
  }
};

uint16_t USBHIDMouseBase::_onGetDescriptor(uint8_t *dst) {
  memcpy(dst, _type->report_descriptor, _type->descriptor_size);
  return _type->descriptor_size;
}

void USBHIDMouseBase::buttons(uint8_t b) {
  if (b != _buttons) {
    _buttons = b;
  }
}

void USBHIDMouseBase::begin() {
  hid.begin();
}

void USBHIDMouseBase::end() {}

void USBHIDMouseBase::press(uint8_t b) {
  this->buttons(_buttons | b);
}

void USBHIDMouseBase::release(uint8_t b) {
  this->buttons(_buttons & ~b);
}

bool USBHIDMouseBase::isPressed(uint8_t b) {
  if ((b & _buttons) > 0) {
    return true;
  }
  return false;
}

static const uint8_t abs_mouse_report_descriptor[] = {TUD_HID_REPORT_DESC_ABSMOUSE(HID_REPORT_ID(HID_REPORT_ID_MOUSE))};

HIDMouseType_t HIDMouseAbs = {HID_MOUSE_ABSOLUTE, abs_mouse_report_descriptor, sizeof(abs_mouse_report_descriptor), sizeof(hid_abs_mouse_report_t)};

void USBHIDAbsoluteMouse::move(int16_t x, int16_t y, int8_t wheel, int8_t pan) {
  hid_abs_mouse_report_t report;
  report.buttons = _buttons;
  report.x = _lastx = x;
  report.y = _lasty = y;
  report.wheel = wheel;
  report.pan = pan;
  sendReport(report);
}

void USBHIDAbsoluteMouse::click(uint8_t b) {
  _buttons = b;
  move(_lastx, _lasty);
  _buttons = 0;
  move(_lastx, _lasty);
}

void USBHIDAbsoluteMouse::buttons(uint8_t b) {
  if (b != _buttons) {
    _buttons = b;
    move(_lastx, _lasty);
  }
}

static const uint8_t rel_mouse_report_descriptor[] = {TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(HID_REPORT_ID_MOUSE))};

HIDMouseType_t HIDMouseRel = {HID_MOUSE_RELATIVE, rel_mouse_report_descriptor, sizeof(rel_mouse_report_descriptor), sizeof(hid_mouse_report_t)};

void USBHIDRelativeMouse::move(int8_t x, int8_t y, int8_t wheel, int8_t pan) {
  hid_mouse_report_t report = {.buttons = _buttons, .x = x, .y = y, .wheel = wheel, .pan = pan};
  sendReport(report);
}

void USBHIDRelativeMouse::click(uint8_t b) {
  _buttons = b;
  move(0, 0);
  _buttons = 0;
  move(0, 0);
}

void USBHIDRelativeMouse::buttons(uint8_t b) {
  if (b != _buttons) {
    _buttons = b;
    move(0, 0);
  }
}

#endif /* CONFIG_TINYUSB_HID_ENABLED */
#endif /* SOC_USB_OTG_SUPPORTED */
