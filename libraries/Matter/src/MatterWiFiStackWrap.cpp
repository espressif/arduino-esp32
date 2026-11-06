// Copyright 2026 Espressif Systems (Shanghai) PTE LTD
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

#include <sdkconfig.h>
#ifdef CONFIG_ESP_MATTER_ENABLE_DATA_MODEL

#include <Matter.h>

#if defined(CONFIG_ENABLE_WIFI_STATION) && CONFIG_ENABLE_WIFI_STATION

#include <esp_netif.h>
#include <lib/core/CHIPError.h>

static bool sWrapperRan = false;

extern "C" CHIP_ERROR __real__ZN4chip11DeviceLayer8Internal10ESP32Utils13InitWiFiStackEv(void);

extern "C" CHIP_ERROR __wrap__ZN4chip11DeviceLayer8Internal10ESP32Utils13InitWiFiStackEv(void) {
  sWrapperRan = true;
  const matterNetwork_t selected = ArduinoMatter::getSelectedNetwork();
  if (selected == MATTER_NETWORK_THREAD || selected == MATTER_NETWORK_ETHERNET) {
    if (esp_netif_init() != ESP_OK) {
      return CHIP_ERROR_INTERNAL;
    }
    return CHIP_NO_ERROR;
  }
  return __real__ZN4chip11DeviceLayer8Internal10ESP32Utils13InitWiFiStackEv();
}

bool matterWifiStackWrapRan() {
  return sWrapperRan;
}

#else

bool matterWifiStackWrapRan() {
  return false;
}

#endif  // CONFIG_ENABLE_WIFI_STATION
#endif  // CONFIG_ESP_MATTER_ENABLE_DATA_MODEL
