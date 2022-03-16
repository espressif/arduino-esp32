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
#include "RMakerDevice.h"

class Node
{
    private:
        esp_rmaker_node_t *node;
    
    public:
        Node()
        {
            node = NULL;
        }
        void setNodeHandle(esp_rmaker_node_t *rnode)
        {
            node = rnode;
        }
        esp_rmaker_node_t *getNodeHandle()
        {
            return node;
        }
    
        esp_err_t addDevice(Device device);
        esp_err_t removeDevice(Device device);

        char *getNodeID();
        node_info_t *getNodeInfo();
        esp_err_t addNodeAttr(const char *attr_name, const char *val);
};
#endif