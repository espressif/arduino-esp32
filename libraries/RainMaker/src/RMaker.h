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
#include "sdkconfig.h"
#ifdef CONFIG_ESP_RMAKER_WORK_QUEUE_TASK_STACK
#include "esp_system.h"
#include "Arduino.h"
#include "RMakerNode.h"
#include "RMakerQR.h"
#include "RMakerUtils.h"
#include <esp_rmaker_standard_types.h>

class RMakerClass {
private:
  esp_rmaker_config_t rainmaker_cfg = {false};

public:
  void setTimeSync(bool val);
  Node initNode(const char *name, const char *type = "ESP RainMaker with Arduino");
  esp_err_t deinitNode(Node node);
  esp_err_t setTimeZone(const char *tz = "Asia/Shanghai");
  esp_err_t enableSchedule();
  esp_err_t enableTZService();
  esp_err_t enableOTA(ota_type_t type, const char *cert = ESP_RMAKER_OTA_DEFAULT_SERVER_CERT);
  esp_err_t enableScenes();
  esp_err_t enableSystemService(uint16_t flags, int8_t reboot_seconds = 2, int8_t reset_seconds = 2, int8_t reset_reboot_seconds = 2);
  esp_err_t start();
  esp_err_t stop();
};

extern RMakerClass RMaker;
#endif
