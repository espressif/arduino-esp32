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
#include <app/server/Server.h>
#include <MatterEndpoints/MatterSoilSensor.h>
#include <app/clusters/soil-measurement-server/soil-measurement-cluster.h>
#include <clusters/SoilMeasurement/Attributes.h>
#include <app/ClusterCallbacks.h>
#include <app/PluginApplicationCallbacks.h>
#include <data_model_provider/esp_matter_data_model_provider.h>
#include <unordered_map>

using namespace esp_matter;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

// Soil Sensor device type (0x0045) - not yet defined by esp_matter_endpoint.h.
static constexpr uint32_t kSoilSensorDeviceTypeId = 0x0045;
static constexpr uint8_t kSoilSensorDeviceTypeVersion = 1;

namespace {
// Soil Measurement (cluster 0x0430) has no ember-native implementation: it only exists as a
// code-driven cluster, the same family as BooleanState/ValveConfigurationAndControl. esp_matter
// doesn't (yet) ship a esp_matter::cluster::soil_measurement convenience wrapper to register it, so
// this translation unit plugs it in itself - the same technique esp_matter's own
// data_model_provider/clusters/boolean_state_integration.cpp uses for BooleanState.
std::unordered_map<chip::EndpointId, chip::app::LazyRegisteredServerCluster<SoilMeasurementCluster>> gSoilServers;
}  // namespace

// Registered below as the Soil Measurement cluster's init callback - invoked once when the endpoint is
// attached to the Matter node, constructing and registering the live SoilMeasurementCluster instance
// that actually backs SoilMoistureMeasuredValue/SoilMoistureMeasurementLimits.
void ESPMatterSoilMeasurementClusterServerInitCallback(chip::EndpointId endpoint_id) {
  if (gSoilServers[endpoint_id].IsConstructed()) {
    return;
  }

  // SoilMoistureMeasurementLimits is mandatory and fixed for the lifetime of the cluster: report the
  // full [0..100] percent range this endpoint accepts, with no additional accuracy ranges.
  SoilMeasurement::Attributes::SoilMoistureMeasurementLimits::TypeInfo::Type limits{};
  limits.measurementType = Globals::MeasurementTypeEnum::kSoilMoisture;
  limits.measured = true;
  limits.minMeasuredValue = 0;
  limits.maxMeasuredValue = 100;

  gSoilServers[endpoint_id].Create(endpoint_id, limits);
  CHIP_ERROR err = esp_matter::data_model::provider::get_instance().registry().Register(gSoilServers[endpoint_id].Registration());
  if (err != CHIP_NO_ERROR) {
    log_e("Failed to register Soil Measurement cluster on endpoint %u", endpoint_id);
  }
}

void ESPMatterSoilMeasurementClusterServerShutdownCallback(chip::EndpointId endpoint_id) {
  auto it = gSoilServers.find(endpoint_id);
  if (it == gSoilServers.end() || !it->second.IsConstructed()) {
    return;
  }
  esp_matter::data_model::provider::get_instance().registry().Unregister(&it->second.Cluster());
  it->second.Destroy();
  gSoilServers.erase(it);
}

static SoilMeasurementCluster *getSoilMeasurementCluster(uint16_t endpoint_id) {
  chip::app::ServerClusterInterface *iface =
    esp_matter::data_model::provider::get_instance().registry().Get(chip::app::ConcreteClusterPath(endpoint_id, SoilMeasurement::Id));
  return static_cast<SoilMeasurementCluster *>(iface);
}

bool MatterSoilSensor::attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) {
  bool ret = true;
  if (!started) {
    log_e("Matter Soil Sensor device has not begun.");
    return false;
  }

  log_d(
    "Soil Sensor Attr update callback: endpoint: %u, cluster: %" PRIu32 ", attribute: %" PRIu32 ", val: %" PRIu32, endpoint_id, cluster_id, attribute_id,
    val->val.u32
  );
  return ret;
}

MatterSoilSensor::MatterSoilSensor() {}

MatterSoilSensor::~MatterSoilSensor() {
  end();
}

bool MatterSoilSensor::begin() {
  ArduinoMatter::_init();

  if (getEndPointId() != 0) {
    log_e("Matter Soil Sensor with Endpoint Id %u device has already been created.", getEndPointId());
    return false;
  }

  // No esp_matter::endpoint::soil_sensor helper exists yet, so the endpoint is assembled from the
  // generic building blocks by hand - the same steps such a helper would take: a bare endpoint, the
  // mandatory Descriptor and Identify clusters, the device type, and finally the Soil Measurement
  // cluster itself (registered above, not backed by ember attributes).
  endpoint_t *endpoint = endpoint::create(node::get(), ENDPOINT_FLAG_NONE, (void *)this);
  if (endpoint == nullptr) {
    log_e("Failed to create Soil Sensor endpoint");
    return false;
  }

  cluster::descriptor::config_t descriptor_config;
  if (cluster::descriptor::create(endpoint, &descriptor_config, CLUSTER_FLAG_SERVER) == nullptr) {
    log_e("Failed to create Descriptor cluster");
    return false;
  }

  cluster::identify::config_t identify_config;
  identify_config.identify_type = chip::to_underlying(Identify::IdentifyTypeEnum::kVisibleIndicator);
  if (cluster::identify::create(endpoint, &identify_config, CLUSTER_FLAG_SERVER) == nullptr) {
    log_e("Failed to create Identify cluster");
    return false;
  }

  if (add_device_type(endpoint, kSoilSensorDeviceTypeId, kSoilSensorDeviceTypeVersion) != ESP_OK) {
    log_e("Failed to add Soil Sensor device type");
    return false;
  }

  cluster_t *soil_cluster = cluster::create(endpoint, SoilMeasurement::Id, CLUSTER_FLAG_SERVER);
  if (soil_cluster == nullptr) {
    log_e("Failed to create Soil Measurement cluster");
    return false;
  }
  cluster::set_plugin_server_init_callback(soil_cluster, MatterSoilMeasurementPluginServerInitCallback);
  cluster::set_init_and_shutdown_callbacks(soil_cluster, ESPMatterSoilMeasurementClusterServerInitCallback, ESPMatterSoilMeasurementClusterServerShutdownCallback);

  setEndPointId(endpoint::get_id(endpoint));
  log_i("Soil Sensor created with endpoint_id %u", getEndPointId());

  // The live SoilMeasurementCluster isn't registered until the Matter stack starts (see
  // ESPMatterSoilMeasurementClusterServerInitCallback above), so the initial SoilMoistureMeasuredValue
  // stays null until setSoilMoisture() is called after Matter.begin() - same rule as the Boolean State
  // sensors (see README.md).
  started = true;
  soilMoisture = 0;

  return true;
}

void MatterSoilSensor::end() {
  started = false;
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

  // avoid processing if there was no change
  if (soilMoisture == soilMoisturePercent) {
    return true;
  }

  SoilMeasurementCluster *cluster = getSoilMeasurementCluster(getEndPointId());
  if (cluster == nullptr) {
    log_e("Soil Measurement cluster not found on endpoint %u. Call Matter.begin() first.", getEndPointId());
    return false;
  }

  lock::ScopedChipStackLock lock(portMAX_DELAY);
  chip::app::DataModel::Nullable<chip::Percent> val(soilMoisturePercent);
  if (cluster->SetSoilMoistureMeasuredValue(val) != CHIP_NO_ERROR) {
    log_e("Failed to update Soil Sensor moisture value.");
    return false;
  }
  soilMoisture = soilMoisturePercent;
  log_v("Soil Sensor set to %u Percent", soilMoisturePercent);

  return true;
}

#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
