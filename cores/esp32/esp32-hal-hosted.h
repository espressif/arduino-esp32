// Copyright 2015-2025 Espressif Systems (Shanghai) PTE LTD
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

#ifndef MAIN_ESP32_HAL_HOSTED_H_
#define MAIN_ESP32_HAL_HOSTED_H_

#include "sdkconfig.h"
#if defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE) || defined(CONFIG_ESP_WIFI_REMOTE_ENABLED)

#include "stdint.h"
#include "stdbool.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint8_t pin_clk;
  uint8_t pin_cmd;
  uint8_t pin_d0;
  uint8_t pin_d1;
  uint8_t pin_d2;
  uint8_t pin_d3;
  uint8_t pin_reset;
} sdio_pin_config_t;

bool hostedInitBLE();
bool hostedInitWiFi();
bool hostedDeinitBLE();
bool hostedDeinitWiFi();
bool hostedIsInitialized();
bool hostedIsBLEActive();
bool hostedIsWiFiActive();
bool hostedSetPins(int8_t clk, int8_t cmd, int8_t d0, int8_t d1, int8_t d2, int8_t d3, int8_t rst);
void hostedGetPins(int8_t *clk, int8_t *cmd, int8_t *d0, int8_t *d1, int8_t *d2, int8_t *d3, int8_t *rst);
void hostedGetHostVersion(uint32_t *major, uint32_t *minor, uint32_t *patch);
void hostedGetSlaveVersion(uint32_t *major, uint32_t *minor, uint32_t *patch);
bool hostedHasUpdate();
char *hostedGetUpdateURL();
bool hostedBeginUpdate();
bool hostedWriteUpdate(uint8_t *buf, uint32_t len);
bool hostedEndUpdate();
bool hostedActivateUpdate();

#ifdef __cplusplus
}
#endif

#endif /* defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE) || defined(CONFIG_ESP_WIFI_REMOTE_ENABLED) */
#endif /* MAIN_ESP32_HAL_HOSTED_H_ */
