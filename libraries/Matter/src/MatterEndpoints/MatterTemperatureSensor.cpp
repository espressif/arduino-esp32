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
#include <MatterEndpoints/MatterTemperatureSensor.h>

using namespace esp_matter;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

bool MatterTemperatureSensor::attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) {
  bool ret = true;
  if (!started) {
    log_e("Matter Temperature Sensor device has not begun.");
    return false;
  }

  log_d("Temperature Sensor Attr update callback: endpoint: %u, cluster: %u, attribute: %u, val: %u", endpoint_id, cluster_id, attribute_id, val->val.u32);
  return ret;
}

MatterTemperatureSensor::MatterTemperatureSensor() {}

MatterTemperatureSensor::~MatterTemperatureSensor() {
  end();
}

bool MatterTemperatureSensor::begin(int16_t _rawTemperature) {
  ArduinoMatter::_init();

  if (getEndPointId() != 0) {
    log_e("Temperature Sensor with Endpoint Id %d device has already been created.", getEndPointId());
    return false;
  }

  temperature_sensor::config_t temperature_sensor_config;
  temperature_sensor_config.temperature_measurement.measured_value = _rawTemperature;
  temperature_sensor_config.temperature_measurement.min_measured_value = nullptr;
  temperature_sensor_config.temperature_measurement.max_measured_value = nullptr;

  // endpoint handles can be used to add/modify clusters
  endpoint_t *endpoint = temperature_sensor::create(node::get(), &temperature_sensor_config, ENDPOINT_FLAG_NONE, (void *)this);
  if (endpoint == nullptr) {
    log_e("Failed to create Temperature Sensor endpoint");
    return false;
  }
  rawTemperature = _rawTemperature;
  setEndPointId(endpoint::get_id(endpoint));
  log_i("Temperature Sensor created with endpoint_id %d", getEndPointId());
  started = true;
  return true;
}

void MatterTemperatureSensor::end() {
  started = false;
}

bool MatterTemperatureSensor::setRawTemperature(int16_t _rawTemperature) {
  if (!started) {
    log_e("Matter Temperature Sensor device has not begun.");
    return false;
  }

  // avoid processing if there was no change
  if (rawTemperature == _rawTemperature) {
    return true;
  }

  esp_matter_attr_val_t temperatureVal = esp_matter_invalid(NULL);

  if (!getAttributeVal(TemperatureMeasurement::Id, TemperatureMeasurement::Attributes::MeasuredValue::Id, &temperatureVal)) {
    log_e("Failed to get Temperature Sensor Attribute.");
    return false;
  }
  if (temperatureVal.val.i16 != _rawTemperature) {
    temperatureVal.val.i16 = _rawTemperature;
    bool ret;
    ret = updateAttributeVal(TemperatureMeasurement::Id, TemperatureMeasurement::Attributes::MeasuredValue::Id, &temperatureVal);
    if (!ret) {
      log_e("Failed to update Temperature Sensor Attribute.");
      return false;
    }
    rawTemperature = _rawTemperature;
  }
  log_v("Temperature Sensor set to %.02fC", (float)_rawTemperature / 100.00);

  return true;
}

#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
