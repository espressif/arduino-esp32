// Copyright 2015-2022 Espressif Systems (Shanghai) PTE LTD
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
#ifdef CONFIG_ESP_INSIGHTS_ENABLED
#include "Arduino.h"
#include "esp_system.h"
#include "esp_insights.h"
class InsightsClass
{
    private:
        esp_insights_config_t config = {.log_type = 0, .node_id = NULL, .auth_key = NULL, .alloc_ext_ram = false};
    public:
        esp_err_t init(const char *auth_key, uint32_t log_type, const char *node_id = NULL, bool alloc_ext_ram = false);
        esp_err_t enable(const char *auth_key, uint32_t log_type, const char *node_id = NULL, bool alloc_ext_ram = false);
        esp_err_t registerTransport(esp_insights_transport_config_t *config);
        esp_err_t sendData();
        void deinit();
        void disable();
        void unregisterTransport();
};

extern InsightsClass Insights;
#endif