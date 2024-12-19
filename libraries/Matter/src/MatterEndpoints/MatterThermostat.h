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

#pragma once
#include <sdkconfig.h>
#ifdef CONFIG_ESP_MATTER_ENABLE_DATA_MODEL

#include <Matter.h>
#include <MatterEndPoint.h>
#include <app-common/zap-generated/cluster-enums.h>

using namespace chip::app::Clusters;

class MatterThermostat : public MatterEndPoint {
public:
  // clang-format off
  enum ControlSequenceOfOperation_t {
    THERMOSTAT_SEQ_OP_COOLING                = (uint8_t) Thermostat::ControlSequenceOfOperationEnum::kCoolingOnly,
    THERMOSTAT_SEQ_OP_COOLING_REHEAT         = (uint8_t) Thermostat::ControlSequenceOfOperationEnum::kCoolingWithReheat,
    THERMOSTAT_SEQ_OP_HEATING                = (uint8_t) Thermostat::ControlSequenceOfOperationEnum::kHeatingOnly,
    THERMOSTAT_SEQ_OP_HEATING_REHEAT         = (uint8_t) Thermostat::ControlSequenceOfOperationEnum::kHeatingWithReheat,
    THERMOSTAT_SEQ_OP_COOLING_HEATING        = (uint8_t) Thermostat::ControlSequenceOfOperationEnum::kCoolingAndHeating,
    THERMOSTAT_SEQ_OP_COOLING_HEATING_REHEAT = (uint8_t) Thermostat::ControlSequenceOfOperationEnum::kCoolingAndHeatingWithReheat,
  };

  enum ThermostatMode_t {
    THERMOSTAT_MODE_OFF            = (uint8_t) Thermostat::SystemModeEnum::kOff,
    THERMOSTAT_MODE_AUTO           = (uint8_t) Thermostat::SystemModeEnum::kAuto,
    THERMOSTAT_MODE_COOL           = (uint8_t) Thermostat::SystemModeEnum::kCool,
    THERMOSTAT_MODE_HEAT           = (uint8_t) Thermostat::SystemModeEnum::kHeat,
    THERMOSTAT_MODE_EMERGENCY_HEAT = (uint8_t) Thermostat::SystemModeEnum::kEmergencyHeat,
    THERMOSTAT_MODE_PRECOOLING     = (uint8_t) Thermostat::SystemModeEnum::kPrecooling,
    THERMOSTAT_MODE_FAN_ONLY       = (uint8_t) Thermostat::SystemModeEnum::kFanOnly,
    THERMOSTAT_MODE_DRY            = (uint8_t) Thermostat::SystemModeEnum::kDry,
    THERMOSTAT_MODE_SLEEP          = (uint8_t) Thermostat::SystemModeEnum::kSleep
  };

  enum ThermostatAutoMode_t {
    THERMOSTAT_AUTO_MODE_DISABLED = (uint8_t) Thermostat::SystemModeEnum::kOff,
    THERMOSTAT_AUTO_MODE_ENABLED  = (uint8_t) Thermostat::SystemModeEnum::kAuto,
  };
  // clang-format on

  MatterThermostat();
  ~MatterThermostat();
  // begin Matter Thermostat endpoint with initial Operation Mode
  bool begin(ControlSequenceOfOperation_t controlSequence = THERMOSTAT_SEQ_OP_COOLING, ThermostatAutoMode_t autoMode = THERMOSTAT_AUTO_MODE_DISABLED);
  // this will stop processing Thermostat Matter events
  void end();

  // set the Thermostat Mode
  bool setMode(ThermostatMode_t mode);
  // get the Thermostat Mode
  ThermostatMode_t getMode() {
    return currentMode;
  }
  // returns a friendly string for the Fan Mode
  static const char *getThermostatModeString(uint8_t mode) {
    return thermostatModeString[mode];
  }

  // get the Thermostat Control Sequence of Operation
  ControlSequenceOfOperation_t getControlSequence() {
    return controlSequence;
  }

  // get the minimum heating setpoint in 1/100th of a Celsio degree
  float getMinHeatSetpoint() {
    return (float)kDefaultMinHeatSetpointLimit / 100.00;
  }
  // get the maximum heating setpoint in 1/100th of a Celsio degree
  float getMaxHeatSetpoint() {
    return (float)kDefaultMaxHeatSetpointLimit / 100.00;
  }
  // get the minimum cooling setpoint in 1/100th of a Celsio degree
  float getMinCoolSetpoint() {
    return (float)kDefaultMinCoolSetpointLimit / 100.00;
  }
  // get the maximum cooling setpoint in 1/100th of a Celsio degree
  float getMaxCoolSetpoint() {
    return (float)kDefaultMaxCoolSetpointLimit / 100.00;
  }
  // get the deadband in 1/10th of a Celsio degree
  float getDeadBand() {
    return (float)kDefaultDeadBand / 10.00;
  }

  // generic function for setting the cooling and heating setpoints - checks if the setpoints are valid
  // it can be used to set both setpoints at the same time or only one of them, by setting the other to (float)0xffff
  // Heating Setpoint must be lower than Cooling Setpoint
  // When using AUTO mode the Cooling Setpoint must be higher than Heating Setpoint by at least the 2.5C (deadband)
  // Thermostat Matter Server will enforce those rules and the Max/Min setpoints limits as in the Matter Specification
  bool setCoolingHeatingSetpoints(double _setpointHeatingTemperature, double _setpointCollingTemperature);

  // set the heating setpoint in 1/100th of a Celsio degree
  bool setHeatingSetpoint(double _setpointHeatingTemperature) {
    return setCoolingHeatingSetpoints((double)0xffff, _setpointHeatingTemperature);
  }
  // get the heating setpoint in 1/100th of a Celsio degree
  double getHeatingSetpoint() {
    return heatingSetpointTemperature / 100.0;
  }
  // set the cooling setpoint in 1/100th of a Celsio degree
  bool setCoolingSetpoint(double _setpointCollingTemperature) {
    return setCoolingHeatingSetpoints(_setpointCollingTemperature, (double)0xffff);
  }
  // get the cooling setpoint in 1/100th of a Celsio degree
  double getCoolingSetpoint() {
    return coolingSetpointTemperature / 100.0;
  }

  // set the local Thermostat temperature in  Celsio degrees
  bool setLocalTemperature(double temperature) {
    // stores up to 1/100th of a Celsio degree precision
    int16_t rawValue = static_cast<int16_t>(temperature * 100.0f);
    return setRawTemperature(rawValue, Thermostat::Attributes::LocalTemperature::Id, &localTemperature);
  }
  // returns the local Thermostat float temperature with 1/100th of a Celsio degree precision
  double getLocalTemperature() {
    return (double)localTemperature / 100.0;
  }

  // User Callback for whenever the Thermostat Mode is changed by the Matter Controller
  using EndPointModeCB = std::function<bool(ThermostatMode_t)>;
  void onChangeMode(EndPointModeCB onChangeCB) {
    _onChangeModeCB = onChangeCB;
  }

  // User Callback for whenever the Local Temperature is changed by the Matter Controller
  using EndPointTemperatureCB = std::function<bool(float)>;
  void onChangeLocalTemperature(EndPointTemperatureCB onChangeCB) {
    _onChangeTemperatureCB = onChangeCB;
  }

  // User Callback for whenever the Cooling or Heating Setpoint is changed by the Matter Controller
  using EndPointCoolingSetpointCB = std::function<bool(double)>;
  void onChangeCoolingSetpoint(EndPointCoolingSetpointCB onChangeCB) {
    _onChangeCoolingSetpointCB = onChangeCB;
  }

  // User Callback for whenever the Cooling or Heating Setpoint is changed by the Matter Controller
  using EndPointHeatingSetpointCB = std::function<bool(double)>;
  void onChangeHeatingSetpoint(EndPointHeatingSetpointCB onChangeCB) {
    _onChangeHeatingSetpointCB = onChangeCB;
  }

  // User Callback for whenever any parameter is changed by the Matter Controller
  // Main parameters are Thermostat Mode, Local Temperature, Cooling Setpoint and Heating Setpoint
  // Those can be obtained using getMode(), getTemperature(), getCoolingSetpoint() and getHeatingSetpoint()
  using EndPointCB = std::function<bool(void)>;
  void onChange(EndPointCB onChangeCB) {
    _onChangeCB = onChangeCB;
  }

  // this function is called by Matter internal event processor. It could be overwritten by the application, if necessary.
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
  static const char *thermostatModeString[5];
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
