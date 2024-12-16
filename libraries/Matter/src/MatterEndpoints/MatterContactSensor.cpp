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
#include <MatterEndpoints/MatterContactSensor.h>

using namespace esp_matter;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

bool MatterContactSensor::attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) {
  bool ret = true;
  if (!started) {
    log_e("Matter Contact Sensor device has not begun.");
    return false;
  }

  log_d("Contact Sensor Attr update callback: endpoint: %u, cluster: %u, attribute: %u, val: %u", endpoint_id, cluster_id, attribute_id, val->val.u32);
  return ret;
}

MatterContactSensor::MatterContactSensor() {}

MatterContactSensor::~MatterContactSensor() {
  end();
}

bool MatterContactSensor::begin(bool _contactState) {
  ArduinoMatter::_init();

  if (getEndPointId() != 0) {
    log_e("Matter Contact Sensor with Endpoint Id %d device has already been created.", getEndPointId());
    return false;
  }

  contact_sensor::config_t contact_sensor_config;
  contact_sensor_config.boolean_state.state_value = _contactState;

  // endpoint handles can be used to add/modify clusters.
  endpoint_t *endpoint = contact_sensor::create(node::get(), &contact_sensor_config, ENDPOINT_FLAG_NONE, (void *)this);
  if (endpoint == nullptr) {
    log_e("Failed to create Contact Sensor endpoint");
    return false;
  }
  contactState = _contactState;
  setEndPointId(endpoint::get_id(endpoint));
  log_i("Contact Sensor created with endpoint_id %d", getEndPointId());
  started = true;
  return true;
}

void MatterContactSensor::end() {
  started = false;
}

bool MatterContactSensor::setContact(bool _contactState) {
  if (!started) {
    log_e("Matter Contact Sensor device has not begun.");
    return false;
  }

  // avoid processing if there was no change
  if (contactState == _contactState) {
    return true;
  }

  esp_matter_attr_val_t contactVal = esp_matter_invalid(NULL);

  if (!getAttributeVal(BooleanState::Id, BooleanState::Attributes::StateValue::Id, &contactVal)) {
    log_e("Failed to get Contact Sensor Attribute.");
    return false;
  }
  if (contactVal.val.u8 != _contactState) {
    contactVal.val.u8 = _contactState;
    bool ret;
    ret = updateAttributeVal(BooleanState::Id, BooleanState::Attributes::StateValue::Id, &contactVal);
    if (!ret) {
      log_e("Failed to update Contact Sensor Attribute.");
      return false;
    }
    contactState = _contactState;
  }
  log_v("Contact Sensor set to %s", _contactState ? "Closed" : "Open");

  return true;
}

#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
