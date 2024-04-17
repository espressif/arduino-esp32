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

#pragma once

#include "soc/soc_caps.h"
#if SOC_USB_OTG_SUPPORTED

#include "USBHID.h"
#if CONFIG_TINYUSB_HID_ENABLED

/// Standard Gamepad Buttons Naming from Linux input event codes
/// https://github.com/torvalds/linux/blob/master/include/uapi/linux/input-event-codes.h
#define BUTTON_A 0
#define BUTTON_B 1
#define BUTTON_C 2
#define BUTTON_X 3
#define BUTTON_Y 4
#define BUTTON_Z 5
#define BUTTON_TL 6
#define BUTTON_TR 7
#define BUTTON_TL2 8
#define BUTTON_TR2 9
#define BUTTON_SELECT 10
#define BUTTON_START 11
#define BUTTON_MODE 12
#define BUTTON_THUMBL 13
#define BUTTON_THUMBR 14

#define BUTTON_SOUTH BUTTON_A
#define BUTTON_EAST BUTTON_B
#define BUTTON_NORTH BUTTON_X
#define BUTTON_WEST BUTTON_Y

/// Standard Gamepad HAT/DPAD Buttons (from Linux input event codes)
#define HAT_CENTER 0
#define HAT_UP 1
#define HAT_UP_RIGHT 2
#define HAT_RIGHT 3
#define HAT_DOWN_RIGHT 4
#define HAT_DOWN 5
#define HAT_DOWN_LEFT 6
#define HAT_LEFT 7
#define HAT_UP_LEFT 8

class USBHIDGamepad : public USBHIDDevice {
private:
  USBHID hid;
  int8_t _x;          ///< Delta x  movement of left analog-stick
  int8_t _y;          ///< Delta y  movement of left analog-stick
  int8_t _z;          ///< Delta z  movement of right analog-joystick
  int8_t _rz;         ///< Delta Rz movement of right analog-joystick
  int8_t _rx;         ///< Delta Rx movement of analog left trigger
  int8_t _ry;         ///< Delta Ry movement of analog right trigger
  uint8_t _hat;       ///< Buttons mask for currently pressed buttons in the DPad/hat
  uint32_t _buttons;  ///< Buttons mask for currently pressed buttons
  bool write();
public:
  USBHIDGamepad(void);
  void begin(void);
  void end(void);

  bool leftStick(int8_t x, int8_t y);
  bool rightStick(int8_t z, int8_t rz);

  bool leftTrigger(int8_t rx);
  bool rightTrigger(int8_t ry);

  bool hat(uint8_t hat);

  bool pressButton(uint8_t button);
  bool releaseButton(uint8_t button);

  bool send(int8_t x, int8_t y, int8_t z, int8_t rz, int8_t rx, int8_t ry, uint8_t hat, uint32_t buttons);

  // internal use
  uint16_t _onGetDescriptor(uint8_t* buffer);
};

#endif /* CONFIG_TINYUSB_HID_ENABLED */
#endif /* SOC_USB_OTG_SUPPORTED */
