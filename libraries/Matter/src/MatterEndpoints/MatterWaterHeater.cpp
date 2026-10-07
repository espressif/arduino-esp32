// Copyright 2025 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
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
#include <MatterEndpoints/MatterWaterHeater.h>
#include <esp_matter.h>
#include <esp_matter_core.h>
#ifdef CONFIG_ESP_MATTER_ENABLE_GENERATED_DATA_MODEL
#include <thermostat.h>
#include <water_heater_management.h>
#endif
#include <app-common/zap-generated/cluster-enums.h>
#include <app/clusters/mode-base-server/CodegenIntegration.h>
#include <app/clusters/water-heater-management-server/Delegate.h>
#include <app/reporting/reporting.h>
#include <lib/support/Span.h>
#include <platform/CHIPDeviceLayer.h>
#include <system/SystemLayer.h>

#ifndef CONFIG_ESP_MATTER_ENABLE_GENERATED_DATA_MODEL
// Pre-Matter.begin() FeatureMap writes must skip attribute callbacks (legacy update_feature_map()).
namespace esp_matter {
namespace attribute {
esp_err_t get_val_internal(attribute_t *attribute, esp_matter_attr_val_t *val);
esp_err_t set_val_internal(attribute_t *attribute, esp_matter_attr_val_t *val, bool call_callbacks);
}  // namespace attribute
}  // namespace esp_matter
#endif

using namespace esp_matter;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

namespace {
constexpr int16_t DEFAULT_LOCAL_TEMPERATURE = 2000;
constexpr int16_t DEFAULT_HEATING_SETPOINT = 4800;

constexpr int16_t ABS_MIN_HEATING_SETPOINT = 2000;
constexpr int16_t MIN_HEATING_SETPOINT = 2000;
constexpr int16_t ABS_MAX_HEATING_SETPOINT = 8500;
constexpr int16_t MAX_HEATING_SETPOINT = 8500;

constexpr uint8_t DEFAULT_SYSTEM_MODE = static_cast<uint8_t>(Thermostat::SystemModeEnum::kHeat);
constexpr uint8_t DEFAULT_HEATER_TYPES = MatterWaterHeater::IMMERSION_ELEMENT_1;

constexpr uint8_t DEFAULT_HEAT_DEMAND = 0;
constexpr uint16_t DEFAULT_TANK_VOLUME = 100;
constexpr uint8_t DEFAULT_TANK_PERCENTAGE = 100;
constexpr int64_t DEFAULT_ESTIMATED_HEAT_REQUIRED = 0;
constexpr uint8_t DEFAULT_BOOST_STATE = MatterWaterHeater::BOOST_INACTIVE;

constexpr uint8_t DEFAULT_WATER_HEATER_MODE = MatterWaterHeater::WATER_HEATER_MODE_MANUAL;
}  // namespace

namespace {
constexpr uint8_t kModeCount = 3;

struct ModeOption {
  const char *label;
  uint8_t value;
  chip::app::Clusters::ModeBase::ModeTag tag;
};

// Mode values match MatterWaterHeater::WaterHeaterMode_t. Tags are spec ModeTag values.
constexpr ModeOption kModes[kModeCount] = {
  {"Off", MatterWaterHeater::WATER_HEATER_MODE_OFF, chip::app::Clusters::ModeBase::ModeTag::kMin},
  {"Manual", MatterWaterHeater::WATER_HEATER_MODE_MANUAL, chip::app::Clusters::ModeBase::ModeTag::kDay},
  {"Eco", MatterWaterHeater::WATER_HEATER_MODE_ECO, chip::app::Clusters::ModeBase::ModeTag::kLowEnergy},
};

#ifndef CONFIG_ESP_MATTER_ENABLE_GENERATED_DATA_MODEL
// Legacy energy_management::add(cluster_t*) is declared but not linked. Create the
// optional WHM attributes here and write FeatureMap without pre-update callbacks.
bool orWhmFeatureMap(cluster_t *cluster, uint32_t feature_bit) {
  esp_matter_attr_val_t feature_map_val = esp_matter_invalid(NULL);
  attribute_t *feature_map_attr = attribute::get(cluster, Globals::Attributes::FeatureMap::Id);
  if (feature_map_attr == nullptr || ::esp_matter::attribute::get_val_internal(feature_map_attr, &feature_map_val) != ESP_OK) {
    return false;
  }
  feature_map_val.val.u32 |= feature_bit;
  return ::esp_matter::attribute::set_val_internal(feature_map_attr, &feature_map_val, false) == ESP_OK;
}

bool addLegacyWhmEnergyManagement(cluster_t *cluster) {
  if (!orWhmFeatureMap(cluster, static_cast<uint32_t>(WaterHeaterManagement::Feature::kEnergyManagement))) {
    return false;
  }
  return ::esp_matter::cluster::water_heater_management::attribute::create_tank_volume(cluster, 0) != nullptr &&
         ::esp_matter::cluster::water_heater_management::attribute::create_estimated_heat_required(cluster, 0) != nullptr;
}

bool addLegacyWhmTankPercent(cluster_t *cluster) {
  if (::esp_matter::cluster::water_heater_management::feature::tank_percent::add(cluster) != ESP_OK) {
    return false;
  }
  // tank_percent::get_id() wrongly returns kEnergyManagement in legacy esp_matter_feature.cpp.
  return orWhmFeatureMap(cluster, static_cast<uint32_t>(WaterHeaterManagement::Feature::kTankPercent));
}
#endif

// WHM attributes are code-driven (Delegate). Ember get/update fails (GET_VAL / 262).
void reportWhmAttribute(uint16_t endpointId, chip::AttributeId attributeId) {
  if (endpointId == 0 || !chip::DeviceLayer::SystemLayer().IsInitialized()) {
    return;
  }
  const CHIP_ERROR err = chip::DeviceLayer::SystemLayer().ScheduleLambda([endpointId, attributeId]() {
    MatterReportingAttributeChangeCallback(endpointId, WaterHeaterManagement::Id, attributeId);
  });
  if (err != CHIP_NO_ERROR) {
    log_v("Failed to schedule WHM attribute report: %" CHIP_ERROR_FORMAT, err.Format());
  }
}
}  // namespace

// Bridges Water Heater Mode to CHIP ModeBase. Methods run on the Matter event loop (Init / ChangeToMode)
// or under ScopedChipStackLock from setWaterHeaterMode().
class MatterWaterHeater::ModeDelegate : public chip::app::Clusters::ModeBase::Delegate {
public:
  explicit ModeDelegate(MatterWaterHeater *owner) : owner(owner) {}

  chip::app::Clusters::ModeBase::Instance *instance() const {
    return const_cast<chip::app::Clusters::ModeBase::Instance *>(GetInstance());
  }

  CHIP_ERROR Init() override {
    chip::app::Clusters::ModeBase::Instance *modeInstance = instance();
    if (modeInstance == nullptr) {
      return CHIP_NO_ERROR;
    }
    if (owner->waterHeaterModeSetByApp) {
      modeInstance->UpdateCurrentMode(owner->waterHeaterMode);
    } else {
      owner->waterHeaterMode = modeInstance->GetCurrentMode();
    }
    return CHIP_NO_ERROR;
  }

  CHIP_ERROR GetModeLabelByIndex(uint8_t modeIndex, chip::MutableCharSpan &label) override {
    if (modeIndex >= kModeCount) {
      return CHIP_ERROR_PROVIDER_LIST_EXHAUSTED;
    }
    return chip::CopyCharSpanToMutableCharSpan(chip::CharSpan::fromCharString(kModes[modeIndex].label), label);
  }

  CHIP_ERROR GetModeValueByIndex(uint8_t modeIndex, uint8_t &value) override {
    if (modeIndex >= kModeCount) {
      return CHIP_ERROR_PROVIDER_LIST_EXHAUSTED;
    }
    value = kModes[modeIndex].value;
    return CHIP_NO_ERROR;
  }

  CHIP_ERROR GetModeTagsByIndex(uint8_t modeIndex, chip::app::DataModel::List<chip::app::Clusters::detail::Structs::ModeTagStruct::Type> &modeTags) override {
    if (modeIndex >= kModeCount) {
      return CHIP_ERROR_PROVIDER_LIST_EXHAUSTED;
    }
    if (modeTags.size() < 1) {
      return CHIP_ERROR_BUFFER_TOO_SMALL;
    }
    modeTags[0].value = static_cast<uint16_t>(kModes[modeIndex].tag);
    modeTags.reduce_size(1);
    return CHIP_NO_ERROR;
  }

  void HandleChangeToMode(uint8_t newMode, chip::app::Clusters::ModeBase::Commands::ChangeToModeResponse::Type &response) override {
    owner->waterHeaterMode = newMode;
    response.status = static_cast<uint8_t>(chip::app::Clusters::ModeBase::StatusCode::kSuccess);
  }

private:
  MatterWaterHeater *owner;
};

// Bridges Boost/CancelBoost and WHM attribute reads to the Arduino cache. CHIP
// WaterHeaterManagementCluster is created only when this delegate is set on create().
class MatterWaterHeater::ManagementDelegate : public chip::app::Clusters::WaterHeaterManagement::Delegate {
public:
  explicit ManagementDelegate(MatterWaterHeater *owner) : owner(owner) {}

  chip::Protocols::InteractionModel::Status HandleBoost(
    uint32_t duration, chip::Optional<bool> oneShot, chip::Optional<bool> emergencyBoost, chip::Optional<int16_t> temporarySetpoint,
    chip::Optional<chip::Percent> targetPercentage, chip::Optional<chip::Percent> targetReheat
  ) override {
    (void)emergencyBoost;
    (void)targetReheat;

    if (temporarySetpoint.HasValue()) {
      const int16_t requested = temporarySetpoint.Value();
      if (requested < owner->minimumHeatingSetpoint || requested > owner->maximumHeatingSetpoint) {
        return chip::Protocols::InteractionModel::Status::ConstraintError;
      }
    }
    if (targetPercentage.HasValue() && targetPercentage.Value() > 100) {
      return chip::Protocols::InteractionModel::Status::ConstraintError;
    }

    if (temporarySetpoint.HasValue()) {
      if (!owner->boostHasTemporarySetpoint) {
        owner->boostSavedHeatingSetpoint = owner->heatingSetpoint;
        owner->boostHasTemporarySetpoint = true;
      }
      if (!owner->setHeatingSetpointRaw(temporarySetpoint.Value())) {
        return chip::Protocols::InteractionModel::Status::ConstraintError;
      }
    } else if (owner->boostHasTemporarySetpoint) {
      (void)owner->setHeatingSetpointRaw(owner->boostSavedHeatingSetpoint);
      owner->boostHasTemporarySetpoint = false;
    }

    if (!owner->applyBoostState(MatterWaterHeater::BOOST_ACTIVE)) {
      return chip::Protocols::InteractionModel::Status::Failure;
    }

    owner->boostOneShot = oneShot.ValueOr(false);
    owner->boostHasTargetPercentage = targetPercentage.HasValue();
    owner->boostTargetPercentage = targetPercentage.ValueOr(0);
    owner->armBoostTimer(duration);
    (void)GenerateBoostStartedEvent(duration, oneShot, emergencyBoost, temporarySetpoint, targetPercentage, targetReheat);
    owner->maybeFinishOneShotBoost();
    return chip::Protocols::InteractionModel::Status::Success;
  }

  chip::Protocols::InteractionModel::Status HandleCancelBoost() override {
    if (owner->boostState == MatterWaterHeater::BOOST_INACTIVE && !owner->boostTimerArmed) {
      return chip::Protocols::InteractionModel::Status::Success;
    }
    owner->finishBoost(true);
    return chip::Protocols::InteractionModel::Status::Success;
  }

  chip::BitMask<chip::app::Clusters::WaterHeaterManagement::WaterHeaterHeatSourceBitmap> GetHeaterTypes() override {
    return chip::BitMask<chip::app::Clusters::WaterHeaterManagement::WaterHeaterHeatSourceBitmap>(owner->heaterTypes);
  }

  chip::BitMask<chip::app::Clusters::WaterHeaterManagement::WaterHeaterHeatSourceBitmap> GetHeatDemand() override {
    return chip::BitMask<chip::app::Clusters::WaterHeaterManagement::WaterHeaterHeatSourceBitmap>(owner->heatDemand);
  }

  uint16_t GetTankVolume() override {
    return owner->tankVolume;
  }

  chip::Energy_mWh GetEstimatedHeatRequired() override {
    return owner->estimatedHeatRequired;
  }

  chip::Percent GetTankPercentage() override {
    return owner->tankPercentage;
  }

  chip::app::Clusters::WaterHeaterManagement::BoostStateEnum GetBoostState() override {
    return static_cast<chip::app::Clusters::WaterHeaterManagement::BoostStateEnum>(owner->boostState);
  }

private:
  MatterWaterHeater *owner;
};

MatterWaterHeater::MatterWaterHeater() {}

MatterWaterHeater::~MatterWaterHeater() {
  end();
}

bool MatterWaterHeater::begin() {
  if (initialized) {
    return false;
  }

  ensureMatterNode();

  if (getEndPointId() != 0) {
    log_e("MatterWaterHeater already has endpoint %u; end() after Matter.begin() does not destroy it.", getEndPointId());
    return false;
  }

  if (ArduinoMatter::isStackStarted()) {
    log_e("MatterWaterHeater::begin() must be called before Matter.begin() to provision optional WHM features.");
    return false;
  }

  // Water Heater Management
  esp_matter::cluster::water_heater_management::config_t management_config;
  management_config.heater_types = DEFAULT_HEATER_TYPES;
  management_config.heat_demand = DEFAULT_HEAT_DEMAND;
  management_config.boost_state = DEFAULT_BOOST_STATE;

  // Water Heater Mode is a CHIP mode-base server. The delegate must be set before create() so
  // WaterHeaterModeDelegateInitCB builds ModeBase::Instance at stack start.
  modeDelegate = new (std::nothrow) ModeDelegate(this);
  managementDelegate = new (std::nothrow) ManagementDelegate(this);
  if (modeDelegate == nullptr || managementDelegate == nullptr) {
    log_e("Failed to allocate Water Heater delegates");
    delete modeDelegate;
    delete managementDelegate;
    modeDelegate = nullptr;
    managementDelegate = nullptr;
    return false;
  }
  esp_matter::cluster::water_heater_mode::config_t mode_config;
  mode_config.delegate = modeDelegate;
  management_config.delegate = managementDelegate;

  // Thermostat. This is the endpoint's thermostat configuration, not the cluster namespace -
  // use the fully-qualified name to avoid the endpoint::thermostat / cluster::thermostat ambiguity.
  esp_matter::cluster::thermostat::config_t thermostat_config;
  thermostat_config.local_temperature = DEFAULT_LOCAL_TEMPERATURE;
  thermostat_config.control_sequence_of_operation = static_cast<uint8_t>(Thermostat::ControlSequenceOfOperationEnum::kHeatingOnly);
  thermostat_config.system_mode = DEFAULT_SYSTEM_MODE;
  thermostat_config.features.heating.occupied_heating_setpoint = DEFAULT_HEATING_SETPOINT;
  thermostat_config.feature_flags |= esp_matter::cluster::thermostat::feature::heating::get_id();

  // Water Heater device type 0x050F. The generated device type creates Water Heater Management,
  // Water Heater Mode and Thermostat on the same endpoint.
  esp_matter::endpoint::water_heater::config_t water_heater_config;
  water_heater_config.water_heater_management = management_config;
  water_heater_config.water_heater_mode = mode_config;
  water_heater_config.thermostat = thermostat_config;

  endpoint_t *endpoint = esp_matter::endpoint::water_heater::create(node::get(), &water_heater_config, ENDPOINT_FLAG_DESTROYABLE, this);
  if (endpoint == nullptr) {
    delete modeDelegate;
    delete managementDelegate;
    modeDelegate = nullptr;
    managementDelegate = nullptr;
    return false;
  }

  cluster_t *management_cluster = cluster::get(endpoint, WaterHeaterManagement::Id);
  if (management_cluster == nullptr) {
    log_e("Water Heater Management cluster missing after endpoint create");
    esp_matter::endpoint::destroy(node::get(), endpoint);
    delete modeDelegate;
    delete managementDelegate;
    modeDelegate = nullptr;
    managementDelegate = nullptr;
    return false;
  }

  // water_heater::create() only adds OccupiedHeatingSetpoint (Heating feature). Controllers
  // that expose a heating range need AbsMin/AbsMaxHeatSetpointLimit.
  cluster_t *thermostat_cluster = cluster::get(endpoint, Thermostat::Id);
  if (thermostat_cluster == nullptr) {
    log_e("Thermostat cluster missing after endpoint create");
    esp_matter::endpoint::destroy(node::get(), endpoint);
    delete modeDelegate;
    delete managementDelegate;
    modeDelegate = nullptr;
    managementDelegate = nullptr;
    return false;
  }
  if (cluster::thermostat::attribute::create_abs_min_heat_setpoint_limit(thermostat_cluster, ABS_MIN_HEATING_SETPOINT) == nullptr
      || cluster::thermostat::attribute::create_abs_max_heat_setpoint_limit(thermostat_cluster, ABS_MAX_HEATING_SETPOINT) == nullptr
      || cluster::thermostat::attribute::create_min_heat_setpoint_limit(thermostat_cluster, MIN_HEATING_SETPOINT) == nullptr
      || cluster::thermostat::attribute::create_max_heat_setpoint_limit(thermostat_cluster, MAX_HEATING_SETPOINT) == nullptr) {
    log_e("Failed to add Thermostat heating setpoint limits");
    esp_matter::endpoint::destroy(node::get(), endpoint);
    delete modeDelegate;
    delete managementDelegate;
    modeDelegate = nullptr;
    managementDelegate = nullptr;
    return false;
  }

  // water_heater::create() only adds mandatory WHM attributes; TankVolume / TankPercentage need
  // EnergyManagement and TankPercent features before setTankVolume() / setTankPercentage().
#ifdef CONFIG_ESP_MATTER_ENABLE_GENERATED_DATA_MODEL
  esp_matter::cluster::water_heater_management::feature::energy_management::config_t energy_management_config;
  energy_management_config.tank_volume = DEFAULT_TANK_VOLUME;
  energy_management_config.estimated_heat_required = 0;
  if (esp_matter::cluster::water_heater_management::feature::energy_management::add(management_cluster, &energy_management_config) != ESP_OK) {
    log_e("Failed to add Water Heater Management EnergyManagement feature");
    esp_matter::endpoint::destroy(node::get(), endpoint);
    delete modeDelegate;
    delete managementDelegate;
    modeDelegate = nullptr;
    managementDelegate = nullptr;
    return false;
  }

  esp_matter::cluster::water_heater_management::feature::tank_percent::config_t tank_percent_config;
  tank_percent_config.tank_percentage = DEFAULT_TANK_PERCENTAGE;
  if (esp_matter::cluster::water_heater_management::feature::tank_percent::add(management_cluster, &tank_percent_config) != ESP_OK) {
    log_e("Failed to add Water Heater Management TankPercent feature");
    esp_matter::endpoint::destroy(node::get(), endpoint);
    delete modeDelegate;
    delete managementDelegate;
    modeDelegate = nullptr;
    managementDelegate = nullptr;
    return false;
  }
#else
  if (!addLegacyWhmEnergyManagement(management_cluster)) {
    log_e("Failed to add Water Heater Management EnergyManagement feature");
    esp_matter::endpoint::destroy(node::get(), endpoint);
    delete modeDelegate;
    delete managementDelegate;
    modeDelegate = nullptr;
    managementDelegate = nullptr;
    return false;
  }
  if (!addLegacyWhmTankPercent(management_cluster)) {
    log_e("Failed to add Water Heater Management TankPercent feature");
    esp_matter::endpoint::destroy(node::get(), endpoint);
    delete modeDelegate;
    delete managementDelegate;
    modeDelegate = nullptr;
    managementDelegate = nullptr;
    return false;
  }
#endif

  setEndPointId(endpoint::get_id(endpoint));
  initialized = true;

  // Initialize the local cache from the values configured above.
  localTemperature = DEFAULT_LOCAL_TEMPERATURE;
  heatingSetpoint = DEFAULT_HEATING_SETPOINT;

  absoluteMinimumHeatingSetpoint = ABS_MIN_HEATING_SETPOINT;
  minimumHeatingSetpoint = MIN_HEATING_SETPOINT;
  absoluteMaximumHeatingSetpoint = ABS_MAX_HEATING_SETPOINT;
  maximumHeatingSetpoint = MAX_HEATING_SETPOINT;

  systemMode = DEFAULT_SYSTEM_MODE;

  heaterTypes = DEFAULT_HEATER_TYPES;
  heatDemand = DEFAULT_HEAT_DEMAND;
  tankVolume = DEFAULT_TANK_VOLUME;
  tankPercentage = DEFAULT_TANK_PERCENTAGE;
  estimatedHeatRequired = DEFAULT_ESTIMATED_HEAT_REQUIRED;
  boostState = DEFAULT_BOOST_STATE;
  waterHeaterMode = DEFAULT_WATER_HEATER_MODE;
  // ModeBase defaults to the first supported mode (Off). Pin Manual so the stack Init()
  // does not overwrite the cache and leave the example tank frozen at cold water.
  setWaterHeaterMode(static_cast<WaterHeaterMode_t>(DEFAULT_WATER_HEATER_MODE));

  // Push initial tank values (attributes exist after feature::add above).
  setTankVolume(DEFAULT_TANK_VOLUME);
  setTankPercentage(DEFAULT_TANK_PERCENTAGE);
  setEstimatedHeatRequired(DEFAULT_ESTIMATED_HEAT_REQUIRED);
  syncHeatDemand();

  return true;
}

void MatterWaterHeater::end() {
  if (initialized) {
    finishBoost(false);
  } else {
    cancelBoostTimer();
  }

  if (!ArduinoMatter::isStackStarted()) {
    if (getEndPointId() != 0) {
      endpoint_t *ep = endpoint::get(node::get(), getEndPointId());
      if (ep != nullptr) {
        esp_matter::endpoint::destroy(node::get(), ep);
      }
      // setEndPointId(0) is rejected; clear so begin() can create a new endpoint.
      endpoint_id = 0;
    }
    delete modeDelegate;
    delete managementDelegate;
    modeDelegate = nullptr;
    managementDelegate = nullptr;
    initialized = false;
    return;
  }

  initialized = false;
  if (getEndPointId() != 0) {
    log_w(
      "MatterWaterHeater::end() after Matter.begin() leaves endpoint %u until reboot; begin() cannot recreate it.", getEndPointId()
    );
  }
}

/*
 * --------------------------------------------------------------------------
 * Thermostat / local temperature
 * --------------------------------------------------------------------------
 */

bool MatterWaterHeater::setLocalTemperature(float temperature) {
  return setLocalTemperatureRaw(static_cast<int16_t>(temperature * 100.0f));
}

float MatterWaterHeater::getLocalTemperature() {
  return static_cast<float>(localTemperature) / 100.0f;
}

bool MatterWaterHeater::setLocalTemperatureRaw(int16_t temperature) {
  if (!initialized) {
    return false;
  }

  esp_matter_attr_val_t value = esp_matter_invalid(NULL);
  if (!getAttributeVal(Thermostat::Id, Thermostat::Attributes::LocalTemperature::Id, &value)) {
    return false;
  }

  if (value.val.i16 == temperature) {
    localTemperature = temperature;
    maybeFinishOneShotBoost();
    return true;
  }

  value.val.i16 = temperature;
  if (!updateAttributeVal(Thermostat::Id, Thermostat::Attributes::LocalTemperature::Id, &value)) {
    return false;
  }

  localTemperature = temperature;
  maybeFinishOneShotBoost();
  return true;
}

int16_t MatterWaterHeater::getLocalTemperatureRaw() {
  return localTemperature;
}

/*
 * --------------------------------------------------------------------------
 * Heating setpoint
 * --------------------------------------------------------------------------
 */

bool MatterWaterHeater::setHeatingSetpoint(float temperature) {
  return setHeatingSetpointRaw(static_cast<int16_t>(temperature * 100.0f));
}

float MatterWaterHeater::getHeatingSetpoint() {
  return static_cast<float>(heatingSetpoint) / 100.0f;
}

bool MatterWaterHeater::setHeatingSetpointRaw(int16_t temperature) {
  if (!initialized) {
    return false;
  }

  if (temperature < minimumHeatingSetpoint || temperature > maximumHeatingSetpoint) {
    return false;
  }

  esp_matter_attr_val_t value = esp_matter_invalid(NULL);
  if (!getAttributeVal(Thermostat::Id, Thermostat::Attributes::OccupiedHeatingSetpoint::Id, &value)) {
    return false;
  }

  if (value.val.i16 == temperature) {
    heatingSetpoint = temperature;
    return true;
  }

  value.val.i16 = temperature;
  if (!updateAttributeVal(Thermostat::Id, Thermostat::Attributes::OccupiedHeatingSetpoint::Id, &value)) {
    return false;
  }

  heatingSetpoint = temperature;
  return true;
}

int16_t MatterWaterHeater::getHeatingSetpointRaw() {
  return heatingSetpoint;
}

/*
 * --------------------------------------------------------------------------
 * Heating setpoint limits
 * --------------------------------------------------------------------------
 */

bool MatterWaterHeater::setAbsoluteMinimumHeatingSetpoint(float temperature) {
  const int16_t value = static_cast<int16_t>(temperature * 100.0f);

  if (!initialized) {
    return false;
  }

  if (value < ABS_MIN_HEATING_SETPOINT || value > ABS_MAX_HEATING_SETPOINT) {
    return false;
  }

  esp_matter_attr_val_t attr = esp_matter_invalid(NULL);
  if (!getAttributeVal(Thermostat::Id, Thermostat::Attributes::AbsMinHeatSetpointLimit::Id, &attr)) {
    return false;
  }

  attr.val.i16 = value;
  if (!updateAttributeVal(Thermostat::Id, Thermostat::Attributes::AbsMinHeatSetpointLimit::Id, &attr)) {
    return false;
  }

  absoluteMinimumHeatingSetpoint = value;

  if (minimumHeatingSetpoint < value) {
    setMinimumHeatingSetpoint(static_cast<float>(value) / 100.0f);
  }

  return true;
}

bool MatterWaterHeater::setMinimumHeatingSetpoint(float temperature) {
  const int16_t value = static_cast<int16_t>(temperature * 100.0f);

  if (!initialized) {
    return false;
  }

  if (value < absoluteMinimumHeatingSetpoint || value > maximumHeatingSetpoint) {
    return false;
  }

  esp_matter_attr_val_t attr = esp_matter_invalid(NULL);
  if (!getAttributeVal(Thermostat::Id, Thermostat::Attributes::MinHeatSetpointLimit::Id, &attr)) {
    return false;
  }

  attr.val.i16 = value;
  if (!updateAttributeVal(Thermostat::Id, Thermostat::Attributes::MinHeatSetpointLimit::Id, &attr)) {
    return false;
  }

  minimumHeatingSetpoint = value;

  if (heatingSetpoint < value) {
    setHeatingSetpointRaw(value);
  }

  return true;
}

bool MatterWaterHeater::setAbsoluteMaximumHeatingSetpoint(float temperature) {
  const int16_t value = static_cast<int16_t>(temperature * 100.0f);

  if (!initialized) {
    return false;
  }

  if (value < ABS_MIN_HEATING_SETPOINT || value > ABS_MAX_HEATING_SETPOINT) {
    return false;
  }

  esp_matter_attr_val_t attr = esp_matter_invalid(NULL);
  if (!getAttributeVal(Thermostat::Id, Thermostat::Attributes::AbsMaxHeatSetpointLimit::Id, &attr)) {
    return false;
  }

  attr.val.i16 = value;
  if (!updateAttributeVal(Thermostat::Id, Thermostat::Attributes::AbsMaxHeatSetpointLimit::Id, &attr)) {
    return false;
  }

  absoluteMaximumHeatingSetpoint = value;

  if (maximumHeatingSetpoint > value) {
    setMaximumHeatingSetpoint(static_cast<float>(value) / 100.0f);
  }

  return true;
}

bool MatterWaterHeater::setMaximumHeatingSetpoint(float temperature) {
  const int16_t value = static_cast<int16_t>(temperature * 100.0f);

  if (!initialized) {
    return false;
  }

  if (value < minimumHeatingSetpoint || value > absoluteMaximumHeatingSetpoint) {
    return false;
  }

  esp_matter_attr_val_t attr = esp_matter_invalid(NULL);
  if (!getAttributeVal(Thermostat::Id, Thermostat::Attributes::MaxHeatSetpointLimit::Id, &attr)) {
    return false;
  }

  attr.val.i16 = value;
  if (!updateAttributeVal(Thermostat::Id, Thermostat::Attributes::MaxHeatSetpointLimit::Id, &attr)) {
    return false;
  }

  maximumHeatingSetpoint = value;

  if (heatingSetpoint > value) {
    setHeatingSetpointRaw(value);
  }

  return true;
}

float MatterWaterHeater::getAbsoluteMinimumHeatingSetpoint() {
  return static_cast<float>(absoluteMinimumHeatingSetpoint) / 100.0f;
}

float MatterWaterHeater::getMinimumHeatingSetpoint() {
  return static_cast<float>(minimumHeatingSetpoint) / 100.0f;
}

float MatterWaterHeater::getAbsoluteMaximumHeatingSetpoint() {
  return static_cast<float>(absoluteMaximumHeatingSetpoint) / 100.0f;
}

float MatterWaterHeater::getMaximumHeatingSetpoint() {
  return static_cast<float>(maximumHeatingSetpoint) / 100.0f;
}

/*
 * --------------------------------------------------------------------------
 * Thermostat system mode
 * --------------------------------------------------------------------------
 */

bool MatterWaterHeater::setSystemMode(SystemMode_t mode) {
  if (!initialized) {
    return false;
  }

  if (mode != SYSTEM_MODE_OFF && mode != SYSTEM_MODE_HEAT) {
    return false;
  }

  esp_matter_attr_val_t value = esp_matter_invalid(NULL);
  if (!getAttributeVal(Thermostat::Id, Thermostat::Attributes::SystemMode::Id, &value)) {
    return false;
  }

  value.val.u8 = static_cast<uint8_t>(mode);
  if (!updateAttributeVal(Thermostat::Id, Thermostat::Attributes::SystemMode::Id, &value)) {
    return false;
  }

  systemMode = static_cast<uint8_t>(mode);
  // Per Matter spec, HeatDemand reflects the heat sources currently active: it must follow SystemMode.
  syncHeatDemand();
  return true;
}

MatterWaterHeater::SystemMode_t MatterWaterHeater::getSystemMode() {
  return static_cast<SystemMode_t>(systemMode);
}

/*
 * --------------------------------------------------------------------------
 * Water Heater Management
 * --------------------------------------------------------------------------
 */

bool MatterWaterHeater::setHeaterTypes(uint8_t value) {
  if (!initialized) {
    return false;
  }

  // HeaterTypes is code-driven (WHM Delegate). Ember rejects get/update (GET_VAL / 262).
  if (heaterTypes != value) {
    heaterTypes = value;
    reportWhmAttribute(getEndPointId(), WaterHeaterManagement::Attributes::HeaterTypes::Id);
  }
  // The active source bits (HeatDemand) can never exceed the heater types the appliance actually has.
  syncHeatDemand();
  return true;
}

uint8_t MatterWaterHeater::getHeaterTypes() {
  return heaterTypes;
}

bool MatterWaterHeater::setHeatDemand(uint8_t value) {
  if (!initialized) {
    return false;
  }

  // HeatDemand is code-driven (WHM Delegate). The ember store rejects writes (262);
  // CHIP and Matter controllers read GetHeatDemand() from this cache.
  if (heatDemand != value) {
    heatDemand = value;
    reportWhmAttribute(getEndPointId(), WaterHeaterManagement::Attributes::HeatDemand::Id);
  }
  return true;
}

uint8_t MatterWaterHeater::getHeatDemand() {
  return heatDemand;
}

void MatterWaterHeater::syncHeatDemand() {
  // Boost overrides normal heating: while active, the configured heater sources are demanded
  // regardless of SystemMode. Otherwise HeatDemand follows SystemMode (Heat -> sources active,
  // Off -> no source active).
  const uint8_t demand = (boostState == BOOST_ACTIVE || systemMode == SYSTEM_MODE_HEAT) ? heaterTypes : 0;
  if (demand != heatDemand) {
    setHeatDemand(demand);
  }
}

bool MatterWaterHeater::setTankVolume(uint16_t value) {
  if (!initialized) {
    return false;
  }

  // TankVolume is code-driven (WHM Delegate GetTankVolume). Ember rejects get/update.
  if (tankVolume != value) {
    tankVolume = value;
    reportWhmAttribute(getEndPointId(), WaterHeaterManagement::Attributes::TankVolume::Id);
  }
  return true;
}

uint16_t MatterWaterHeater::getTankVolume() {
  return tankVolume;
}

bool MatterWaterHeater::setTankPercentage(uint8_t value) {
  if (!initialized || value > 100) {
    return false;
  }

  // TankPercentage is code-driven (WHM Delegate GetTankPercentage). Ember rejects get/update.
  if (tankPercentage != value) {
    tankPercentage = value;
    reportWhmAttribute(getEndPointId(), WaterHeaterManagement::Attributes::TankPercentage::Id);
  }
  maybeFinishOneShotBoost();
  return true;
}

uint8_t MatterWaterHeater::getTankPercentage() {
  return tankPercentage;
}

bool MatterWaterHeater::setEstimatedHeatRequired(int64_t energy_mWh) {
  if (!initialized || energy_mWh < 0) {
    return false;
  }

  // EstimatedHeatRequired is code-driven (WHM Delegate GetEstimatedHeatRequired).
  if (estimatedHeatRequired != energy_mWh) {
    estimatedHeatRequired = energy_mWh;
    reportWhmAttribute(getEndPointId(), WaterHeaterManagement::Attributes::EstimatedHeatRequired::Id);
  }
  return true;
}

int64_t MatterWaterHeater::getEstimatedHeatRequired() {
  return estimatedHeatRequired;
}

bool MatterWaterHeater::applyBoostState(BoostState_t state) {
  if (!initialized) {
    return false;
  }

  if (state != BOOST_INACTIVE && state != BOOST_ACTIVE) {
    return false;
  }

  // BoostState is code-driven (WHM Delegate GetBoostState). Ember rejects writes (262).
  const bool changed = boostState != static_cast<uint8_t>(state);
  boostState = static_cast<uint8_t>(state);
  if (changed) {
    reportWhmAttribute(getEndPointId(), WaterHeaterManagement::Attributes::BoostState::Id);
  }
  // Per Matter spec, HeatDemand reflects the heat sources currently active: Boost must be reflected too.
  syncHeatDemand();
  return true;
}

bool MatterWaterHeater::setBoostState(BoostState_t state) {
  if (!initialized) {
    return false;
  }

  if (state != BOOST_INACTIVE && state != BOOST_ACTIVE) {
    return false;
  }

  if (state == BOOST_INACTIVE && (boostState == BOOST_ACTIVE || boostTimerArmed || boostHasTemporarySetpoint)) {
    finishBoost(true);
    return boostState == BOOST_INACTIVE;
  }

  return applyBoostState(state);
}

void MatterWaterHeater::boostTimerCallback(chip::System::Layer *layer, void *appState) {
  (void)layer;
  static_cast<MatterWaterHeater *>(appState)->finishBoost(true);
}

void MatterWaterHeater::armBoostTimer(uint32_t durationSeconds) {
  cancelBoostTimer();
  if (durationSeconds == 0) {
    return;
  }
  if (!chip::DeviceLayer::SystemLayer().IsInitialized()) {
    log_w("Cannot arm Water Heater boost timer; SystemLayer is not initialized");
    return;
  }

  lock::ScopedChipStackLock lock(portMAX_DELAY);
  const CHIP_ERROR err =
    chip::DeviceLayer::SystemLayer().StartTimer(chip::System::Clock::Seconds32(durationSeconds), boostTimerCallback, this);
  if (err != CHIP_NO_ERROR) {
    log_e("Failed to start Water Heater boost timer: %" CHIP_ERROR_FORMAT, err.Format());
    return;
  }
  boostTimerArmed = true;
}

void MatterWaterHeater::cancelBoostTimer() {
  if (!boostTimerArmed) {
    return;
  }
  if (chip::DeviceLayer::SystemLayer().IsInitialized()) {
    lock::ScopedChipStackLock lock(portMAX_DELAY);
    chip::DeviceLayer::SystemLayer().CancelTimer(boostTimerCallback, this);
  }
  boostTimerArmed = false;
}

void MatterWaterHeater::finishBoost(bool emitEndedEvent) {
  if (boostFinishing) {
    return;
  }
  boostFinishing = true;

  cancelBoostTimer();
  if (boostHasTemporarySetpoint) {
    (void)setHeatingSetpointRaw(boostSavedHeatingSetpoint);
    boostHasTemporarySetpoint = false;
  }
  boostOneShot = false;
  boostHasTargetPercentage = false;

  if (boostState != BOOST_INACTIVE) {
    (void)applyBoostState(BOOST_INACTIVE);
  }

  if (emitEndedEvent && managementDelegate != nullptr) {
    // GenerateEvent() requires the CHIP stack lock. Controller CancelBoost / the boost
    // timer already hold it; setBoostState() from loop() does not.
    ManagementDelegate *delegate = managementDelegate;
    CHIP_ERROR err = CHIP_NO_ERROR;
    if (chip::DeviceLayer::PlatformMgr().IsChipStackLockedByCurrentThread()) {
      err = delegate->GenerateBoostEndedEvent();
    } else if (chip::DeviceLayer::SystemLayer().IsInitialized()) {
      err = chip::DeviceLayer::SystemLayer().ScheduleLambda([delegate]() {
        const CHIP_ERROR emitErr = delegate->GenerateBoostEndedEvent();
        if (emitErr != CHIP_NO_ERROR) {
          log_w("Failed to emit Water Heater BoostEnded: %" CHIP_ERROR_FORMAT, emitErr.Format());
        }
      });
    } else {
      err = CHIP_ERROR_INCORRECT_STATE;
    }
    if (err != CHIP_NO_ERROR) {
      log_w("Failed to emit Water Heater BoostEnded: %" CHIP_ERROR_FORMAT, err.Format());
    }
  }

  boostFinishing = false;
}

void MatterWaterHeater::maybeFinishOneShotBoost() {
  if (!boostOneShot || boostState != BOOST_ACTIVE) {
    return;
  }
  if (localTemperature < heatingSetpoint) {
    return;
  }
  if (boostHasTargetPercentage && tankPercentage < boostTargetPercentage) {
    return;
  }
  finishBoost(true);
}

MatterWaterHeater::BoostState_t MatterWaterHeater::getBoostState() {
  return static_cast<BoostState_t>(boostState);
}

/*
 * --------------------------------------------------------------------------
 * Water Heater Mode
 * --------------------------------------------------------------------------
 */

bool MatterWaterHeater::setWaterHeaterMode(WaterHeaterMode_t mode) {
  if (!initialized) {
    return false;
  }

  if (mode != WATER_HEATER_MODE_OFF && mode != WATER_HEATER_MODE_MANUAL && mode != WATER_HEATER_MODE_ECO) {
    return false;
  }

  chip::app::Clusters::ModeBase::Instance *modeInstance = (modeDelegate != nullptr) ? modeDelegate->instance() : nullptr;
  if (modeInstance == nullptr) {
    // ModeBase::Instance is created when the Matter stack starts. Cache until Init().
    waterHeaterMode = static_cast<uint8_t>(mode);
    waterHeaterModeSetByApp = true;
    return true;
  }

  lock::ScopedChipStackLock lock(portMAX_DELAY);
  if (modeInstance->UpdateCurrentMode(static_cast<uint8_t>(mode)) != chip::Protocols::InteractionModel::Status::Success) {
    return false;
  }
  waterHeaterMode = static_cast<uint8_t>(mode);
  waterHeaterModeSetByApp = true;
  return true;
}

MatterWaterHeater::WaterHeaterMode_t MatterWaterHeater::getWaterHeaterMode() {
  return static_cast<WaterHeaterMode_t>(waterHeaterMode);
}

/*
 * --------------------------------------------------------------------------
 * MatterEndPoint callback
 * --------------------------------------------------------------------------
 */

bool MatterWaterHeater::attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) {
  if (!initialized || val == nullptr) {
    return false;
  }

  if (endpoint_id != getEndPointId()) {
    return false;
  }

  switch (cluster_id) {
    case Thermostat::Id:
      switch (attribute_id) {
        case Thermostat::Attributes::LocalTemperature::Id:
          localTemperature = val->val.i16;
          maybeFinishOneShotBoost();
          return true;

        case Thermostat::Attributes::OccupiedHeatingSetpoint::Id:
          heatingSetpoint = val->val.i16;
          return true;

        case Thermostat::Attributes::AbsMinHeatSetpointLimit::Id:
          absoluteMinimumHeatingSetpoint = val->val.i16;
          return true;

        case Thermostat::Attributes::MinHeatSetpointLimit::Id:
          minimumHeatingSetpoint = val->val.i16;
          return true;

        case Thermostat::Attributes::AbsMaxHeatSetpointLimit::Id:
          absoluteMaximumHeatingSetpoint = val->val.i16;
          return true;

        case Thermostat::Attributes::MaxHeatSetpointLimit::Id:
          maximumHeatingSetpoint = val->val.i16;
          return true;

        case Thermostat::Attributes::SystemMode::Id:
          systemMode = val->val.u8;
          syncHeatDemand();
          return true;

        default: break;
      }
      break;

    case WaterHeaterManagement::Id:
      switch (attribute_id) {
        case WaterHeaterManagement::Attributes::HeaterTypes::Id:
          heaterTypes = val->val.u8;
          syncHeatDemand();
          return true;

        case WaterHeaterManagement::Attributes::HeatDemand::Id:
          heatDemand = val->val.u8;
          return true;

        case WaterHeaterManagement::Attributes::BoostState::Id:
          if (!boostFinishing && val->val.u8 == BOOST_INACTIVE &&
              (boostState == BOOST_ACTIVE || boostTimerArmed || boostHasTemporarySetpoint)) {
            finishBoost(true);
            return true;
          }
          boostState = val->val.u8;
          syncHeatDemand();
          return true;

        case WaterHeaterManagement::Attributes::TankVolume::Id:
          tankVolume = val->val.u16;
          return true;

        case WaterHeaterManagement::Attributes::TankPercentage::Id:
          tankPercentage = val->val.u8;
          maybeFinishOneShotBoost();
          return true;

        case WaterHeaterManagement::Attributes::EstimatedHeatRequired::Id:
          estimatedHeatRequired = val->val.i64;
          return true;

        default: break;
      }
      break;

    case WaterHeaterMode::Id:
      switch (attribute_id) {
        case WaterHeaterMode::Attributes::CurrentMode::Id:
          waterHeaterMode = val->val.u8;
          return true;

        default: break;
      }
      break;

    default: break;
  }

  return true;
}
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
