#include <sdkconfig.h>

#ifdef CONFIG_ESP_MATTER_ENABLE_DATA_MODEL

#include <Matter.h>
#include <MatterEndpoints/MatterWaterHeater.h>

#include <esp_matter.h>
#include <esp_matter_core.h>

#include <app-common/zap-generated/cluster-enums.h>

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

constexpr uint8_t DEFAULT_SYSTEM_MODE =
  static_cast<uint8_t>(Thermostat::SystemModeEnum::kHeat);

constexpr uint8_t DEFAULT_HEATER_TYPES =
  MatterWaterHeater::IMMERSION_ELEMENT_1;

constexpr uint8_t DEFAULT_HEAT_DEMAND = 0;
constexpr uint16_t DEFAULT_TANK_VOLUME = 100;
constexpr uint8_t DEFAULT_TANK_PERCENTAGE = 100;
constexpr uint8_t DEFAULT_BOOST_STATE =
  MatterWaterHeater::BOOST_INACTIVE;

constexpr uint8_t DEFAULT_WATER_HEATER_MODE =
  MatterWaterHeater::WATER_HEATER_MODE_MANUAL;

}  // namespace

MatterWaterHeater::MatterWaterHeater()
{
}

MatterWaterHeater::~MatterWaterHeater()
{
  end();
}

bool MatterWaterHeater::begin()
{
  if (initialized) {
    return false;
  }

  ArduinoMatter::_init();

  if (getEndPointId() != 0) {
    return false;
  }

  /*
   * Water Heater Management
   */
  esp_matter::cluster::water_heater_management::config_t
    management_config;

  management_config.heater_types = DEFAULT_HEATER_TYPES;
  management_config.heat_demand = DEFAULT_HEAT_DEMAND;
  management_config.boost_state = DEFAULT_BOOST_STATE;

  /*
   * Water Heater Mode
   */
  esp_matter::cluster::water_heater_mode::config_t
    mode_config;

  /*
   * Thermostat
   *
   * This is the endpoint thermostat configuration, not the cluster
   * namespace. Use the fully-qualified namespace to avoid the
   * endpoint::thermostat / cluster::thermostat ambiguity.
   */
  esp_matter::endpoint::thermostat::config_t thermostat_config;

  thermostat_config.local_temperature = DEFAULT_LOCAL_TEMPERATURE;

  thermostat_config.control_sequence_of_operation =
    static_cast<uint8_t>(
      Thermostat::ControlSequenceOfOperationEnum::kHeatingOnly);

  thermostat_config.system_mode = DEFAULT_SYSTEM_MODE;

  thermostat_config.features.heating.occupied_heating_setpoint =
    DEFAULT_HEATING_SETPOINT;

  thermostat_config.features.heating.unoccupied_heating_setpoint =
    DEFAULT_HEATING_SETPOINT;

  thermostat_config.feature_flags |=
    esp_matter::cluster::thermostat::feature::heating::get_id();

  /*
   * Water Heater device type 0x050F.
   *
   * The generated device type creates:
   *
   *   - Water Heater Management
   *   - Water Heater Mode
   *   - Thermostat
   *
   * on the same endpoint.
   */
  esp_matter::endpoint::water_heater::config_t water_heater_config;

  water_heater_config.water_heater_management = management_config;
  water_heater_config.water_heater_mode = mode_config;
  water_heater_config.thermostat = thermostat_config;

  endpoint_t *endpoint =
    esp_matter::endpoint::water_heater::create(
      node::get(),
      &water_heater_config,
      ENDPOINT_FLAG_NONE,
      this);

  if (endpoint == nullptr) {
    return false;
  }

  /*
   * Enable the optional Water Heater Management features.
   *
   * These features are no longer configured through
   * management_config.features.* in current esp-matter.
   */
  cluster_t *management_cluster =
    cluster::get(
      endpoint,
      WaterHeaterManagement::Id);

  if (management_cluster == nullptr) {
    return false;
  }

  if (esp_matter::cluster::water_heater_management::
        feature::energy_management::add(management_cluster) != ESP_OK) {
    return false;
  }

  if (esp_matter::cluster::water_heater_management::
        feature::tank_percentage::add(management_cluster) != ESP_OK) {
    return false;
  }

  /*
   * Store the endpoint ID in MatterEndPoint.
   */
  setEndPointId(endpoint::get_id(endpoint));

  initialized = true;

  /*
   * Initialize the local cache from the values configured above.
   */
  localTemperature = DEFAULT_LOCAL_TEMPERATURE;
  heatingSetpoint = DEFAULT_HEATING_SETPOINT;

  absoluteMinimumHeatingSetpoint =
    ABS_MIN_HEATING_SETPOINT;

  minimumHeatingSetpoint =
    MIN_HEATING_SETPOINT;

  absoluteMaximumHeatingSetpoint =
    ABS_MAX_HEATING_SETPOINT;

  maximumHeatingSetpoint =
    MAX_HEATING_SETPOINT;

  systemMode = DEFAULT_SYSTEM_MODE;

  heaterTypes = DEFAULT_HEATER_TYPES;
  heatDemand = DEFAULT_HEAT_DEMAND;
  tankVolume = DEFAULT_TANK_VOLUME;
  tankPercentage = DEFAULT_TANK_PERCENTAGE;
  boostState = DEFAULT_BOOST_STATE;
  waterHeaterMode = DEFAULT_WATER_HEATER_MODE;

  /*
   * The generated Water Heater device type creates the mandatory
   * attributes. The optional management features have just been
   * added above.
   *
   * Push our configured initial values after the attributes exist.
   */
  setTankVolume(DEFAULT_TANK_VOLUME);
  setTankPercentage(DEFAULT_TANK_PERCENTAGE);

  return true;
}

void MatterWaterHeater::end()
{
  initialized = false;
}

/*
 * --------------------------------------------------------------------------
 * Thermostat / local temperature
 * --------------------------------------------------------------------------
 */

bool MatterWaterHeater::setLocalTemperature(float temperature)
{
  return setLocalTemperatureRaw(
    static_cast<int16_t>(temperature * 100.0f));
}

float MatterWaterHeater::getLocalTemperature()
{
  return static_cast<float>(localTemperature) / 100.0f;
}

bool MatterWaterHeater::setLocalTemperatureRaw(int16_t temperature)
{
  if (!initialized) {
    return false;
  }

  esp_matter_attr_val_t value =
    esp_matter_invalid(NULL);

  if (!getAttributeVal(
        Thermostat::Id,
        Thermostat::Attributes::LocalTemperature::Id,
        &value)) {
    return false;
  }

  if (value.val.i16 == temperature) {
    localTemperature = temperature;
    return true;
  }

  value.val.i16 = temperature;

  if (!updateAttributeVal(
        Thermostat::Id,
        Thermostat::Attributes::LocalTemperature::Id,
        &value)) {
    return false;
  }

  localTemperature = temperature;
  return true;
}

int16_t MatterWaterHeater::getLocalTemperatureRaw()
{
  return localTemperature;
}

/*
 * --------------------------------------------------------------------------
 * Heating setpoint
 * --------------------------------------------------------------------------
 */

bool MatterWaterHeater::setHeatingSetpoint(float temperature)
{
  return setHeatingSetpointRaw(
    static_cast<int16_t>(temperature * 100.0f));
}

float MatterWaterHeater::getHeatingSetpoint()
{
  return static_cast<float>(heatingSetpoint) / 100.0f;
}

bool MatterWaterHeater::setHeatingSetpointRaw(int16_t temperature)
{
  if (!initialized) {
    return false;
  }

  if (temperature < minimumHeatingSetpoint ||
      temperature > maximumHeatingSetpoint) {
    return false;
  }

  esp_matter_attr_val_t value =
    esp_matter_invalid(NULL);

  if (!getAttributeVal(
        Thermostat::Id,
        Thermostat::Attributes::OccupiedHeatingSetpoint::Id,
        &value)) {
    return false;
  }

  if (value.val.i16 == temperature) {
    heatingSetpoint = temperature;
    return true;
  }

  value.val.i16 = temperature;

  if (!updateAttributeVal(
        Thermostat::Id,
        Thermostat::Attributes::OccupiedHeatingSetpoint::Id,
        &value)) {
    return false;
  }

  heatingSetpoint = temperature;
  return true;
}

int16_t MatterWaterHeater::getHeatingSetpointRaw()
{
  return heatingSetpoint;
}

/*
 * --------------------------------------------------------------------------
 * Heating setpoint limits
 * --------------------------------------------------------------------------
 */

bool MatterWaterHeater::setAbsoluteMinimumHeatingSetpoint(
  float temperature)
{
  const int16_t value =
    static_cast<int16_t>(temperature * 100.0f);

  if (!initialized) {
    return false;
  }

  if (value < ABS_MIN_HEATING_SETPOINT ||
      value > ABS_MAX_HEATING_SETPOINT) {
    return false;
  }

  esp_matter_attr_val_t attr =
    esp_matter_invalid(NULL);

  if (!getAttributeVal(
        Thermostat::Id,
        Thermostat::Attributes::
          AbsMinHeatSetpointLimit::Id,
        &attr)) {
    return false;
  }

  attr.val.i16 = value;

  if (!updateAttributeVal(
        Thermostat::Id,
        Thermostat::Attributes::
          AbsMinHeatSetpointLimit::Id,
        &attr)) {
    return false;
  }

  absoluteMinimumHeatingSetpoint = value;

  if (minimumHeatingSetpoint < value) {
    setMinimumHeatingSetpoint(
      static_cast<float>(value) / 100.0f);
  }

  return true;
}

bool MatterWaterHeater::setMinimumHeatingSetpoint(
  float temperature)
{
  const int16_t value =
    static_cast<int16_t>(temperature * 100.0f);

  if (!initialized) {
    return false;
  }

  if (value < absoluteMinimumHeatingSetpoint ||
      value > maximumHeatingSetpoint) {
    return false;
  }

  esp_matter_attr_val_t attr =
    esp_matter_invalid(NULL);

  if (!getAttributeVal(
        Thermostat::Id,
        Thermostat::Attributes::
          MinHeatSetpointLimit::Id,
        &attr)) {
    return false;
  }

  attr.val.i16 = value;

  if (!updateAttributeVal(
        Thermostat::Id,
        Thermostat::Attributes::
          MinHeatSetpointLimit::Id,
        &attr)) {
    return false;
  }

  minimumHeatingSetpoint = value;

  if (heatingSetpoint < value) {
    setHeatingSetpointRaw(value);
  }

  return true;
}

bool MatterWaterHeater::setAbsoluteMaximumHeatingSetpoint(
  float temperature)
{
  const int16_t value =
    static_cast<int16_t>(temperature * 100.0f);

  if (!initialized) {
    return false;
  }

  if (value < ABS_MIN_HEATING_SETPOINT ||
      value > ABS_MAX_HEATING_SETPOINT) {
    return false;
  }

  esp_matter_attr_val_t attr =
    esp_matter_invalid(NULL);

  if (!getAttributeVal(
        Thermostat::Id,
        Thermostat::Attributes::
          AbsMaxHeatSetpointLimit::Id,
        &attr)) {
    return false;
  }

  attr.val.i16 = value;

  if (!updateAttributeVal(
        Thermostat::Id,
        Thermostat::Attributes::
          AbsMaxHeatSetpointLimit::Id,
        &attr)) {
    return false;
  }

  absoluteMaximumHeatingSetpoint = value;

  if (maximumHeatingSetpoint > value) {
    setMaximumHeatingSetpoint(
      static_cast<float>(value) / 100.0f);
  }

  return true;
}

bool MatterWaterHeater::setMaximumHeatingSetpoint(
  float temperature)
{
  const int16_t value =
    static_cast<int16_t>(temperature * 100.0f);

  if (!initialized) {
    return false;
  }

  if (value < minimumHeatingSetpoint ||
      value > absoluteMaximumHeatingSetpoint) {
    return false;
  }

  esp_matter_attr_val_t attr =
    esp_matter_invalid(NULL);

  if (!getAttributeVal(
        Thermostat::Id,
        Thermostat::Attributes::
          MaxHeatSetpointLimit::Id,
        &attr)) {
    return false;
  }

  attr.val.i16 = value;

  if (!updateAttributeVal(
        Thermostat::Id,
        Thermostat::Attributes::
          MaxHeatSetpointLimit::Id,
        &attr)) {
    return false;
  }

  maximumHeatingSetpoint = value;

  if (heatingSetpoint > value) {
    setHeatingSetpointRaw(value);
  }

  return true;
}

float MatterWaterHeater::getAbsoluteMinimumHeatingSetpoint()
{
  return static_cast<float>(
    absoluteMinimumHeatingSetpoint) / 100.0f;
}

float MatterWaterHeater::getMinimumHeatingSetpoint()
{
  return static_cast<float>(
    minimumHeatingSetpoint) / 100.0f;
}

float MatterWaterHeater::getAbsoluteMaximumHeatingSetpoint()
{
  return static_cast<float>(
    absoluteMaximumHeatingSetpoint) / 100.0f;
}

float MatterWaterHeater::getMaximumHeatingSetpoint()
{
  return static_cast<float>(
    maximumHeatingSetpoint) / 100.0f;
}

/*
 * --------------------------------------------------------------------------
 * Thermostat system mode
 * --------------------------------------------------------------------------
 */

bool MatterWaterHeater::setSystemMode(
  SystemMode_t mode)
{
  if (!initialized) {
    return false;
  }

  if (mode != SYSTEM_MODE_OFF &&
      mode != SYSTEM_MODE_HEAT) {
    return false;
  }

  esp_matter_attr_val_t value =
    esp_matter_invalid(NULL);

  if (!getAttributeVal(
        Thermostat::Id,
        Thermostat::Attributes::SystemMode::Id,
        &value)) {
    return false;
  }

  value.val.u8 = static_cast<uint8_t>(mode);

  if (!updateAttributeVal(
        Thermostat::Id,
        Thermostat::Attributes::SystemMode::Id,
        &value)) {
    return false;
  }

  systemMode = static_cast<uint8_t>(mode);
  return true;
}

MatterWaterHeater::SystemMode_t
MatterWaterHeater::getSystemMode()
{
  return static_cast<SystemMode_t>(systemMode);
}

/*
 * --------------------------------------------------------------------------
 * Water Heater Management
 * --------------------------------------------------------------------------
 */

bool MatterWaterHeater::setHeaterTypes(
  uint8_t value)
{
  if (!initialized) {
    return false;
  }

  esp_matter_attr_val_t attr =
    esp_matter_invalid(NULL);

  if (!getAttributeVal(
        WaterHeaterManagement::Id,
        WaterHeaterManagement::Attributes::
          HeaterTypes::Id,
        &attr)) {
    return false;
  }

  attr.val.u8 = value;

  if (!updateAttributeVal(
        WaterHeaterManagement::Id,
        WaterHeaterManagement::Attributes::
          HeaterTypes::Id,
        &attr)) {
    return false;
  }

  heaterTypes = value;
  return true;
}

uint8_t MatterWaterHeater::getHeaterTypes()
{
  return heaterTypes;
}

bool MatterWaterHeater::setHeatDemand(
  uint8_t value)
{
  if (!initialized) {
    return false;
  }

  esp_matter_attr_val_t attr =
    esp_matter_invalid(NULL);

  if (!getAttributeVal(
        WaterHeaterManagement::Id,
        WaterHeaterManagement::Attributes::
          HeatDemand::Id,
        &attr)) {
    return false;
  }

  attr.val.u8 = value;

  if (!updateAttributeVal(
        WaterHeaterManagement::Id,
        WaterHeaterManagement::Attributes::
          HeatDemand::Id,
        &attr)) {
    return false;
  }

  heatDemand = value;
  return true;
}

uint8_t MatterWaterHeater::getHeatDemand()
{
  return heatDemand;
}

bool MatterWaterHeater::setTankVolume(
  uint16_t value)
{
  if (!initialized) {
    return false;
  }

  esp_matter_attr_val_t attr =
    esp_matter_invalid(NULL);

  if (!getAttributeVal(
        WaterHeaterManagement::Id,
        WaterHeaterManagement::Attributes::
          TankVolume::Id,
        &attr)) {
    return false;
  }

  attr.val.u16 = value;

  if (!updateAttributeVal(
        WaterHeaterManagement::Id,
        WaterHeaterManagement::Attributes::
          TankVolume::Id,
        &attr)) {
    return false;
  }

  tankVolume = value;
  return true;
}

uint16_t MatterWaterHeater::getTankVolume()
{
  return tankVolume;
}

bool MatterWaterHeater::setTankPercentage(
  uint8_t value)
{
  if (!initialized || value > 100) {
    return false;
  }

  esp_matter_attr_val_t attr =
    esp_matter_invalid(NULL);

  if (!getAttributeVal(
        WaterHeaterManagement::Id,
        WaterHeaterManagement::Attributes::
          TankPercentage::Id,
        &attr)) {
    return false;
  }

  attr.val.u8 = value;

  if (!updateAttributeVal(
        WaterHeaterManagement::Id,
        WaterHeaterManagement::Attributes::
          TankPercentage::Id,
        &attr)) {
    return false;
  }

  tankPercentage = value;
  return true;
}

uint8_t MatterWaterHeater::getTankPercentage()
{
  return tankPercentage;
}

bool MatterWaterHeater::setBoostState(
  BoostState_t state)
{
  if (!initialized) {
    return false;
  }

  if (state != BOOST_INACTIVE &&
      state != BOOST_ACTIVE) {
    return false;
  }

  esp_matter_attr_val_t attr =
    esp_matter_invalid(NULL);

  if (!getAttributeVal(
        WaterHeaterManagement::Id,
        WaterHeaterManagement::Attributes::
          BoostState::Id,
        &attr)) {
    return false;
  }

  attr.val.u8 = static_cast<uint8_t>(state);

  if (!updateAttributeVal(
        WaterHeaterManagement::Id,
        WaterHeaterManagement::Attributes::
          BoostState::Id,
        &attr)) {
    return false;
  }

  boostState = static_cast<uint8_t>(state);
  return true;
}

MatterWaterHeater::BoostState_t
MatterWaterHeater::getBoostState()
{
  return static_cast<BoostState_t>(boostState);
}

/*
 * --------------------------------------------------------------------------
 * Water Heater Mode
 * --------------------------------------------------------------------------
 */

bool MatterWaterHeater::setWaterHeaterMode(
  WaterHeaterMode_t mode)
{
  if (!initialized) {
    return false;
  }

  if (mode != WATER_HEATER_MODE_OFF &&
      mode != WATER_HEATER_MODE_MANUAL &&
      mode != WATER_HEATER_MODE_ECO) {
    return false;
  }

  esp_matter_attr_val_t attr =
    esp_matter_invalid(NULL);

  if (!getAttributeVal(
        WaterHeaterMode::Id,
        WaterHeaterMode::Attributes::
          CurrentMode::Id,
        &attr)) {
    return false;
  }

  attr.val.u8 = static_cast<uint8_t>(mode);

  if (!updateAttributeVal(
        WaterHeaterMode::Id,
        WaterHeaterMode::Attributes::
          CurrentMode::Id,
        &attr)) {
    return false;
  }

  waterHeaterMode = static_cast<uint8_t>(mode);
  return true;
}

MatterWaterHeater::WaterHeaterMode_t
MatterWaterHeater::getWaterHeaterMode()
{
  return static_cast<WaterHeaterMode_t>(
    waterHeaterMode);
}

/*
 * --------------------------------------------------------------------------
 * MatterEndPoint callback
 * --------------------------------------------------------------------------
 */

bool MatterWaterHeater::attributeChangeCB(
  uint16_t endpoint_id,
  uint32_t cluster_id,
  uint32_t attribute_id,
  esp_matter_attr_val_t *val)
{
  if (!initialized || val == nullptr) {
    return false;
  }

  if (endpoint_id != getEndPointId()) {
    return false;
  }

  switch (cluster_id) {

    case Thermostat::Id:
      switch (attribute_id) {

        case Thermostat::Attributes::
          LocalTemperature::Id:
          localTemperature = val->val.i16;
          return true;

        case Thermostat::Attributes::
          OccupiedHeatingSetpoint::Id:
          heatingSetpoint = val->val.i16;
          return true;

        case Thermostat::Attributes::
          AbsMinHeatSetpointLimit::Id:
          absoluteMinimumHeatingSetpoint =
            val->val.i16;
          return true;

        case Thermostat::Attributes::
          MinHeatSetpointLimit::Id:
          minimumHeatingSetpoint =
            val->val.i16;
          return true;

        case Thermostat::Attributes::
          AbsMaxHeatSetpointLimit::Id:
          absoluteMaximumHeatingSetpoint =
            val->val.i16;
          return true;

        case Thermostat::Attributes::
          MaxHeatSetpointLimit::Id:
          maximumHeatingSetpoint =
            val->val.i16;
          return true;

        case Thermostat::Attributes::
          SystemMode::Id:
          systemMode = val->val.u8;
          return true;

        default:
          break;
      }
      break;

    case WaterHeaterManagement::Id:
      switch (attribute_id) {

        case WaterHeaterManagement::Attributes::
          HeaterTypes::Id:
          heaterTypes = val->val.u8;
          return true;

        case WaterHeaterManagement::Attributes::
          HeatDemand::Id:
          heatDemand = val->val.u8;
          return true;

        case WaterHeaterManagement::Attributes::
          BoostState::Id:
          boostState = val->val.u8;
          return true;

        /*
         * TankVolume and TankPercentage are optional features,
         * but once enabled above they are normal attributes.
         */
        case WaterHeaterManagement::Attributes::
          TankVolume::Id:
          tankVolume = val->val.u16;
          return true;

        case WaterHeaterManagement::Attributes::
          TankPercentage::Id:
          tankPercentage = val->val.u8;
          return true;

        default:
          break;
      }
      break;

    case WaterHeaterMode::Id:
      switch (attribute_id) {

        case WaterHeaterMode::Attributes::
          CurrentMode::Id:
          waterHeaterMode = val->val.u8;
          return true;

        default:
          break;
      }
      break;

    default:
      break;
  }

  return true;
}

#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */