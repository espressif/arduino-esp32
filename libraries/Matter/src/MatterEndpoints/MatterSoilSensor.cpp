// Copyright 2026 Espressif Systems (Shanghai) PTE LTD
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
#include <MatterEndpoints/MatterSoilSensor.h>
#include <clusters/SoilMeasurement/Attributes.h>
#include <clusters/soil_measurement/integration.h>

using namespace esp_matter;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;
namespace soil_endpoint = esp_matter::endpoint::soil_sensor;

namespace {

using SoilMoistureValue = SoilMeasurement::Attributes::SoilMoistureMeasuredValue::TypeInfo::Type;

void setSoilMoistureLimitsForEndpoint(uint16_t endpoint_id) {
  SoilMeasurement::Attributes::SoilMoistureMeasurementLimits::TypeInfo::Type limits{};
  limits.measurementType = Globals::MeasurementTypeEnum::kSoilMoisture;
  limits.measured = true;
  limits.minMeasuredValue = 0;
  limits.maxMeasuredValue = 100;
  SoilMeasurement::SetSoilMoistureLimits(endpoint_id, limits);
}

}  // namespace

bool MatterSoilSensor::attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) {
  if (!started) {
    log_e("Matter Soil Sensor device has not begun.");
    return false;
  }

  if (val != nullptr) {
    log_d(
      "Soil Sensor Attr update callback: endpoint: %u, cluster: %" PRIu32 ", attribute: %" PRIu32 ", val: %" PRIu32, endpoint_id, cluster_id, attribute_id,
      val->val.u32
    );
  } else {
    log_d(
      "Soil Sensor Attr update callback: endpoint: %u, cluster: %" PRIu32 ", attribute: %" PRIu32 ", val: (null)", endpoint_id, cluster_id, attribute_id
    );
  }
  return true;
}

MatterSoilSensor::MatterSoilSensor() {}

MatterSoilSensor::~MatterSoilSensor() {
  end();
}

void MatterSoilSensor::destroySoilSensorEndpoint(endpoint_t *endpoint) {
  node_t *node = node::get();
  if (node != nullptr && endpoint != nullptr) {
    endpoint::destroy(node, endpoint);
  }
}

bool MatterSoilSensor::createSoilSensorEndpoint() {
  if (!ensureMatterNode()) {
    return false;
  }

  if (getEndPointId() != 0) {
    log_e("Matter Soil Sensor with Endpoint Id %u device has already been created.", getEndPointId());
    return false;
  }

  // soil_measurement::config_t is common::config_t in the legacy data-model headers shipped with
  // esp32-arduino-libs; measured value is driven only via SoilMeasurementCluster after Matter.begin().
  soil_endpoint::config_t soil_sensor_config{};

  endpoint_t *endpoint = soil_endpoint::create(node::get(), &soil_sensor_config, ENDPOINT_FLAG_NONE, (void *)this);
  if (endpoint == nullptr) {
    log_e("Failed to create Soil Sensor endpoint");
    return false;
  }

  setSoilMoistureLimitsForEndpoint(endpoint::get_id(endpoint));

  if (!registerCreatedEndpoint(endpoint)) {
    log_e("Failed to register Soil Sensor endpoint");
    destroySoilSensorEndpoint(endpoint);
    return false;
  }

  log_i("Soil Sensor created with endpoint_id %u", getEndPointId());

  started = true;
  soilMoisture = 0;
  hasPendingMoistureReport = false;

  return true;
}

bool MatterSoilSensor::begin() {
  return createSoilSensorEndpoint();
}

bool MatterSoilSensor::begin(uint8_t initialSoilMoisturePercent) {
  if (initialSoilMoisturePercent > 100) {
    log_e("Soil Sensor Moisture Percentage value out of range [0..100].");
    return false;
  }

  if (!createSoilSensorEndpoint()) {
    return false;
  }

  if (!setSoilMoisture(initialSoilMoisturePercent)) {
    endpoint_t *endpoint = endpoint::get(node::get(), getEndPointId());
    destroySoilSensorEndpoint(endpoint);
    setEndPointId(0);
    started = false;
    soilMoisture = 0;
    hasPendingMoistureReport = false;
    return false;
  }

  return true;
}

void MatterSoilSensor::end() {
  started = false;
  hasPendingMoistureReport = false;
}

void MatterSoilSensor::onStackStarted() {
  if (!hasPendingMoistureReport) {
    return;
  }

  if (findRegisteredCluster(SoilMeasurement::Id) == nullptr) {
    log_e("Soil Measurement cluster not found on endpoint %u after Matter.begin().", getEndPointId());
    return;
  }

  lock::ScopedChipStackLock lock(portMAX_DELAY);
  SoilMoistureValue val(soilMoisture);
  if (SoilMeasurement::SetSoilMoistureMeasuredValue(getEndPointId(), val) != CHIP_NO_ERROR) {
    log_e("Failed to apply cached Soil Sensor moisture value after Matter.begin().");
  }
}

bool MatterSoilSensor::setSoilMoisture(uint8_t soilMoisturePercent) {
  if (!started) {
    log_e("Matter Soil Sensor device has not begun.");
    return false;
  }
  if (soilMoisturePercent > 100) {
    log_e("Soil Sensor Moisture Percentage value out of range [0..100].");
    return false;
  }

  if (soilMoisture == soilMoisturePercent && findRegisteredCluster(SoilMeasurement::Id) != nullptr) {
    hasPendingMoistureReport = true;
    return true;
  }

  if (findRegisteredCluster(SoilMeasurement::Id) == nullptr) {
    soilMoisture = soilMoisturePercent;
    hasPendingMoistureReport = true;
    return true;
  }

  lock::ScopedChipStackLock lock(portMAX_DELAY);
  SoilMoistureValue val(soilMoisturePercent);
  if (SoilMeasurement::SetSoilMoistureMeasuredValue(getEndPointId(), val) != CHIP_NO_ERROR) {
    log_e("Failed to update Soil Sensor moisture value.");
    return false;
  }
  soilMoisture = soilMoisturePercent;
  hasPendingMoistureReport = true;
  log_v("Soil Sensor set to %u Percent", soilMoisturePercent);

  return true;
}

#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
