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
#include "RMakerType.h"

class Param
{
    private:
        const param_handle_t *param_handle;
       
    public:
        Param()
        {
            param_handle = NULL;
        }
        Param(const char *param_name, const char *param_type, param_val_t val, uint8_t properties)
        {
            param_handle = esp_rmaker_param_create(param_name, param_type, val, properties);
        }
        void setParamHandle(const param_handle_t *param_handle) 
        {
            this->param_handle = param_handle;
        }
        const char *getParamName()
        {
            return esp_rmaker_param_get_name(param_handle);
        }
        const param_handle_t *getParamHandle()
        {
            return param_handle;
        } 
         
        esp_err_t addUIType(const char *ui_type);
        esp_err_t addBounds(param_val_t min, param_val_t max, param_val_t step);
        esp_err_t updateAndReport(param_val_t val);
};
#endif