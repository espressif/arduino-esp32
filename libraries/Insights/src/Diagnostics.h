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
#include "esp_diagnostics.h"
class DiagnosticsClass
{
    public:
        esp_err_t initLogHook(esp_diag_log_write_cb_t write_cb, void *cb_arg = NULL);
        void enableLogHook(uint32_t type);
        void disableLogHook(uint32_t type);
};

extern DiagnosticsClass Diagnostics;
#endif