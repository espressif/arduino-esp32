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
#include "USBHID.h"
#if CONFIG_TINYUSB_HID_ENABLED

#define SYSTEM_CONTROL_NONE         0
#define SYSTEM_CONTROL_POWER_OFF    1
#define SYSTEM_CONTROL_STANDBY      2
#define SYSTEM_CONTROL_WAKE_HOST    3

class USBHIDSystemControl: public USBHIDDevice {
private:
    USBHID hid;
    bool send(uint8_t value);
public:
    USBHIDSystemControl(void);
    void begin(void);
    void end(void);
    size_t press(uint8_t k);
    size_t release();

    // internal use
    uint16_t _onGetDescriptor(uint8_t* buffer);
};

#endif
