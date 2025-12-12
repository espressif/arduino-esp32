// Copyright 2025 Espressif Systems (Shanghai) PTE LTD
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

#include <Arduino.h>
#include <esp_matter.h>
#include <functional>

using namespace esp_matter;

// Matter Endpoint Base Class. Controls the endpoint ID and allows the child class to overwrite attribute change call
class MatterEndPoint {
public:
  enum attrOperation_t {
    ATTR_SET = false,
    ATTR_UPDATE = true
  };

  using EndPointIdentifyCB = std::function<bool(bool)>;

  // this function is called by Matter internal event processor. It could be overwritten by the application, if necessary.
  virtual bool attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) = 0;

  // This function is called to create a secondary network interface endpoint.
  // It can be used for devices that support multiple network interfaces,
  // such as Ethernet, Thread and Wi-Fi.
  bool createSecondaryNetworkInterface();

  // This function is called to get the secondary network interface endpoint ID.
  uint16_t getSecondaryNetworkEndPointId();

  // This function is called to get the current Matter Accessory endpoint ID.
  uint16_t getEndPointId();

  // This function is called to set the current Matter Accessory endpoint ID.
  void setEndPointId(uint16_t ep);

  // helper functions for attribute manipulation
  esp_matter::attribute_t *getAttribute(uint32_t cluster_id, uint32_t attribute_id);

  // get the value of an attribute from its cluster id and
  bool getAttributeVal(uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *attrVal);

  // set the value of an attribute from its cluster id and
  bool setAttributeVal(uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *attrVal);

  // update the value of an attribute from its cluster id
  bool updateAttributeVal(uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *attrVal);

  // This callback is invoked when clients interact with the Identify Cluster of an specific endpoint.
  bool endpointIdentifyCB(uint16_t endpoint_id, bool identifyIsEnabled);

  // User callback for the Identify Cluster functionality
  void onIdentify(EndPointIdentifyCB onEndPointIdentifyCB);

protected:
  // used for secondary network interface endpoints
  static uint16_t secondary_network_endpoint_id;
  // main endpoint ID
  uint16_t endpoint_id = 0;
  EndPointIdentifyCB _onEndPointIdentifyCB = nullptr;
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
