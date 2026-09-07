#include <sdkconfig.h>

#ifdef CONFIG_ESP_MATTER_ENABLE_DATA_MODEL

#include <Matter.h>
#include <MatterEndpoints/MatterWaterHeater.h>

#include <esp_matter.h>
#include <esp_matter_core.h>

#include <app-common/zap-generated/cluster-enums.h>

using namespace esp_matter;
using namespace esp_matter::endpoint;
using namespace esp_matter::cluster;

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

constexpr uint8_t DEFAULT_HEATER_TYPES = 0x01;
constexpr uint8_t DEFAULT_HEAT_DEMAND = 0;
constexpr uint16_t DEFAULT_TANK_VOLUME = 100;
constexpr uint8_t DEFAULT_TANK_PERCENTAGE = 100;
constexpr uint8_t DEFAULT_BOOST_STATE = 0;

constexpr uint8_t DEFAULT_WATER_HEATER_MODE = 1;

/*
 * Matter Water Heater device type:
 *
 *   0x050F
 *
 * esp-matter's generated implementation creates:
 *
 *   Descriptor
 *   WaterHeaterManagement
 *   WaterHeaterMode
 *   Thermostat
 *
 * automatically.
 */

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
    log_e("Matter Water Heater has already been initialized.");
    return false;
  }

  /*
   * Make sure the Arduino Matter node exists.
   *
   * This is the same lifecycle used by the other Arduino-ESP32
   * Matter endpoints.
   */
  ArduinoMatter::_init();

  if (getEndPointId() != 0) {
    log_e(
      "Matter Water Heater with Endpoint Id %u already exists.",
      getEndPointId()
    );
    return false;
  }

  /*
   * Water Heater Management
   */
  water_heater_management::config_t management_config;

  management_config.heater_types = DEFAULT_HEATER_TYPES;
  management_config.heat_demand = DEFAULT_HEAT_DEMAND;
  management_config.boost_state = DEFAULT_BOOST_STATE;

  /*
   * Enable the Energy Management feature.
   */
  management_config.features.energy_management.tank_volume =
    DEFAULT_TANK_VOLUME;

  /*
   * Enable TankPercent feature.
   */
  management_config.features.tank_percent.tank_percentage =
    DEFAULT_TANK_PERCENTAGE;

  /*
   * Water Heater Mode
   */
  water_heater_mode::config_t mode_config;

  /*
   * Thermostat
   */
  thermostat::config_t thermostat_config;

  thermostat_config.local_temperature =
    DEFAULT_LOCAL_TEMPERATURE;

  thermostat_config.control_sequence_of_operation =
    static_cast<uint8_t>(
      Thermostat::ControlSequenceOfOperationEnum::kHeatingOnly
    );

  thermostat_config.system_mode =
    DEFAULT_SYSTEM_MODE;

  thermostat_config.features.heating.occupied_heating_setpoint =
    DEFAULT_HEATING_SETPOINT;

  thermostat_config.features.heating.unoccupied_heating_setpoint =
    DEFAULT_HEATING_SETPOINT;

  thermostat_config.feature_flags |=
    thermostat::feature::heating::get_id();

  /*
   * Device type configuration.
   */
  water_heater::config_t water_heater_config;

  water_heater_config.water_heater_management =
    management_config;

  water_heater_config.water_heater_mode =
    mode_config;

  water_heater_config.thermostat =
    thermostat_config;

  /*
   * Create the Water Heater endpoint.
   */
  endpoint_t *endpoint =
    water_heater::create(
      node::get(),
      &water_heater_config,
      ENDPOINT_FLAG_NONE,
      this
    );

  if (endpoint == nullptr) {
    log_e("Failed to create Matter Water Heater endpoint.");
    return false;
  }

  setEndPointId(endpoint::get_id(endpoint));

  /*
   * Cache the initial values.
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

  waterHeaterMode =
    DEFAULT_WATER_HEATER_MODE;

  initialized = true;

  return true;
}

void MatterWaterHeater::end()
{
  /*
   * Arduino-ESP32 Matter endpoints don't currently expose a public
   * endpoint::destroy() equivalent through MatterEndPoint.
   *
   * Keep the object logically stopped, as the existing endpoint
   * implementations do.
   */
  initialized = false;
}

/*
 * --------------------------------------------------------------------------
 * Attribute callback
 * --------------------------------------------------------------------------
 */

bool MatterWaterHeater::attributeChangeCB(
  uint16_t endpoint_id,
  uint32_t cluster_id,
  uint32_t attribute_id,
  esp_matter_attr_val_t *val
)
{
  if (!initialized || val == nullptr) {
    return false;
  }

  if (endpoint_id != getEndPointId()) {
    return true;
  }

  /*
   * Thermostat
   */
  if (cluster_id == Thermostat::Id) {

    switch (attribute_id) {

      case Thermostat::Attributes::LocalTemperature::Id:
        localTemperature = val->val.i16;

        log_v(
          "Water Heater local temperature: %.2f C",
          static_cast<float>(localTemperature) / 100.0f
        );
        break;

      case Thermostat::Attributes::OccupiedHeatingSetpoint::Id:
        heatingSetpoint = val->val.i16;

        log_v(
          "Water Heater heating setpoint: %.2f C",
          static_cast<float>(heatingSetpoint) / 100.0f
        );
        break;

      case Thermostat::Attributes::SystemMode::Id:
        systemMode = val->val.u8;

        log_v(
          "Water Heater system mode: %u",
          systemMode
        );
        break;

      default:
        break;
    }

    return true;
  }

  /*
   * Water Heater Management
   */
  if (cluster_id == WaterHeaterManagement::Id) {

    switch (attribute_id) {

      case WaterHeaterManagement::Attributes::HeaterTypes::Id:
        heaterTypes = val->val.u8;
        break;

      case WaterHeaterManagement::Attributes::HeatDemand::Id:
        heatDemand = val->val.u8;
        break;

      case WaterHeaterManagement::Attributes::TankVolume::Id:
        tankVolume = val->val.u16;
        break;

      case WaterHeaterManagement::Attributes::TankPercentage::Id:
        tankPercentage = val->val.u8;
        break;

      case WaterHeaterManagement::Attributes::BoostState::Id:
        boostState = val->val.u8;
        break;

      default:
        break;
    }

    return true;
  }

  /*
   * Water Heater Mode
   */
  if (cluster_id == WaterHeaterMode::Id) {

    if (attribute_id ==
        WaterHeaterMode::Attributes::CurrentMode::Id) {

      waterHeaterMode = val->val.u8;

      log_v(
        "Water Heater operation mode: %u",
        waterHeaterMode
      );
    }

    return true;
  }

  return true;
}

/*
 * --------------------------------------------------------------------------
 * Temperature
 * --------------------------------------------------------------------------
 */

bool MatterWaterHeater::setLocalTemperature(float temperature)
{
  return setLocalTemperatureRaw(
    static_cast<int16_t>(temperature * 100.0f)
  );
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
    esp_matter_nullable<int16_t>(temperature);

  value = esp_matter_attr_val(temperature);

  return updateAttributeVal(
    Thermostat::Id,
    Thermostat::Attributes::LocalTemperature::Id,
    &value
  );
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
    static_cast<int16_t>(temperature * 100.0f)
  );
}

float MatterWaterHeater::getHeatingSetpoint()
{
  return static_cast<float>(heatingSetpoint) / 100.0f;
}

bool MatterWaterHeater::setHeatingSetpointRaw(
  int16_t temperature
)
{
  if (!initialized) {
    return false;
  }

  if (temperature < minimumHeatingSetpoint ||
      temperature > maximumHeatingSetpoint) {
    log_e(
      "Heating setpoint %.2f C is outside [%0.2f, %0.2f] C.",
      static_cast<float>(temperature) / 100.0f,
      static_cast<float>(minimumHeatingSetpoint) / 100.0f,
      static_cast<float>(maximumHeatingSetpoint) / 100.0f
    );

    return false;
  }

    esp_matter_attr_val_t val = esp_matter_invalid(NULL);

    val.type = ESP_MATTER_VAL_TYPE_INT16;
    val.val.i16 = temperature;

    if (!updateAttributeVal(
            Thermostat::Id,
            Thermostat::Attributes::OccupiedHeatingSetpoint::Id,
            &val)) {
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
 * Heating limits
 * --------------------------------------------------------------------------
 */

bool MatterWaterHeater::setAbsoluteMinimumHeatingSetpoint(
  float temperature
)
{
  if (!initialized) {
    return false;
  }

  int16_t value =
    static_cast<int16_t>(temperature * 100.0f);

  if (value > absoluteMinimumHeatingSetpoint) {
    /*
     * The absolute minimum can only be lowered by hardware
     * configuration; don't allow an invalid Matter state.
     */
  }

  esp_matter_attr_val_t attr =
    esp_matter_attr_val(value);

  if (!updateAttributeVal(
        Thermostat::Id,
        Thermostat::Attributes::AbsMinHeatSetpointLimit::Id,
        &attr)) {
    return false;
  }

  absoluteMinimumHeatingSetpoint = value;

  return true;
}

bool MatterWaterHeater::setMinimumHeatingSetpoint(
  float temperature
)
{
  if (!initialized) {
    return false;
  }

  int16_t value =
    static_cast<int16_t>(temperature * 100.0f);

  if (value < absoluteMinimumHeatingSetpoint ||
      value > absoluteMaximumHeatingSetpoint) {
    return false;
  }

  esp_matter_attr_val_t attr =
    esp_matter_attr_val(value);

  if (!updateAttributeVal(
        Thermostat::Id,
        Thermostat::Attributes::MinHeatSetpointLimit::Id,
        &attr)) {
    return false;
  }

  minimumHeatingSetpoint = value;

  return true;
}

bool MatterWaterHeater::setAbsoluteMaximumHeatingSetpoint(
  float temperature
)
{
  if (!initialized) {
    return false;
  }

  int16_t value =
    static_cast<int16_t>(temperature * 100.0f);

  esp_matter_attr_val_t attr =
    esp_matter_attr_val(value);

  if (!updateAttributeVal(
        Thermostat::Id,
        Thermostat::Attributes::AbsMaxHeatSetpointLimit::Id,
        &attr)) {
    return false;
  }

  absoluteMaximumHeatingSetpoint = value;

  return true;
}

bool MatterWaterHeater::setMaximumHeatingSetpoint(
  float temperature
)
{
  if (!initialized) {
    return false;
  }

  int16_t value =
    static_cast<int16_t>(temperature * 100.0f);

  if (value < absoluteMinimumHeatingSetpoint ||
      value > absoluteMaximumHeatingSetpoint) {
    return false;
  }

  esp_matter_attr_val_t attr =
    esp_matter_attr_val(value);

  if (!updateAttributeVal(
        Thermostat::Id,
        Thermostat::Attributes::MaxHeatSetpointLimit::Id,
        &attr)) {
    return false;
  }

  maximumHeatingSetpoint = value;

  return true;
}

float MatterWaterHeater::getAbsoluteMinimumHeatingSetpoint()
{
  return static_cast<float>(
    absoluteMinimumHeatingSetpoint
  ) / 100.0f;
}

float MatterWaterHeater::getMinimumHeatingSetpoint()
{
  return static_cast<float>(
    minimumHeatingSetpoint
  ) / 100.0f;
}

float MatterWaterHeater::getAbsoluteMaximumHeatingSetpoint()
{
  return static_cast<float>(
    absoluteMaximumHeatingSetpoint
  ) / 100.0f;
}

float MatterWaterHeater::getMaximumHeatingSetpoint()
{
  return static_cast<float>(
    maximumHeatingSetpoint
  ) / 100.0f;
}

/*
 * --------------------------------------------------------------------------
 * System mode
 * --------------------------------------------------------------------------
 */

bool MatterWaterHeater::setSystemMode(
  SystemMode_t mode
)
{
  if (!initialized) {
    return false;
  }

  if (mode != SYSTEM_MODE_OFF &&
      mode != SYSTEM_MODE_HEAT) {
    return false;
  }

  esp_matter_attr_val_t value =
    esp_matter_attr_val(
      static_cast<uint8_t>(mode)
    );

  if (!updateAttributeVal(
        Thermostat::Id,
        Thermostat::Attributes::SystemMode::Id,
        &value)) {
    return false;
  }

  systemMode = mode;

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
  uint8_t value
)
{
  if (!initialized) {
    return false;
  }

  esp_matter_attr_val_t attr =
    esp_matter_attr_val(
      value,
      esp_matter_attr_val::uint_sub_type::k_bitmap
    );

  if (!updateAttributeVal(
        WaterHeaterManagement::Id,
        WaterHeaterManagement::Attributes::HeaterTypes::Id,
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
  uint8_t value
)
{
  if (!initialized) {
    return false;
  }

  esp_matter_attr_val_t attr =
    esp_matter_attr_val(
      value,
      esp_matter_attr_val::uint_sub_type::k_bitmap
    );

  if (!updateAttributeVal(
        WaterHeaterManagement::Id,
        WaterHeaterManagement::Attributes::HeatDemand::Id,
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
  uint16_t value
)
{
  if (!initialized) {
    return false;
  }

  esp_matter_attr_val_t attr =
    esp_matter_attr_val(value);

  if (!updateAttributeVal(
        WaterHeaterManagement::Id,
        WaterHeaterManagement::Attributes::TankVolume::Id,
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
  uint8_t value
)
{
  if (!initialized || value > 100) {
    return false;
  }

  esp_matter_attr_val_t attr =
    esp_matter_attr_val(value);

  if (!updateAttributeVal(
        WaterHeaterManagement::Id,
        WaterHeaterManagement::Attributes::TankPercentage::Id,
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
  BoostState_t state
)
{
  if (!initialized) {
    return false;
  }

  esp_matter_attr_val_t attr =
    esp_matter_attr_val(
      static_cast<uint8_t>(state),
      esp_matter_attr_val::uint_sub_type::k_enum
    );

  if (!updateAttributeVal(
        WaterHeaterManagement::Id,
        WaterHeaterManagement::Attributes::BoostState::Id,
        &attr)) {
    return false;
  }

  boostState = state;

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
  WaterHeaterMode_t mode
)
{
  if (!initialized) {
    return false;
  }

  if (mode > WATER_HEATER_MODE_ECO) {
    return false;
  }

  esp_matter_attr_val_t value =
    esp_matter_attr_val(
      static_cast<uint8_t>(mode)
    );

  if (!updateAttributeVal(
        WaterHeaterMode::Id,
        WaterHeaterMode::Attributes::CurrentMode::Id,
        &value)) {
    return false;
  }

  waterHeaterMode = mode;

  return true;
}

MatterWaterHeater::WaterHeaterMode_t
MatterWaterHeater::getWaterHeaterMode()
{
  return static_cast<WaterHeaterMode_t>(
    waterHeaterMode
  );
}

#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */