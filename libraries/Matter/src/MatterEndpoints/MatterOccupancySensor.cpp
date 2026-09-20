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
#include <MatterEndpoints/MatterOccupancySensor.h>
#include <esp_matter_cluster.h>
#include <esp_matter_attribute.h>
#include <esp_matter_core.h>
#include <algorithm>
#include <app/clusters/occupancy-sensor-server/OccupancySensingCluster.h>

using namespace esp_matter;
using namespace esp_matter::endpoint;
using namespace esp_matter::cluster;
using namespace esp_matter::cluster::occupancy_sensing::attribute;
using namespace chip::app::Clusters;

// clang-format off
// Indexed by OccupancySensorTypeEnum: kPir, kUltrasonic, kPIRAndUltrasonic, kPhysicalContact
const uint8_t MatterOccupancySensor::occupancySensorTypeBitmap[4] = {
  MatterOccupancySensor::occupancySensorTypePir,
  MatterOccupancySensor::occupancySensorTypeUltrasonic,
  MatterOccupancySensor::occupancySensorTypePir | MatterOccupancySensor::occupancySensorTypeUltrasonic,
  MatterOccupancySensor::occupancySensorTypePhysicalContact
};
// clang-format on

bool MatterOccupancySensor::attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) {
  if (!started) {
    log_e("Matter Occupancy Sensor device has not begun.");
    return false;
  }

  log_d("Occupancy Sensor Attr update callback: endpoint: %u, cluster: %" PRIu32 ", attribute: %" PRIu32, endpoint_id, cluster_id, attribute_id);

  if (cluster_id == OccupancySensing::Id && attribute_id == OccupancySensing::Attributes::HoldTime::Id) {
    const uint16_t newHoldTime = val->val.u16;
    if (_onHoldTimeChangeCB && !_onHoldTimeChangeCB(newHoldTime)) {
      return false;
    }
    holdTime_seconds = newHoldTime;
  }

  return true;
}

void MatterOccupancySensor::onHoldTimeChange(HoldTimeChangeCB onHoldTimeChangeCB) {
  _onHoldTimeChangeCB = onHoldTimeChangeCB;
}

MatterOccupancySensor::MatterOccupancySensor() {}

MatterOccupancySensor::~MatterOccupancySensor() {
  end();
}

bool MatterOccupancySensor::begin(bool _occupancyState, OccupancySensorType_t _occupancySensorType) {
  ensureMatterNode();

  holdTime_seconds = 0;
  if (getEndPointId() != 0) {
    log_e("Matter Occupancy Sensor with Endpoint Id %u device has already been created.", getEndPointId());
    return false;
  }
  occupancySensorType = _occupancySensorType;

  occupancy_sensor::config_t occupancy_sensor_config;
  occupancy_sensor_config.occupancy_sensing.occupancy = _occupancyState;
  occupancy_sensor_config.occupancy_sensing.occupancy_sensor_type = _occupancySensorType;
  occupancy_sensor_config.occupancy_sensing.occupancy_sensor_type_bitmap = occupancySensorTypeBitmap[_occupancySensorType];

  using namespace esp_matter::cluster::occupancy_sensing::feature;

  switch (_occupancySensorType) {
    case OCCUPANCY_SENSOR_TYPE_PIR:        occupancy_sensor_config.occupancy_sensing.feature_flags = passive_infrared::get_id(); break;
    case OCCUPANCY_SENSOR_TYPE_ULTRASONIC: occupancy_sensor_config.occupancy_sensing.feature_flags = ultrasonic::get_id(); break;
    case OCCUPANCY_SENSOR_TYPE_PIR_AND_ULTRASONIC:
      occupancy_sensor_config.occupancy_sensing.feature_flags = passive_infrared::get_id() | ultrasonic::get_id();
      break;
    case OCCUPANCY_SENSOR_TYPE_PHYSICAL_CONTACT: occupancy_sensor_config.occupancy_sensing.feature_flags = physical_contact::get_id(); break;
    default:
      occupancy_sensor_config.occupancy_sensing.feature_flags = other::get_id();
      break;
  }

  endpoint_t *endpoint = occupancy_sensor::create(node::get(), &occupancy_sensor_config, ENDPOINT_FLAG_NONE, (void *)this);
  if (endpoint == nullptr) {
    log_e("Failed to create Occupancy Sensor endpoint");
    return false;
  }
  setEndPointId(endpoint::get_id(endpoint));

  occupancyState = _occupancyState;

  log_i("Occupancy Sensor created with endpoint_id %u", getEndPointId());

  started = true;
  return true;
}

void MatterOccupancySensor::end() {
  started = false;
}

bool MatterOccupancySensor::ensureHoldTimeAttributes() {
  if (getEndPointId() == 0) {
    log_e("Endpoint ID is not set");
    return false;
  }

  endpoint_t *ep = endpoint::get(node::get(), getEndPointId());
  cluster_t *cluster = (ep != nullptr) ? cluster::get(ep, OccupancySensing::Id) : nullptr;
  if (cluster == nullptr) {
    log_e("Failed to get Occupancy Sensing cluster");
    return false;
  }

  if (esp_matter::attribute::get(cluster, OccupancySensing::Attributes::HoldTime::Id) != nullptr) {
    return true;
  }

  if (ArduinoMatter::isStackStarted()) {
    log_e("HoldTime must be enabled before Matter.begin(). Call setHoldTime() or setHoldTimeLimits() after begin() and before Matter.begin().");
    return false;
  }

  const uint16_t initialHold = holdTime_seconds > 0 ? holdTime_seconds : 1;
  if (create_hold_time(cluster, initialHold) == nullptr) {
    log_e("Failed to create HoldTime attribute");
    return false;
  }
  if (create_hold_time_limits(cluster, NULL, 0, 0) == nullptr) {
    log_e("Failed to create HoldTimeLimits attribute");
    return false;
  }
  log_d("HoldTime attribute created");
  return true;
}

void MatterOccupancySensor::onStackStarted() {
  OccupancySensingCluster *cluster = static_cast<OccupancySensingCluster *>(findRegisteredCluster(OccupancySensing::Id));
  if (cluster == nullptr) {
    log_e("OccupancySensing cluster not found after Matter.begin().");
    return;
  }

  lock::ScopedChipStackLock lock(portMAX_DELAY);
  if (cluster->IsHoldTimeEnabled()) {
    if (holdTimeMax_seconds > 0) {
      OccupancySensing::Structs::HoldTimeLimitsStruct::Type limits;
      limits.holdTimeMin = holdTimeMin_seconds;
      limits.holdTimeMax = holdTimeMax_seconds;
      limits.holdTimeDefault = holdTimeDefault_seconds;
      cluster->SetHoldTimeLimits(limits);
      const auto &applied = cluster->GetHoldTimeLimits();
      holdTimeMin_seconds = applied.holdTimeMin;
      holdTimeMax_seconds = applied.holdTimeMax;
      holdTimeDefault_seconds = applied.holdTimeDefault;
    }
    if (holdTime_seconds > 0) {
      const auto status = cluster->SetHoldTime(holdTime_seconds);
      if (!status.IsSuccess()) {
        log_w("Failed to apply cached HoldTime %u; using cluster value.", holdTime_seconds);
        holdTime_seconds = cluster->GetHoldTime();
      }
    } else {
      holdTime_seconds = cluster->GetHoldTime();
    }
  }
  if (cluster->IsOccupied() != occupancyState) {
    cluster->SetOccupancy(occupancyState);
  }
}

bool MatterOccupancySensor::setOccupancy(bool _occupancyState) {
  if (!started) {
    log_e("Matter Occupancy Sensor device has not begun.");
    return false;
  }

  // CHIP SetOccupancy(false) restarts the HoldTime timer on every call.
  // Skip no-ops so loop() polling does not keep the hub occupied forever.
  if (occupancyState == _occupancyState) {
    return true;
  }

  occupancyState = _occupancyState;

  OccupancySensingCluster *cluster = static_cast<OccupancySensingCluster *>(findRegisteredCluster(OccupancySensing::Id));
  if (cluster == nullptr) {
    return true;
  }

  lock::ScopedChipStackLock lock(portMAX_DELAY);
  // With HoldTime enabled, SetOccupancy(false) starts the hold timer rather than
  // going vacant immediately. occupancyState tracks the last requested reading.
  cluster->SetOccupancy(_occupancyState);
  log_v("Occupancy Sensor set to %s", _occupancyState ? "Occupied" : "Vacant");
  return true;
}

bool MatterOccupancySensor::isOccupied() {
  OccupancySensingCluster *cluster = static_cast<OccupancySensingCluster *>(findRegisteredCluster(OccupancySensing::Id));
  if (cluster == nullptr) {
    return occupancyState;
  }
  lock::ScopedChipStackLock lock(portMAX_DELAY);
  return cluster->IsOccupied();
}

bool MatterOccupancySensor::setHoldTime(uint16_t _holdTime_seconds) {
  if (!started) {
    log_e("Matter Occupancy Sensor device has not begun.");
    return false;
  }

  if (getEndPointId() == 0) {
    log_e("Endpoint ID is not set");
    return false;
  }

  if (holdTime_seconds == _holdTime_seconds) {
    return true;
  }

  if (holdTimeMax_seconds > 0) {
    if (_holdTime_seconds < holdTimeMin_seconds) {
      log_e("HoldTime (%u) is below minimum (%u seconds)", _holdTime_seconds, holdTimeMin_seconds);
      return false;
    }
    if (_holdTime_seconds > holdTimeMax_seconds) {
      log_e("HoldTime (%u) exceeds maximum (%u seconds)", _holdTime_seconds, holdTimeMax_seconds);
      return false;
    }
  }

  if (_holdTime_seconds == 0) {
    log_e("HoldTime 0 is not allowed. Use a value of at least 1 second.");
    return false;
  }

  if (!ensureHoldTimeAttributes()) {
    return false;
  }

  OccupancySensingCluster *cluster = static_cast<OccupancySensingCluster *>(findRegisteredCluster(OccupancySensing::Id));
  if (cluster == nullptr) {
    holdTime_seconds = _holdTime_seconds;
    return true;
  }

  if (!cluster->IsHoldTimeEnabled()) {
    log_e("HoldTime is not enabled on the OccupancySensing cluster.");
    return false;
  }

  lock::ScopedChipStackLock lock(portMAX_DELAY);
  const auto status = cluster->SetHoldTime(_holdTime_seconds);
  if (!status.IsSuccess()) {
    log_e("Failed to set HoldTime to %u seconds.", _holdTime_seconds);
    return false;
  }

  holdTime_seconds = _holdTime_seconds;
  log_v("HoldTime set to %u seconds", _holdTime_seconds);
  return true;
}

bool MatterOccupancySensor::setHoldTimeLimits(uint16_t _holdTimeMin_seconds, uint16_t _holdTimeMax_seconds, uint16_t _holdTimeDefault_seconds) {
  if (!started) {
    log_e("Matter Occupancy Sensor device has not begun.");
    return false;
  }

  if (getEndPointId() == 0) {
    log_e("Endpoint ID is not set");
    return false;
  }

  // CHIP OccupancySensingCluster::SetHoldTimeLimits sanitizes the same way.
  const uint16_t holdTimeMin = std::max(static_cast<uint16_t>(1), _holdTimeMin_seconds);
  const uint16_t holdTimeMax =
    std::max({static_cast<uint16_t>(10), holdTimeMin, _holdTimeMax_seconds});
  const uint16_t holdTimeDefault = std::clamp(_holdTimeDefault_seconds, holdTimeMin, holdTimeMax);
  if (holdTimeMin != _holdTimeMin_seconds || holdTimeMax != _holdTimeMax_seconds || holdTimeDefault != _holdTimeDefault_seconds) {
    log_i(
      "HoldTimeLimits coerced to CHIP range: Min=%u (was %u), Max=%u (was %u), Default=%u (was %u)", holdTimeMin, _holdTimeMin_seconds, holdTimeMax,
      _holdTimeMax_seconds, holdTimeDefault, _holdTimeDefault_seconds
    );
  }

  uint16_t adjustedHoldTime = holdTime_seconds;
  if (holdTime_seconds > 0 && holdTime_seconds < holdTimeMin) {
    adjustedHoldTime = holdTimeMin;
    log_i("Current HoldTime (%u) is below new minimum (%u), adjusting to minimum", holdTime_seconds, holdTimeMin);
  } else if (holdTime_seconds > holdTimeMax) {
    adjustedHoldTime = holdTimeMax;
    log_i("Current HoldTime (%u) exceeds new maximum (%u), adjusting to maximum", holdTime_seconds, holdTimeMax);
  }

  if (!ensureHoldTimeAttributes()) {
    return false;
  }

  OccupancySensingCluster *cluster = static_cast<OccupancySensingCluster *>(findRegisteredCluster(OccupancySensing::Id));
  if (cluster == nullptr) {
    holdTimeMin_seconds = holdTimeMin;
    holdTimeMax_seconds = holdTimeMax;
    holdTimeDefault_seconds = holdTimeDefault;
    holdTime_seconds = adjustedHoldTime;
    return true;
  }

  if (!cluster->IsHoldTimeEnabled()) {
    log_e("HoldTime is not enabled on the OccupancySensing cluster.");
    return false;
  }

  lock::ScopedChipStackLock lock(portMAX_DELAY);
  OccupancySensing::Structs::HoldTimeLimitsStruct::Type limits;
  limits.holdTimeMin = holdTimeMin;
  limits.holdTimeMax = holdTimeMax;
  limits.holdTimeDefault = holdTimeDefault;
  cluster->SetHoldTimeLimits(limits);
  const auto &applied = cluster->GetHoldTimeLimits();
  holdTimeMin_seconds = applied.holdTimeMin;
  holdTimeMax_seconds = applied.holdTimeMax;
  holdTimeDefault_seconds = applied.holdTimeDefault;

  if (adjustedHoldTime > 0) {
    const auto status = cluster->SetHoldTime(adjustedHoldTime);
    if (!status.IsSuccess()) {
      log_e("Failed to set HoldTime to %u after updating limits.", adjustedHoldTime);
      return false;
    }
  }

  holdTime_seconds = adjustedHoldTime;

  log_v("HoldTimeLimits set: Min=%u, Max=%u, Default=%u seconds", holdTimeMin_seconds, holdTimeMax_seconds, holdTimeDefault_seconds);
  return true;
}

#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
