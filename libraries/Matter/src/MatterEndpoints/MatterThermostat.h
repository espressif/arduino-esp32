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

#pragma once
#include <sdkconfig.h>
#ifdef CONFIG_ESP_MATTER_ENABLE_DATA_MODEL

#include <Matter.h>
#include <MatterEndPoint.h>
#include <app-common/zap-generated/cluster-enums.h>

using namespace chip::app::Clusters;

/**
 * @brief Matter thermostat endpoint with temperature control, setpoints, and operating modes.
 *
 * Supports heating and cooling setpoints, local temperature reporting, deadband
 * control for AUTO mode, and callbacks for mode/temperature/setpoint changes.
 * Compatible with Home Assistant, Apple Home, Amazon Alexa, and Google Home.
 */
class MatterThermostat : public MatterEndPoint {
public:
  // clang-format off
  /**
   * @brief Control Sequence of Operation set at @ref begin.
   */
  enum ControlSequenceOfOperation_t {
    THERMOSTAT_SEQ_OP_COOLING                = (uint8_t) Thermostat::ControlSequenceOfOperationEnum::kCoolingOnly,              ///< Cooling only.
    THERMOSTAT_SEQ_OP_COOLING_REHEAT         = (uint8_t) Thermostat::ControlSequenceOfOperationEnum::kCoolingWithReheat,         ///< Cooling with reheat.
    THERMOSTAT_SEQ_OP_HEATING                = (uint8_t) Thermostat::ControlSequenceOfOperationEnum::kHeatingOnly,               ///< Heating only.
    THERMOSTAT_SEQ_OP_HEATING_REHEAT         = (uint8_t) Thermostat::ControlSequenceOfOperationEnum::kHeatingWithReheat,         ///< Heating with reheat.
    THERMOSTAT_SEQ_OP_COOLING_HEATING        = (uint8_t) Thermostat::ControlSequenceOfOperationEnum::kCoolingAndHeating,         ///< Cooling and heating.
    THERMOSTAT_SEQ_OP_COOLING_HEATING_REHEAT = (uint8_t) Thermostat::ControlSequenceOfOperationEnum::kCoolingAndHeatingWithReheat, ///< Cooling and heating with reheat.
  };

  /**
   * @brief Thermostat SystemMode values.
   */
  enum ThermostatMode_t {
    THERMOSTAT_MODE_OFF            = (uint8_t) Thermostat::SystemModeEnum::kOff,           ///< Off.
    THERMOSTAT_MODE_AUTO           = (uint8_t) Thermostat::SystemModeEnum::kAuto,          ///< Auto mode.
    THERMOSTAT_MODE_COOL           = (uint8_t) Thermostat::SystemModeEnum::kCool,          ///< Cooling mode.
    THERMOSTAT_MODE_HEAT           = (uint8_t) Thermostat::SystemModeEnum::kHeat,          ///< Heating mode.
    THERMOSTAT_MODE_EMERGENCY_HEAT = (uint8_t) Thermostat::SystemModeEnum::kEmergencyHeat, ///< Emergency heat.
    THERMOSTAT_MODE_PRECOOLING     = (uint8_t) Thermostat::SystemModeEnum::kPrecooling,    ///< Precooling.
    THERMOSTAT_MODE_FAN_ONLY       = (uint8_t) Thermostat::SystemModeEnum::kFanOnly,       ///< Fan only.
    THERMOSTAT_MODE_DRY            = (uint8_t) Thermostat::SystemModeEnum::kDry,           ///< Dry mode.
    THERMOSTAT_MODE_SLEEP          = (uint8_t) Thermostat::SystemModeEnum::kSleep          ///< Sleep mode.
  };

  /**
   * @brief Whether Auto mode is enabled at @ref begin.
   */
  enum ThermostatAutoMode_t {
    THERMOSTAT_AUTO_MODE_DISABLED = (uint8_t) Thermostat::SystemModeEnum::kOff,   ///< Auto mode disabled.
    THERMOSTAT_AUTO_MODE_ENABLED  = (uint8_t) Thermostat::SystemModeEnum::kAuto,  ///< Auto mode enabled.
  };
  // clang-format on

  /**
   * @brief Create a new Matter thermostat endpoint.
   */
  MatterThermostat();
  /**
   * @brief Destroy the thermostat endpoint object.
   */
  ~MatterThermostat();
  /**
   * @brief Initialize the Matter thermostat endpoint.
   * @param controlSequence Control sequence of operation (default cooling only).
   * @param autoMode Auto mode enabled/disabled (default disabled).
   * @return true if successful, false otherwise.
   */
  bool begin(ControlSequenceOfOperation_t controlSequence = THERMOSTAT_SEQ_OP_COOLING, ThermostatAutoMode_t autoMode = THERMOSTAT_AUTO_MODE_DISABLED);
  /**
   * @brief Stop processing Matter thermostat events.
   */
  void end();

  /**
   * @brief Set the thermostat mode.
   * @param mode Thermostat SystemMode to set.
   * @return true if successful, false otherwise.
   */
  bool setMode(ThermostatMode_t mode);
  /**
   * @brief Get the current thermostat mode.
   * @return Current SystemMode.
   */
  ThermostatMode_t getMode() {
    return currentMode;
  }
  /**
   * @brief Get a friendly string for a thermostat SystemMode value.
   *
   * Includes EMERGENCY_HEAT, PRECOOLING, FAN_ONLY, DRY, and SLEEP.
   * Returns "UNKNOWN" for unused enum value 2 and any out-of-range mode.
   *
   * @param mode SystemMode value.
   * @return Mode name, or "UNKNOWN".
   */
  static const char *getThermostatModeString(uint8_t mode) {
    if (mode >= (sizeof(thermostatModeString) / sizeof(thermostatModeString[0]))) {
      return "UNKNOWN";
    }
    return thermostatModeString[mode];
  }

  /**
   * @brief Get the Control Sequence of Operation set at @ref begin.
   * @return Control sequence.
   */
  ControlSequenceOfOperation_t getControlSequence() {
    return controlSequence;
  }

  /**
   * @brief Get the minimum heating setpoint limit in Celsius.
   * @return Minimum heating setpoint (°C).
   */
  float getMinHeatSetpoint() {
    return (float)kDefaultMinHeatSetpointLimit / 100.00;
  }
  /**
   * @brief Get the maximum heating setpoint limit in Celsius.
   * @return Maximum heating setpoint (°C).
   */
  float getMaxHeatSetpoint() {
    return (float)kDefaultMaxHeatSetpointLimit / 100.00;
  }
  /**
   * @brief Get the minimum cooling setpoint limit in Celsius.
   * @return Minimum cooling setpoint (°C).
   */
  float getMinCoolSetpoint() {
    return (float)kDefaultMinCoolSetpointLimit / 100.00;
  }
  /**
   * @brief Get the maximum cooling setpoint limit in Celsius.
   * @return Maximum cooling setpoint (°C).
   */
  float getMaxCoolSetpoint() {
    return (float)kDefaultMaxCoolSetpointLimit / 100.00;
  }
  /**
   * @brief Get AUTO MinSetpointDeadBand in Celsius (cluster stores tenths of a degree).
   *
   * Reads the attribute when Auto is enabled; otherwise returns the default 2.5 °C.
   *
   * @return Deadband in Celsius.
   */
  float getDeadBand();

  /**
   * @brief Set both cooling and heating setpoints.
   *
   * Heating setpoint must be lower than cooling setpoint. In AUTO mode, cooling
   * setpoint must be at least 2.5 °C higher than heating setpoint (deadband).
   * The Thermostat Matter Server enforces those rules and the max/min limits.
   *
   * @param _setpointHeatingTemperature Heating setpoint in Celsius, or 0xffff to keep current.
   * @param _setpointCoolingTemperature Cooling setpoint in Celsius, or 0xffff to keep current.
   * @return true if successful, false otherwise.
   */
  bool setCoolingHeatingSetpoints(double _setpointHeatingTemperature, double _setpointCoolingTemperature);

  /**
   * @brief Set the heating setpoint in Celsius.
   * @param _setpointHeatingTemperature Heating setpoint in Celsius.
   * @return true if successful, false otherwise.
   */
  bool setHeatingSetpoint(double _setpointHeatingTemperature) {
    return setCoolingHeatingSetpoints(_setpointHeatingTemperature, (double)0xffff);
  }
  /**
   * @brief Get the heating setpoint in Celsius.
   * @return Heating setpoint (°C) with 1/100th degree precision.
   */
  double getHeatingSetpoint() {
    return heatingSetpointTemperature / 100.0;
  }
  /**
   * @brief Set the cooling setpoint in Celsius.
   * @param _setpointCoolingTemperature Cooling setpoint in Celsius.
   * @return true if successful, false otherwise.
   */
  bool setCoolingSetpoint(double _setpointCoolingTemperature) {
    return setCoolingHeatingSetpoints((double)0xffff, _setpointCoolingTemperature);
  }
  /**
   * @brief Get the cooling setpoint in Celsius.
   * @return Cooling setpoint (°C) with 1/100th degree precision.
   */
  double getCoolingSetpoint() {
    return coolingSetpointTemperature / 100.0;
  }

  /**
   * @brief Set the local thermostat temperature in Celsius.
   * @param temperature Temperature in Celsius.
   * @return true if successful, false otherwise.
   */
  bool setLocalTemperature(double temperature) {
    // stores up to 1/100th of a Celsio degree precision
    int16_t rawValue = static_cast<int16_t>(temperature * 100.0f);
    return setRawTemperature(rawValue, Thermostat::Attributes::LocalTemperature::Id, &localTemperature);
  }
  /**
   * @brief Get the current local temperature in Celsius.
   * @return Temperature (°C) with 1/100th degree precision.
   */
  double getLocalTemperature() {
    return (double)localTemperature / 100.0;
  }

  /**
   * @brief Callback invoked when thermostat mode changes.
   * @param mode New thermostat mode.
   * @return true if the change was handled successfully.
   */
  using EndPointModeCB = std::function<bool(ThermostatMode_t)>;
  /**
   * @brief Register a callback for mode changes from the Matter controller.
   * @param onChangeCB Function to call when mode changes.
   */
  void onChangeMode(EndPointModeCB onChangeCB) {
    _onChangeModeCB = onChangeCB;
  }

  /**
   * @brief Callback invoked when local temperature changes.
   * @param temperature New local temperature in Celsius.
   * @return true if the change was handled successfully.
   */
  using EndPointTemperatureCB = std::function<bool(float)>;
  /**
   * @brief Register a callback for local temperature changes from the Matter controller.
   * @param onChangeCB Function to call when local temperature changes.
   */
  void onChangeLocalTemperature(EndPointTemperatureCB onChangeCB) {
    _onChangeTemperatureCB = onChangeCB;
  }

  /**
   * @brief Callback invoked when the cooling setpoint changes.
   * @param setpoint New cooling setpoint in Celsius.
   * @return true if the change was handled successfully.
   */
  using EndPointCoolingSetpointCB = std::function<bool(double)>;
  /**
   * @brief Register a callback for cooling setpoint changes from the Matter controller.
   * @param onChangeCB Function to call when cooling setpoint changes.
   */
  void onChangeCoolingSetpoint(EndPointCoolingSetpointCB onChangeCB) {
    _onChangeCoolingSetpointCB = onChangeCB;
  }

  /**
   * @brief Callback invoked when the heating setpoint changes.
   * @param setpoint New heating setpoint in Celsius.
   * @return true if the change was handled successfully.
   */
  using EndPointHeatingSetpointCB = std::function<bool(double)>;
  /**
   * @brief Register a callback for heating setpoint changes from the Matter controller.
   * @param onChangeCB Function to call when heating setpoint changes.
   */
  void onChangeHeatingSetpoint(EndPointHeatingSetpointCB onChangeCB) {
    _onChangeHeatingSetpointCB = onChangeCB;
  }

  /**
   * @brief Callback invoked when any thermostat parameter changes.
   *
   * Read current values with getMode(), getLocalTemperature(), getCoolingSetpoint(),
   * and getHeatingSetpoint().
   *
   * @return true if the change was handled successfully.
   */
  using EndPointCB = std::function<bool(void)>;
  /**
   * @brief Register a callback for any parameter change from the Matter controller.
   * @param onChangeCB Function to call when state changes.
   */
  void onChange(EndPointCB onChangeCB) {
    _onChangeCB = onChangeCB;
  }

  /**
   * @brief Called by the Matter internal event processor when an attribute changes.
   *
   * Can be overridden by the application if necessary.
   *
   * @param endpoint_id Endpoint ID where the attribute changed.
   * @param cluster_id Cluster ID of the changed attribute.
   * @param attribute_id Attribute ID that changed.
   * @param val Pointer to the new attribute value.
   * @return true if the change was handled successfully.
   */
  bool attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val);

protected:
  bool started = false;
  // implementation keeps temperature in 1/100th of a Celsio degree
  int16_t coolingSetpointTemperature = 2400;  // 24C cooling setpoint
  int16_t localTemperature = 2000;            // 20C local temperature
  int16_t heatingSetpointTemperature = 1600;  // 16C heating setpoint

  ThermostatMode_t currentMode = THERMOSTAT_MODE_OFF;
  ControlSequenceOfOperation_t controlSequence = THERMOSTAT_SEQ_OP_COOLING;
  ThermostatAutoMode_t autoMode = THERMOSTAT_AUTO_MODE_DISABLED;

  EndPointModeCB _onChangeModeCB = NULL;
  EndPointTemperatureCB _onChangeTemperatureCB = NULL;
  EndPointCoolingSetpointCB _onChangeCoolingSetpointCB = NULL;
  EndPointHeatingSetpointCB _onChangeHeatingSetpointCB = NULL;
  EndPointCB _onChangeCB = NULL;

  // internal function to set the raw temperature value (Matter Cluster)
  bool setRawTemperature(int16_t _rawTemperature, uint32_t attribute_id, int16_t *internalValue);

  // clang-format off
  // Default Thermostat values - can't be changed - defined in the Thermostat Cluster Server code
  static const int16_t kDefaultAbsMinHeatSetpointLimit = 700;  // 7C (44.5 F)
  static const int16_t kDefaultMinHeatSetpointLimit    = 700;  // 7C (44.5 F)
  static const int16_t kDefaultAbsMaxHeatSetpointLimit = 3000; // 30C (86 F)
  static const int16_t kDefaultMaxHeatSetpointLimit    = 3000; // 30C (86 F)

  static const int16_t kDefaultAbsMinCoolSetpointLimit = 1600; // 16C (61 F)
  static const int16_t kDefaultMinCoolSetpointLimit    = 1600; // 16C (61 F)
  static const int16_t kDefaultAbsMaxCoolSetpointLimit = 3200; // 32C (90 F)
  static const int16_t kDefaultMaxCoolSetpointLimit    = 3200; // 32C (90 F)

  static const int8_t  kDefaultDeadBand                = 25; // 2.5C when in AUTO mode
  // clang-format on

  // string helper for the THERMOSTAT MODE
  static const char *thermostatModeString[10];
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
