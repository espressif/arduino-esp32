// Copyright 2024 Espressif Systems (Shanghai) PTE LTD
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

#pragma once
#include <sdkconfig.h>
#ifdef CONFIG_ESP_MATTER_ENABLE_DATA_MODEL

#include <Matter.h>
#include <functional>

// Matter Endpoint Base Class. Controls the endpoint ID and allows the child class to overwrite attribute change call
class MatterEndPoint {
public:
  uint16_t getEndPointId() {
    return endpoint_id;
  }
  void setEndPointId(uint16_t ep) {
    endpoint_id = ep;
  }
  // this function is called by Matter internal event processor. It could be overwritten by the application, if necessary.
  virtual bool attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) = 0;

protected:
  uint16_t endpoint_id = 0;
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
