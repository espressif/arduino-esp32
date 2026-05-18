// Copyright 2015-2026 Espressif Systems (Shanghai) PTE LTD
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

#ifndef ESP32_HAL_ALLOC_BLE_MEM_H
#define ESP32_HAL_ALLOC_BLE_MEM_H

#include "soc/soc_caps.h"
#include "sdkconfig.h"

#if defined(CONFIG_BT_CONTROLLER_ENABLED) && defined(CONFIG_SOC_BLE_SUPPORTED)

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Flag defined in esp32-hal-bt.c, set by constructors when BLE libraries are linked
extern bool _bleLibraryInUse;

// Constructor runs before app_main(), setting the flag if any BLE library is used.
// Multiple libraries including this header just set the same flag to true.
__attribute__((constructor)) static void _setBleLibraryInUse(void) {
  _bleLibraryInUse = true;
}

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_BT_CONTROLLER_ENABLED && CONFIG_SOC_BLE_SUPPORTED */

#endif /* ESP32_HAL_ALLOC_BLE_MEM_H */
