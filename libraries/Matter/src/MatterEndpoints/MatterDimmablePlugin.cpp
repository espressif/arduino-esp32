// Copyright 2025 Espressif Systems (Shanghai) PTE LTD
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

#include <sdkconfig.h>
#ifdef CONFIG_ESP_MATTER_ENABLE_DATA_MODEL

#include <Matter.h>
#include <app/server/Server.h>
#include <MatterEndpoints/MatterDimmablePlugin.h>

using namespace esp_matter;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

bool MatterDimmablePlugin::attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) {
  bool ret = true;
  if (!started) {
    log_e("Matter DimmablePlugin device has not begun.");
    return false;
  }

  log_d("DimmablePlugin Attr update callback: endpoint: %u, cluster: %" PRIu32 ", attribute: %" PRIu32 ", val: %" PRIu32, endpoint_id, cluster_id, attribute_id, val->val.u32);

  if (endpoint_id == getEndPointId()) {
    switch (cluster_id) {
      case OnOff::Id:
        if (attribute_id == OnOff::Attributes::OnOff::Id) {
          log_d("DimmablePlugin On/Off State changed to %u", val->val.b);
          if (_onChangeOnOffCB != NULL) {
            ret &= _onChangeOnOffCB(val->val.b);
          }
          if (_onChangeCB != NULL) {
            ret &= _onChangeCB(val->val.b, level);
          }
          if (ret == true) {
            onOffState = val->val.b;
          }
        }
        break;
      case LevelControl::Id:
        if (attribute_id == LevelControl::Attributes::CurrentLevel::Id) {
          log_d("DimmablePlugin Level changed to %u", val->val.u8);
          if (_onChangeLevelCB != NULL) {
            ret &= _onChangeLevelCB(val->val.u8);
          }
          if (_onChangeCB != NULL) {
            ret &= _onChangeCB(onOffState, val->val.u8);
          }
          if (ret == true) {
            level = val->val.u8;
          }
        }
        break;
    }
  }
  return ret;
}

MatterDimmablePlugin::MatterDimmablePlugin() {}

MatterDimmablePlugin::~MatterDimmablePlugin() {
  end();
}

bool MatterDimmablePlugin::begin(bool initialState, uint8_t level) {
  ArduinoMatter::_init();
  if (getEndPointId() != 0) {
    log_e("Matter Dimmable Plugin with Endpoint Id %u device has already been created.", getEndPointId());
    return false;
  }

  dimmable_plugin_unit::config_t plugin_config;
  plugin_config.on_off.on_off = initialState;
  plugin_config.on_off.lighting.start_up_on_off = nullptr;
  onOffState = initialState;

  plugin_config.level_control.current_level = level;
  plugin_config.level_control.lighting.start_up_current_level = nullptr;
  this->level = level;

  // endpoint handles can be used to add/modify clusters.
  endpoint_t *endpoint = dimmable_plugin_unit::create(node::get(), &plugin_config, ENDPOINT_FLAG_NONE, (void *)this);
  if (endpoint == nullptr) {
    log_e("Failed to create dimmable plugin endpoint");
    return false;
  }

  setEndPointId(endpoint::get_id(endpoint));
  log_i("Dimmable Plugin created with endpoint_id %u", getEndPointId());

  /* Mark deferred persistence for some attributes that might be changed rapidly */
  cluster_t *level_control_cluster = cluster::get(endpoint, LevelControl::Id);
  esp_matter::attribute_t *current_level_attribute = attribute::get(level_control_cluster, LevelControl::Attributes::CurrentLevel::Id);
  attribute::set_deferred_persistence(current_level_attribute);

  started = true;
  return true;
}

void MatterDimmablePlugin::end() {
  started = false;
}

bool MatterDimmablePlugin::setOnOff(bool newState) {
  if (!started) {
    log_e("Matter Dimmable Plugin device has not begun.");
    return false;
  }

  // avoid processing if there was no change
  if (onOffState == newState) {
    return true;
  }

  onOffState = newState;

  endpoint_t *endpoint = endpoint::get(node::get(), getEndPointId());
  cluster_t *cluster = cluster::get(endpoint, OnOff::Id);
  esp_matter::attribute_t *attribute = attribute::get(cluster, OnOff::Attributes::OnOff::Id);

  esp_matter_attr_val_t val = esp_matter_invalid(NULL);
  attribute::get_val(attribute, &val);

  if (val.val.b != onOffState) {
    val.val.b = onOffState;
    attribute::update(getEndPointId(), OnOff::Id, OnOff::Attributes::OnOff::Id, &val);
  }
  return true;
}

void MatterDimmablePlugin::updateAccessory() {
  if (_onChangeCB != NULL) {
    _onChangeCB(onOffState, level);
  }
}

bool MatterDimmablePlugin::getOnOff() {
  return onOffState;
}

bool MatterDimmablePlugin::toggle() {
  return setOnOff(!onOffState);
}

bool MatterDimmablePlugin::setLevel(uint8_t newLevel) {
  if (!started) {
    log_w("Matter Dimmable Plugin device has not begun.");
    return false;
  }

  // avoid processing if there was no change
  if (level == newLevel) {
    return true;
  }

  level = newLevel;

  endpoint_t *endpoint = endpoint::get(node::get(), getEndPointId());
  cluster_t *cluster = cluster::get(endpoint, LevelControl::Id);
  esp_matter::attribute_t *attribute = attribute::get(cluster, LevelControl::Attributes::CurrentLevel::Id);

  esp_matter_attr_val_t val = esp_matter_invalid(NULL);
  attribute::get_val(attribute, &val);

  if (val.val.u8 != level) {
    val.val.u8 = level;
    attribute::update(getEndPointId(), LevelControl::Id, LevelControl::Attributes::CurrentLevel::Id, &val);
  }
  return true;
}

uint8_t MatterDimmablePlugin::getLevel() {
  return level;
}

MatterDimmablePlugin::operator bool() {
  return getOnOff();
}

void MatterDimmablePlugin::operator=(bool newState) {
  setOnOff(newState);
}
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
