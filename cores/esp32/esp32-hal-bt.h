// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _ESP32_ESP32_HAL_BT_H_
#define _ESP32_ESP32_HAL_BT_H_

#include "soc/soc_caps.h"
#if SOC_BT_SUPPORTED

#include "esp32-hal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  BT_MODE_DEFAULT,
  BT_MODE_BLE,
  BT_MODE_CLASSIC_BT,
  BT_MODE_BTDM
} bt_mode;

bool btStarted();
bool btStart();
bool btStartMode(bt_mode mode);
bool btStop();

#ifdef __cplusplus
}
#endif

#endif /* SOC_BT_SUPPORTED */

#endif /* _ESP32_ESP32_HAL_BT_H_ */
