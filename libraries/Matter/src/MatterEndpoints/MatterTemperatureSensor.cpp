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
#include <MatterEndpoints/MatterTemperatureSensor.h>
#include <app/clusters/temperature-measurement-server/TemperatureMeasurementCluster.h>
#include <app/data-model/Nullable.h>

using namespace esp_matter;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

namespace {
bool celsiusToRaw(double temperature, int16_t *rawOut) {
  const double raw = temperature * 100.0;
  if (raw < (double)INT16_MIN || raw > (double)INT16_MAX) {
    log_e("Temperature %.02fC is out of range [%.02f..%.02f].", temperature, (double)INT16_MIN / 100.0, (double)INT16_MAX / 100.0);
    return false;
  }
  *rawOut = static_cast<int16_t>(raw);
  return true;
}
}  // namespace

bool MatterTemperatureSensor::attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) {
  bool ret = true;
  if (!started) {
    log_e("Matter Temperature Sensor device has not begun.");
    return false;
  }

  log_d(
    "Temperature Sensor Attr update callback: endpoint: %u, cluster: %" PRIu32 ", attribute: %" PRIu32 ", val: %" PRIu32, endpoint_id, cluster_id, attribute_id,
    val->val.u32
  );
  return ret;
}

bool MatterTemperatureSensor::begin(double temperature) {
  int16_t rawTemperatureValue = 0;
  if (!celsiusToRaw(temperature, &rawTemperatureValue)) {
    return false;
  }
  return begin(rawTemperatureValue);
}

bool MatterTemperatureSensor::setTemperature(double temperature) {
  int16_t rawTemperatureValue = 0;
  if (!celsiusToRaw(temperature, &rawTemperatureValue)) {
    return false;
  }
  return setRawTemperature(rawTemperatureValue);
}

MatterTemperatureSensor::MatterTemperatureSensor() {}

MatterTemperatureSensor::~MatterTemperatureSensor() {
  end();
}

bool MatterTemperatureSensor::begin(int16_t _rawTemperature) {
  ensureMatterNode();

  if (getEndPointId() != 0) {
    log_e("Temperature Sensor with Endpoint Id %u device has already been created.", getEndPointId());
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

  log_i("Temperature Sensor created with endpoint_id %u", getEndPointId());

  started = true;
  return true;
}

void MatterTemperatureSensor::end() {
  started = false;
}

void MatterTemperatureSensor::onStackStarted() {
  TemperatureMeasurementCluster *cluster = static_cast<TemperatureMeasurementCluster *>(findRegisteredCluster(TemperatureMeasurement::Id));
  if (cluster == nullptr) {
    log_e("TemperatureMeasurement cluster not found after Matter.begin().");
    return;
  }
  lock::ScopedChipStackLock lock(portMAX_DELAY);
  if (cluster->SetMeasuredValue(chip::app::DataModel::MakeNullable(rawTemperature)) != CHIP_NO_ERROR) {
    log_e("Failed to apply cached Temperature Sensor value after Matter.begin().");
  }
}

bool MatterTemperatureSensor::setRawTemperature(int16_t _rawTemperature) {
  if (!started) {
    log_e("Matter Temperature Sensor device has not begun.");
    return false;
  }

  if (rawTemperature == _rawTemperature && findRegisteredCluster(TemperatureMeasurement::Id) != nullptr) {
    return true;
  }

  TemperatureMeasurementCluster *cluster = static_cast<TemperatureMeasurementCluster *>(findRegisteredCluster(TemperatureMeasurement::Id));
  if (cluster == nullptr) {
    rawTemperature = _rawTemperature;
    return true;
  }

  lock::ScopedChipStackLock lock(portMAX_DELAY);
  if (cluster->SetMeasuredValue(chip::app::DataModel::MakeNullable(_rawTemperature)) != CHIP_NO_ERROR) {
    log_e("Failed to update Temperature Sensor Attribute.");
    return false;
  }
  rawTemperature = _rawTemperature;
  log_v("Temperature Sensor set to %.02fC", (float)_rawTemperature / 100.00);
  return true;
}

#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
