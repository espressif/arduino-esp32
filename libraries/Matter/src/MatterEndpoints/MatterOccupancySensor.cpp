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
#include <MatterEndpoints/MatterOccupancySensor.h>

using namespace esp_matter;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

// clang-format off
const uint8_t MatterOccupancySensor::occupancySensorTypeBitmap[4] = {
  MatterOccupancySensor::occupancySensorTypePir,
  MatterOccupancySensor::occupancySensorTypePir | MatterOccupancySensor::occupancySensorTypeUltrasonic,
  MatterOccupancySensor::occupancySensorTypeUltrasonic,
  MatterOccupancySensor::occupancySensorTypePhysicalContact
};
// clang-format on

bool MatterOccupancySensor::attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) {
  bool ret = true;
  if (!started) {
    log_e("Matter Occupancy Sensor device has not begun.");
    return false;
  }

  log_d("Occupancy Sensor Attr update callback: endpoint: %u, cluster: %u, attribute: %u, val: %u", endpoint_id, cluster_id, attribute_id, val->val.u32);
  return ret;
}

MatterOccupancySensor::MatterOccupancySensor() {}

MatterOccupancySensor::~MatterOccupancySensor() {
  end();
}

bool MatterOccupancySensor::begin(bool _occupancyState, OccupancySensorType_t _occupancySensorType) {
  ArduinoMatter::_init();

  if (getEndPointId() != 0) {
    log_e("Matter Occupancy Sensor with Endpoint Id %d device has already been created.", getEndPointId());
    return false;
  }

  occupancy_sensor::config_t occupancy_sensor_config;
  occupancy_sensor_config.occupancy_sensing.occupancy = _occupancyState;
  occupancy_sensor_config.occupancy_sensing.occupancy_sensor_type = _occupancySensorType;
  occupancy_sensor_config.occupancy_sensing.occupancy_sensor_type_bitmap = occupancySensorTypeBitmap[_occupancySensorType];

  // endpoint handles can be used to add/modify clusters.
  endpoint_t *endpoint = occupancy_sensor::create(node::get(), &occupancy_sensor_config, ENDPOINT_FLAG_NONE, (void *)this);
  if (endpoint == nullptr) {
    log_e("Failed to create Occupancy Sensor endpoint");
    return false;
  }
  occupancyState = _occupancyState;
  setEndPointId(endpoint::get_id(endpoint));
  log_i("Occupancy Sensor created with endpoint_id %d", getEndPointId());
  started = true;
  return true;
}

void MatterOccupancySensor::end() {
  started = false;
}

bool MatterOccupancySensor::setOccupancy(bool _occupancyState) {
  if (!started) {
    log_e("Matter Occupancy Sensor device has not begun.");
    return false;
  }

  // avoid processing if there was no change
  if (occupancyState == _occupancyState) {
    return true;
  }

  esp_matter_attr_val_t occupancyVal = esp_matter_invalid(NULL);

  if (!getAttributeVal(OccupancySensing::Id, OccupancySensing::Attributes::Occupancy::Id, &occupancyVal)) {
    log_e("Failed to get Occupancy Sensor Attribute.");
    return false;
  }
  if (occupancyVal.val.u8 != _occupancyState) {
    occupancyVal.val.u8 = _occupancyState;
    bool ret;
    ret = updateAttributeVal(OccupancySensing::Id, OccupancySensing::Attributes::Occupancy::Id, &occupancyVal);
    if (!ret) {
      log_e("Failed to update Occupancy Sensor Attribute.");
      return false;
    }
    occupancyState = _occupancyState;
  }
  log_v("Occupancy Sensor set to %s", _occupancyState ? "Occupied" : "Vacant");

  return true;
}

#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
