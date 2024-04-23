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
#include <esp_rmaker_core.h>
#include <esp_rmaker_ota.h>
#include <esp_err.h>
#include <esp32-hal.h>

typedef esp_rmaker_node_t *node_t;
typedef esp_rmaker_node_info_t node_info_t;
typedef esp_rmaker_param_val_t param_val_t;
typedef esp_rmaker_write_ctx_t write_ctx_t;
typedef esp_rmaker_read_ctx_t read_ctx_t;
typedef esp_rmaker_device_t device_handle_t;
typedef esp_rmaker_param_t param_handle_t;
typedef esp_rmaker_ota_type_t ota_type_t;

param_val_t value(int);
param_val_t value(bool);
param_val_t value(char *);
param_val_t value(float);
param_val_t value(const char *);
#endif
