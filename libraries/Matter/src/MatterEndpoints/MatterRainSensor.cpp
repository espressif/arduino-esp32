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

#include <Matter.h>
#include <app/server/Server.h>
#include <MatterEndpoints/MatterRainSensor.h>

using namespace esp_matter;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

bool MatterRainSensor::attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) {
  bool ret = true;
  if (!started) {
    log_e("Matter Rain Sensor device has not begun.");
    return false;
  }

  log_d("Rain Sensor Attr update callback: endpoint: %u, cluster: %u, attribute: %u, val: %u", endpoint_id, cluster_id, attribute_id, val->val.u32);
  return ret;
}

MatterRainSensor::MatterRainSensor() {}

MatterRainSensor::~MatterRainSensor() {
  end();
}

bool MatterRainSensor::begin(bool _rainState) {
  ArduinoMatter::_init();

  if (getEndPointId() != 0) {
    log_e("Matter Rain Sensor with Endpoint Id %d device has already been created.", getEndPointId());
    return false;
  }

  rain_sensor::config_t rain_sensor_config;
  rain_sensor_config.boolean_state.state_value = _rainState;

  // endpoint handles can be used to add/modify clusters.
  endpoint_t *endpoint = rain_sensor::create(node::get(), &rain_sensor_config, ENDPOINT_FLAG_NONE, (void *)this);
  if (endpoint == nullptr) {
    log_e("Failed to create Rain Sensor endpoint");
    return false;
  }
  rainState = _rainState;
  setEndPointId(endpoint::get_id(endpoint));
  log_i("Rain Sensor created with endpoint_id %d", getEndPointId());

  started = true;
  return true;
}

void MatterRainSensor::end() {
  started = false;
}

bool MatterRainSensor::setRain(bool _rainState) {
  if (!started) {
    log_e("Matter Rain Sensor device has not begun.");
    return false;
  }

  // avoid processing if there was no change
  if (rainState == _rainState) {
    return true;
  }

  esp_matter_attr_val_t rainVal = esp_matter_invalid(NULL);

  if (!getAttributeVal(BooleanState::Id, BooleanState::Attributes::StateValue::Id, &rainVal)) {
    log_e("Failed to get Rain Sensor Attribute.");
    return false;
  }
  if (rainVal.val.u8 != _rainState) {
    rainVal.val.u8 = _rainState;
    bool ret;
    ret = updateAttributeVal(BooleanState::Id, BooleanState::Attributes::StateValue::Id, &rainVal);
    if (!ret) {
      log_e("Failed to update Rain Sensor Attribute.");
      return false;
    }
    rainState = _rainState;
  }
  log_v("Rain Sensor set to %s", _rainState ? "Detected" : "Not Detected");

  return true;
}

#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */

