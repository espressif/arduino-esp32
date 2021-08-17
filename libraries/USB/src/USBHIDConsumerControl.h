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

// Power Control
#define CONSUMER_CONTROL_POWER                             0x0030
#define CONSUMER_CONTROL_RESET                             0x0031
#define CONSUMER_CONTROL_SLEEP                             0x0032

// Screen Brightness
#define CONSUMER_CONTROL_BRIGHTNESS_INCREMENT              0x006F
#define CONSUMER_CONTROL_BRIGHTNESS_DECREMENT              0x0070

// These HID usages operate only on mobile systems (battery powered) and
// require Windows 8 (build 8302 or greater).
#define CONSUMER_CONTROL_WIRELESS_RADIO_CONTROLS           0x000C
#define CONSUMER_CONTROL_WIRELESS_RADIO_BUTTONS            0x00C6
#define CONSUMER_CONTROL_WIRELESS_RADIO_LED                0x00C7
#define CONSUMER_CONTROL_WIRELESS_RADIO_SLIDER_SWITCH      0x00C8

// Media Control
#define CONSUMER_CONTROL_PLAY_PAUSE                        0x00CD
#define CONSUMER_CONTROL_SCAN_NEXT                         0x00B5
#define CONSUMER_CONTROL_SCAN_PREVIOUS                     0x00B6
#define CONSUMER_CONTROL_STOP                              0x00B7
#define CONSUMER_CONTROL_VOLUME                            0x00E0
#define CONSUMER_CONTROL_MUTE                              0x00E2
#define CONSUMER_CONTROL_BASS                              0x00E3
#define CONSUMER_CONTROL_TREBLE                            0x00E4
#define CONSUMER_CONTROL_BASS_BOOST                        0x00E5
#define CONSUMER_CONTROL_VOLUME_INCREMENT                  0x00E9
#define CONSUMER_CONTROL_VOLUME_DECREMENT                  0x00EA
#define CONSUMER_CONTROL_BASS_INCREMENT                    0x0152
#define CONSUMER_CONTROL_BASS_DECREMENT                    0x0153
#define CONSUMER_CONTROL_TREBLE_INCREMENT                  0x0154
#define CONSUMER_CONTROL_TREBLE_DECREMENT                  0x0155

// Application Launcher
#define CONSUMER_CONTROL_CONFIGURATION                     0x0183
#define CONSUMER_CONTROL_EMAIL_READER                      0x018A
#define CONSUMER_CONTROL_CALCULATOR                        0x0192
#define CONSUMER_CONTROL_LOCAL_BROWSER                     0x0194

// Browser/Explorer Specific
#define CONSUMER_CONTROL_SEARCH                            0x0221
#define CONSUMER_CONTROL_HOME                              0x0223
#define CONSUMER_CONTROL_BACK                              0x0224
#define CONSUMER_CONTROL_FORWARD                           0x0225
#define CONSUMER_CONTROL_BR_STOP                           0x0226
#define CONSUMER_CONTROL_REFRESH                           0x0227
#define CONSUMER_CONTROL_BOOKMARKS                         0x022A

// Mouse Horizontal scroll
#define CONSUMER_CONTROL_PAN                               0x0238

class USBHIDConsumerControl: public USBHIDDevice {
private:
    USBHID hid;
    bool send(uint16_t value);
public:
    USBHIDConsumerControl(void);
    void begin(void);
    void end(void);
    size_t press(uint16_t k);
    size_t release();

    // internal use
    uint16_t _onGetDescriptor(uint8_t* buffer);
};

#endif
