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
#include <initializer_list>

using namespace esp_matter;

// A single Matter semantic tag (Descriptor cluster TagList entry). Tags disambiguate sibling
// endpoints that expose the same device type, or otherwise clarify an endpoint's role/position
// (e.g. tagging 3 buttons with Number (One/Two/Three) and Position (Top/Middle/Bottom) tags so a
// controller can tell them apart).
// namespaceId/tag values come from the Matter "Standard Namespaces" specification:
// https://github.com/CHIP-Specifications/connectedhomeip-spec/blob/master/src/namespaces
// See MatterTags.h for named constants covering the common namespaces (no magic numbers required)
// and MatterTags::createTag() for a custom namespace/tag/label combination.
struct MatterTag {
  uint8_t namespaceId;
  uint8_t tag;
  const char *label = nullptr;  // optional, nullptr = no label
};

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

  // Maximum number of Descriptor TagList entries per endpoint.
  // Matches esp-matter ESP_MATTER_MAX_SEMANTIC_TAG_COUNT.
  static constexpr uint8_t MAX_TAG_LIST_SIZE = 3;

  // Sets the Descriptor cluster TagList attribute for this endpoint, replacing any tag list set previously.
  // Enables the TagList feature on first use. Call after the endpoint begin() and before Matter.begin().
  // At most MAX_TAG_LIST_SIZE entries are accepted.
  // Switches Custom and Position Row/Column tags require a non-empty label;
  // use createCustomTag(), createRowTag(), or createColumnTag().
  // Each entry's optional `label` pointer, if set, must remain valid for as long as this endpoint is running
  // (it is not copied).
  bool setTagList(const MatterTag *tagList, uint8_t count);

  // Convenience overload: Light1.setTagList({MatterTags::Position::Top, MatterTags::Number::One});
  bool setTagList(std::initializer_list<MatterTag> tagList);

protected:
  // used for secondary network interface endpoints
  static uint16_t secondary_network_endpoint_id;
  // main endpoint ID
  uint16_t endpoint_id = 0;
  EndPointIdentifyCB _onEndPointIdentifyCB = nullptr;
  bool tagListEnabled = false;

  // Enables the Descriptor cluster TagList feature on this endpoint so setTagList() can be used.
  // Called automatically by setTagList(). Idempotent.
  // Subclasses that want TagList advertised even when the sketch never calls setTagList()
  // (Generic Switch) may call this from begin() after setEndPointId().
  bool enableTagList();
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
