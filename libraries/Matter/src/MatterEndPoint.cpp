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

#include <sdkconfig.h>
#ifdef CONFIG_ESP_MATTER_ENABLE_DATA_MODEL

#include <MatterEndPoint.h>

uint16_t MatterEndPoint::secondary_network_endpoint_id = 0;

// This function is called to create a secondary network interface endpoint.
// It can be used for devices that support multiple network interfaces,
// such as Ethernet, Thread and Wi-Fi.
bool MatterEndPoint::createSecondaryNetworkInterface() {
  if (secondary_network_endpoint_id != 0) {
    log_v("Secondary network interface endpoint already exists with ID %d", secondary_network_endpoint_id);
    return false;
  }

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD && CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION
  // Create a secondary network interface endpoint
  endpoint::secondary_network_interface::config_t secondary_network_interface_config;
  secondary_network_interface_config.network_commissioning.feature_map = chip::to_underlying(
    //chip::app::Clusters::NetworkCommissioning::Feature::kWiFiNetworkInterface) |
    chip::app::Clusters::NetworkCommissioning::Feature::kThreadNetworkInterface
  );
  endpoint_t *endpoint = endpoint::secondary_network_interface::create(node::get(), &secondary_network_interface_config, ENDPOINT_FLAG_NONE, nullptr);
  if (endpoint == nullptr) {
    log_e("Failed to create secondary network interface endpoint");
    return false;
  }
  secondary_network_endpoint_id = endpoint::get_id(endpoint);
  log_i("Secondary Network Interface created with endpoint_id %d", secondary_network_endpoint_id);
#else
  log_i("Secondary Network Interface not supported");
  return false;
#endif

  return true;
}

uint16_t MatterEndPoint::getSecondaryNetworkEndPointId() {
  return secondary_network_endpoint_id;
}

uint16_t MatterEndPoint::getEndPointId() {
  return endpoint_id;
}

void MatterEndPoint::setEndPointId(uint16_t ep) {
  if (ep == 0) {
    log_e("Invalid endpoint ID");
    return;
  }
  log_v("Endpoint ID set to %d", ep);

  endpoint_id = ep;
}

// helper functions for attribute manipulation
esp_matter::attribute_t *MatterEndPoint::getAttribute(uint32_t cluster_id, uint32_t attribute_id) {
  if (endpoint_id == 0) {
    log_e("Endpoint ID is not set");
    return nullptr;
  }
  endpoint_t *endpoint = endpoint::get(node::get(), endpoint_id);
  if (endpoint == nullptr) {
    log_e("Endpoint [%d] not found", endpoint_id);
    return nullptr;
  }
  cluster_t *cluster = cluster::get(endpoint, cluster_id);
  if (cluster == nullptr) {
    log_e("Cluster [%d] not found", cluster_id);
    return nullptr;
  }
  esp_matter::attribute_t *attribute = attribute::get(cluster, attribute_id);
  if (attribute == nullptr) {
    log_e("Attribute [%d] not found", attribute_id);
    return nullptr;
  }
  return attribute;
}

// get the value of an attribute from its cluster id and attribute it
bool MatterEndPoint::getAttributeVal(uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *attrVal) {
  esp_matter::attribute_t *attribute = getAttribute(cluster_id, attribute_id);
  if (attribute == nullptr) {
    return false;
  }
  if (attribute::get_val(attribute, attrVal) == ESP_OK) {
    log_v("GET_VAL Success for cluster %d, attribute %d with value %d", cluster_id, attribute_id, attrVal->val.u32);
    return true;
  }
  log_e("GET_VAL FAILED! for cluster %d, attribute %d with value %d", cluster_id, attribute_id, attrVal->val.u32);
  return false;
}

// set the value of an attribute from its cluster id and attribute it
bool MatterEndPoint::setAttributeVal(uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *attrVal) {
  esp_matter::attribute_t *attribute = getAttribute(cluster_id, attribute_id);
  if (attribute == nullptr) {
    return false;
  }
  if (attribute::set_val(attribute, attrVal) == ESP_OK) {
    log_v("SET_VAL Success for cluster %d, attribute %d with value %d", cluster_id, attribute_id, attrVal->val.u32);
    return true;
  }
  log_e("SET_VAL FAILED! for cluster %d, attribute %d with value %d", cluster_id, attribute_id, attrVal->val.u32);
  return false;
}

// update the value of an attribute from its cluster id and attribute it
bool MatterEndPoint::updateAttributeVal(uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *attrVal) {
  if (attribute::update(endpoint_id, cluster_id, attribute_id, attrVal) == ESP_OK) {
    log_v("Update Success for cluster %d, attribute %d with value %d", cluster_id, attribute_id, attrVal->val.u32);
    return true;
  }
  log_e("Update FAILED! for cluster %d, attribute %d with value %d", cluster_id, attribute_id, attrVal->val.u32);
  return false;
}

// This callback is invoked when clients interact with the Identify Cluster of an specific endpoint.
bool MatterEndPoint::endpointIdentifyCB(uint16_t endpoint_id, bool identifyIsEnabled) {
  if (_onEndPointIdentifyCB) {
    return _onEndPointIdentifyCB(identifyIsEnabled);
  }
  return true;
}

// User callback for the Identify Cluster functionality
void MatterEndPoint::onIdentify(EndPointIdentifyCB onEndPointIdentifyCB) {
  _onEndPointIdentifyCB = onEndPointIdentifyCB;
}

#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
