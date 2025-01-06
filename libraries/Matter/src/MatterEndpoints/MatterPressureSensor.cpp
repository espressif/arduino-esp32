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
#include <MatterEndpoints/MatterPressureSensor.h>

using namespace esp_matter;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

bool MatterPressureSensor::attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) {
  bool ret = true;
  if (!started) {
    log_e("Matter Pressure Sensor device has not begun.");
    return false;
  }

  log_d("Pressure Sensor Attr update callback: endpoint: %u, cluster: %u, attribute: %u, val: %u", endpoint_id, cluster_id, attribute_id, val->val.u32);
  return ret;
}

MatterPressureSensor::MatterPressureSensor() {}

MatterPressureSensor::~MatterPressureSensor() {
  end();
}

bool MatterPressureSensor::begin(int16_t _rawPressure) {
  ArduinoMatter::_init();

  if (getEndPointId() != 0) {
    log_e("Matter Pressure Sensor with Endpoint Id %d device has already been created.", getEndPointId());
    return false;
  }

  pressure_sensor::config_t pressure_sensor_config;
  pressure_sensor_config.pressure_measurement.pressure_measured_value = _rawPressure;
  pressure_sensor_config.pressure_measurement.pressure_min_measured_value = nullptr;
  pressure_sensor_config.pressure_measurement.pressure_max_measured_value = nullptr;

  // endpoint handles can be used to add/modify clusters
  endpoint_t *endpoint = pressure_sensor::create(node::get(), &pressure_sensor_config, ENDPOINT_FLAG_NONE, (void *)this);
  if (endpoint == nullptr) {
    log_e("Failed to create Pressure Sensor endpoint");
    return false;
  }
  rawPressure = _rawPressure;
  setEndPointId(endpoint::get_id(endpoint));
  log_i("Pressure Sensor created with endpoint_id %d", getEndPointId());
  started = true;
  return true;
}

void MatterPressureSensor::end() {
  started = false;
}

bool MatterPressureSensor::setRawPressure(int16_t _rawPressure) {
  if (!started) {
    log_e("Matter Pressure Sensor device has not begun.");
    return false;
  }

  // avoid processing if there was no change
  if (rawPressure == _rawPressure) {
    return true;
  }

  esp_matter_attr_val_t pressureVal = esp_matter_invalid(NULL);

  if (!getAttributeVal(PressureMeasurement::Id, PressureMeasurement::Attributes::MeasuredValue::Id, &pressureVal)) {
    log_e("Failed to get Pressure Sensor Attribute.");
    return false;
  }
  if (pressureVal.val.i16 != _rawPressure) {
    pressureVal.val.i16 = _rawPressure;
    bool ret;
    ret = updateAttributeVal(PressureMeasurement::Id, PressureMeasurement::Attributes::MeasuredValue::Id, &pressureVal);
    if (!ret) {
      log_e("Failed to update Pressure Sensor Measurement Attribute.");
      return false;
    }
    rawPressure = _rawPressure;
  }
  log_v("Pressure Sensor set to %.02f Degrees", (float)_rawPressure / 100.00);

  return true;
}

#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
