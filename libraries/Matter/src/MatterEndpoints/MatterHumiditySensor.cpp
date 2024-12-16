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
#include <MatterEndpoints/MatterHumiditySensor.h>

using namespace esp_matter;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

bool MatterHumiditySensor::attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) {
  bool ret = true;
  if (!started) {
    log_e("Matter Humidity Sensor device has not begun.");
    return false;
  }

  log_d("Humidity Sensor Attr update callback: endpoint: %u, cluster: %u, attribute: %u, val: %u", endpoint_id, cluster_id, attribute_id, val->val.u32);
  return ret;
}

MatterHumiditySensor::MatterHumiditySensor() {}

MatterHumiditySensor::~MatterHumiditySensor() {
  end();
}

bool MatterHumiditySensor::begin(uint16_t _rawHumidity) {
  ArduinoMatter::_init();

  if (getEndPointId() != 0) {
    log_e("Matter Humidity Sensor with Endpoint Id %d device has already been created.", getEndPointId());
    return false;
  }

  // is it a valid percentage value?
  if (_rawHumidity > 10000) {
    log_e("Humidity Sensor Percentage value out of range [0..100].");
    return false;
  }

  humidity_sensor::config_t humidity_sensor_config;
  humidity_sensor_config.relative_humidity_measurement.measured_value = _rawHumidity;
  humidity_sensor_config.relative_humidity_measurement.min_measured_value = nullptr;
  humidity_sensor_config.relative_humidity_measurement.max_measured_value = nullptr;

  // endpoint handles can be used to add/modify clusters.
  endpoint_t *endpoint = humidity_sensor::create(node::get(), &humidity_sensor_config, ENDPOINT_FLAG_NONE, (void *)this);
  if (endpoint == nullptr) {
    log_e("Failed to create Humidity Sensor endpoint");
    return false;
  }
  rawHumidity = _rawHumidity;
  setEndPointId(endpoint::get_id(endpoint));
  log_i("Humidity Sensor created with endpoint_id %d", getEndPointId());
  started = true;
  return true;
}

void MatterHumiditySensor::end() {
  started = false;
}

bool MatterHumiditySensor::setRawHumidity(uint16_t _rawHumidity) {
  if (!started) {
    log_e("Matter Humidity Sensor device has not begun.");
    return false;
  }
  // is it a valid percentage value?
  if (_rawHumidity > 10000) {
    log_e("Humidity Sensor Percentage value out of range [0..100].");
    return false;
  }

  // avoid processing if there was no change
  if (rawHumidity == _rawHumidity) {
    return true;
  }

  esp_matter_attr_val_t humidityVal = esp_matter_invalid(NULL);

  if (!getAttributeVal(RelativeHumidityMeasurement::Id, RelativeHumidityMeasurement::Attributes::MeasuredValue::Id, &humidityVal)) {
    log_e("Failed to get Humidity Sensor Attribute.");
    return false;
  }
  if (humidityVal.val.u16 != _rawHumidity) {
    humidityVal.val.u16 = _rawHumidity;
    bool ret;
    ret = updateAttributeVal(RelativeHumidityMeasurement::Id, RelativeHumidityMeasurement::Attributes::MeasuredValue::Id, &humidityVal);
    if (!ret) {
      log_e("Failed to update Humidity Sensor Attribute.");
      return false;
    }
    rawHumidity = _rawHumidity;
  }
  log_v("Humidity Sensor set to %.02f Percent", (float)_rawHumidity / 100.00);

  return true;
}

#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
