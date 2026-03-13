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
#include <MatterEndpoints/MatterLightSensor.h>

using namespace esp_matter;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

bool MatterLightSensor::attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) {
  bool ret = true;
  if (!started) {
    log_e("Matter Light Sensor device has not begun.");
    return false;
  }

  log_d("Light Sensor Attr update callback: endpoint: %u, cluster: %u, attribute: %u, val: %u", endpoint_id, cluster_id, attribute_id, val->val.u32);
  return ret;
}

MatterLightSensor::MatterLightSensor() {}

MatterLightSensor::~MatterLightSensor() {
  end();
}

bool MatterLightSensor::begin(uint16_t _rawIlluminance) {
  ArduinoMatter::_init();

  if (getEndPointId() != 0) {
    log_e("Matter Light Sensor with Endpoint Id %d device has already been created.", getEndPointId());
    return false;
  }

  // is it a valid value?
  if (_rawIlluminance > 65534) {
    log_e("Light illuminance value out of range [1..3576000].");
    return false;
  }

  light_sensor::config_t light_sensor_config;
  light_sensor_config.illuminance_measurement.illuminance_measured_value = _rawIlluminance;
  light_sensor_config.illuminance_measurement.illuminance_min_measured_value = nullptr;
  light_sensor_config.illuminance_measurement.illuminance_max_measured_value = nullptr;

  // endpoint handles can be used to add/modify clusters.
  endpoint_t *endpoint = light_sensor::create(node::get(), &light_sensor_config, ENDPOINT_FLAG_NONE, (void *)this);
  if (endpoint == nullptr) {
    log_e("Failed to create Light Sensor endpoint");
    return false;
  }
  rawIlluminance = _rawIlluminance;
  setEndPointId(endpoint::get_id(endpoint));
  log_i("Light Sensor created with endpoint_id %d", getEndPointId());

  started = true;
  return true;
}

void MatterLightSensor::end() {
  started = false;
}

bool MatterLightSensor::setRawIlluminance(uint16_t _rawIlluminance) {
  if (!started) {
    log_e("Matter Light Sensor device has not begun.");
    return false;
  }
  // is it a valid percentage value?
  if (_rawIlluminance > 357600) {
    log_e("Light illuminance value out of range [1..3576].");
    return false;
  }

  // avoid processing if there was no change
  if (rawIlluminance == _rawIlluminance) {
    return true;
  }

  esp_matter_attr_val_t illuminanceVal = esp_matter_invalid(NULL);

  if (!getAttributeVal(IlluminanceMeasurement::Id, IlluminanceMeasurement::Attributes::MeasuredValue::Id, &illuminanceVal)) {
    log_e("Failed to get Light Sensor Attribute.");
    return false;
  }
  if (illuminanceVal.val.u16 != _rawIlluminance) {
    illuminanceVal.val.u16 = _rawIlluminance;
    bool ret;
    ret = updateAttributeVal(IlluminanceMeasurement::Id, IlluminanceMeasurement::Attributes::MeasuredValue::Id, &illuminanceVal);
    if (!ret) {
      log_e("Failed to update Light Sensor Attribute.");
      return false;
    }
    rawIlluminance = _rawIlluminance;
  }
  log_v("Light Sensor set to %.02f Lux", (float)_rawIlluminance / 100.00);

  return true;
}

#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
