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

#include <sdkconfig.h>
#ifdef CONFIG_ESP_MATTER_ENABLE_DATA_MODEL

#include <Matter.h>
#include <app/server/Server.h>
#include <MatterEndpoints/MatterOnOffPlugin.h>

using namespace esp_matter;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

bool MatterOnOffPlugin::attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) {
  bool ret = true;
  if (!started) {
    log_e("Matter On-Off Plugin device has not begun.");
    return false;
  }

  log_d("OnOff Attr update callback: endpoint: %u, cluster: %u, attribute: %u, val: %u", endpoint_id, cluster_id, attribute_id, val->val.u32);

  if (endpoint_id == getEndPointId()) {
    log_d("OnOffPlugin state changed to %d", val->val.b);
    if (cluster_id == OnOff::Id) {
      if (attribute_id == OnOff::Attributes::OnOff::Id) {
        if (_onChangeOnOffCB != NULL) {
          ret &= _onChangeOnOffCB(val->val.b);
        }
        if (_onChangeCB != NULL) {
          ret &= _onChangeCB(val->val.b);
        }
        if (ret == true) {
          onOffState = val->val.b;
        }
      }
    }
  }
  return ret;
}

MatterOnOffPlugin::MatterOnOffPlugin() {}

MatterOnOffPlugin::~MatterOnOffPlugin() {
  end();
}

bool MatterOnOffPlugin::begin(bool initialState) {
  ArduinoMatter::_init();

  if (getEndPointId() != 0) {
    log_e("Matter On-Off Plugin with Endpoint Id %d device has already been created.", getEndPointId());
    return false;
  }

  on_off_plugin_unit::config_t plugin_config;
  plugin_config.on_off.on_off = initialState;
  plugin_config.on_off.lighting.start_up_on_off = nullptr;

  // endpoint handles can be used to add/modify clusters.
  endpoint_t *endpoint = on_off_plugin_unit::create(node::get(), &plugin_config, ENDPOINT_FLAG_NONE, (void *)this);
  if (endpoint == nullptr) {
    log_e("Failed to create on-off plugin endpoint");
    return false;
  }
  onOffState = initialState;
  setEndPointId(endpoint::get_id(endpoint));
  log_i("On-Off Plugin created with endpoint_id %d", getEndPointId());
  started = true;
  return true;
}

void MatterOnOffPlugin::end() {
  started = false;
}

void MatterOnOffPlugin::updateAccessory() {
  if (_onChangeCB != NULL) {
    _onChangeCB(onOffState);
  }
}

bool MatterOnOffPlugin::setOnOff(bool newState) {
  if (!started) {
    log_e("Matter On-Off Plugin device has not begun.");
    return false;
  }

  // avoid processing if there was no change
  if (onOffState == newState) {
    return true;
  }

  esp_matter_attr_val_t onoffVal = esp_matter_invalid(NULL);

  if (!getAttributeVal(OnOff::Id, OnOff::Attributes::OnOff::Id, &onoffVal)) {
    log_e("Failed to get Pressure Sensor Attribute.");
    return false;
  }
  if (onoffVal.val.b != newState) {
    onoffVal.val.b = newState;
    bool ret;
    ret = updateAttributeVal(OnOff::Id, OnOff::Attributes::OnOff::Id, &onoffVal);
    if (!ret) {
      log_e("Failed to update Pressure Sensor Measurement Attribute.");
      return false;
    }
    onOffState = newState;
  }
  log_v("Plugin OnOff state set to %s", newState ? "ON" : "OFF");
  return true;
}

bool MatterOnOffPlugin::getOnOff() {
  return onOffState;
}

bool MatterOnOffPlugin::toggle() {
  return setOnOff(!onOffState);
}

MatterOnOffPlugin::operator bool() {
  return getOnOff();
}

void MatterOnOffPlugin::operator=(bool newState) {
  setOnOff(newState);
}
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
