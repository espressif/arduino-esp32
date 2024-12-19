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
#include <MatterEndpoints/MatterThermostat.h>

using namespace esp_matter;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

// string helper for the THERMOSTAT MODE
const char *MatterThermostat::thermostatModeString[5] = {"OFF", "AUTO", "UNKNOWN", "COOL", "HEAT"};

// endpoint for color light device
namespace esp_matter {
using namespace cluster;
namespace endpoint {
namespace multi_mode_thermostat {
typedef struct config {
  cluster::descriptor::config_t descriptor;
  cluster::identify::config_t identify;
  cluster::scenes_management::config_t scenes_management;
  cluster::groups::config_t groups;
  cluster::thermostat::config_t thermostat;
} config_t;

uint32_t get_device_type_id() {
  return ESP_MATTER_THERMOSTAT_DEVICE_TYPE_ID;
}

uint8_t get_device_type_version() {
  return ESP_MATTER_THERMOSTAT_DEVICE_TYPE_VERSION;
}

esp_err_t add(endpoint_t *endpoint, config_t *config) {
  if (!endpoint) {
    log_e("Endpoint cannot be NULL");
    return ESP_ERR_INVALID_ARG;
  }
  esp_err_t err = add_device_type(endpoint, get_device_type_id(), get_device_type_version());
  if (err != ESP_OK) {
    log_e("Failed to add device type id:%" PRIu32 ",err: %d", get_device_type_id(), err);
    return err;
  }

  descriptor::create(endpoint, &(config->descriptor), CLUSTER_FLAG_SERVER);
  identify::create(endpoint, &(config->identify), CLUSTER_FLAG_SERVER);
  groups::create(endpoint, &(config->groups), CLUSTER_FLAG_SERVER);
  uint32_t thermostatFeatures = 0;
  switch (config->thermostat.control_sequence_of_operation) {
    case MatterThermostat::THERMOSTAT_SEQ_OP_COOLING:
    case MatterThermostat::THERMOSTAT_SEQ_OP_COOLING_REHEAT: thermostatFeatures = cluster::thermostat::feature::cooling::get_id(); break;
    case MatterThermostat::THERMOSTAT_SEQ_OP_HEATING:
    case MatterThermostat::THERMOSTAT_SEQ_OP_HEATING_REHEAT: thermostatFeatures = cluster::thermostat::feature::heating::get_id(); break;
    case MatterThermostat::THERMOSTAT_SEQ_OP_COOLING_HEATING:
    case MatterThermostat::THERMOSTAT_SEQ_OP_COOLING_HEATING_REHEAT:
      thermostatFeatures = cluster::thermostat::feature::cooling::get_id() | cluster::thermostat::feature::heating::get_id();
      break;
  }
  cluster::thermostat::create(endpoint, &(config->thermostat), CLUSTER_FLAG_SERVER, thermostatFeatures);
  return ESP_OK;
}

endpoint_t *create(node_t *node, config_t *config, uint8_t flags, void *priv_data) {
  endpoint_t *endpoint = endpoint::create(node, flags, priv_data);
  add(endpoint, config);
  return endpoint;
}
}  // namespace multi_mode_thermostat
}  // namespace endpoint
}  // namespace esp_matter

bool MatterThermostat::attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) {
  bool ret = true;
  if (!started) {
    log_e("Matter Thermostat device has not begun.");
    return false;
  }
  log_d("Thermostat Attr update callback: endpoint: %u, cluster: %u, attribute: %u, val: %u", endpoint_id, cluster_id, attribute_id, val->val.u32);

  if (cluster_id == Thermostat::Id) {
    switch (attribute_id) {
      case Thermostat::Attributes::SystemMode::Id:
        if (_onChangeModeCB != NULL) {
          ret &= _onChangeModeCB((ThermostatMode_t)val->val.u8);
        }
        if (_onChangeCB != NULL) {
          ret &= _onChangeCB();
        }
        if (ret == true) {
          currentMode = (ThermostatMode_t)val->val.u8;
          log_v("Thermostat Mode updated to %d", val->val.u8);
        }
        break;
      case Thermostat::Attributes::LocalTemperature::Id:
        if (_onChangeTemperatureCB != NULL) {
          ret &= _onChangeTemperatureCB((float)val->val.i16 / 100.00);
        }
        if (_onChangeCB != NULL) {
          ret &= _onChangeCB();
        }
        if (ret == true) {
          localTemperature = val->val.i16;
          log_v("Local Temperature updated to %.01fC", (float)val->val.i16 / 100.00);
        }
        break;
      case Thermostat::Attributes::OccupiedCoolingSetpoint::Id:
        if (_onChangeCoolingSetpointCB != NULL) {
          ret &= _onChangeCoolingSetpointCB((float)val->val.i16 / 100.00);
        }
        if (_onChangeCB != NULL) {
          ret &= _onChangeCB();
        }
        if (ret == true) {
          coolingSetpointTemperature = val->val.i16;
          log_v("Cooling Setpoint updated to %.01fC", (float)val->val.i16 / 100.00);
        }
        break;
      case Thermostat::Attributes::OccupiedHeatingSetpoint::Id:
        if (_onChangeHeatingSetpointCB != NULL) {
          ret &= _onChangeHeatingSetpointCB((float)val->val.i16 / 100.00);
        }
        if (_onChangeCB != NULL) {
          ret &= _onChangeCB();
        }
        if (ret == true) {
          heatingSetpointTemperature = val->val.i16;
          log_v("Heating Setpoint updated to %.01fC", (float)val->val.i16 / 100.00);
        }
        break;
      default: log_w("Unhandled Thermostat Attribute ID: %u", attribute_id); break;
    }
  }
  return ret;
}

MatterThermostat::MatterThermostat() {}

MatterThermostat::~MatterThermostat() {
  end();
}

bool MatterThermostat::begin(ControlSequenceOfOperation_t _controlSequence, ThermostatAutoMode_t _autoMode) {
  ArduinoMatter::_init();

  if (getEndPointId() != 0) {
    log_e("Matter Thermostat with Endpoint Id %d device has already been created.", getEndPointId());
    return false;
  }

  // check if auto mode is allowed with the control sequence of operation - only allowed for Cooling & Heating
  if (_autoMode == THERMOSTAT_AUTO_MODE_ENABLED && _controlSequence != THERMOSTAT_SEQ_OP_COOLING_HEATING
      && _controlSequence != THERMOSTAT_SEQ_OP_COOLING_HEATING_REHEAT) {
    log_e("Thermostat in Auto Mode requires a Cooling & Heating Control Sequence of Operation.");
    return false;
  }

  const int16_t _localTemperature = 2000;            // initial value to be automatically changed by the Matter Thermostat
  const int16_t _coolingSetpointTemperature = 2400;  // 24C cooling setpoint
  const int16_t _heatingSetpointTemperature = 1600;  // 16C heating setpoint
  const ThermostatMode_t _currentMode = THERMOSTAT_MODE_OFF;

  multi_mode_thermostat::config_t thermostat_config;
  thermostat_config.thermostat.control_sequence_of_operation = (uint8_t)_controlSequence;
  thermostat_config.thermostat.cooling.occupied_cooling_setpoint = _coolingSetpointTemperature;
  thermostat_config.thermostat.heating.occupied_heating_setpoint = _heatingSetpointTemperature;
  thermostat_config.thermostat.system_mode = (uint8_t)_currentMode;
  thermostat_config.thermostat.local_temperature = _localTemperature;

  // endpoint handles can be used to add/modify clusters
  endpoint_t *endpoint = multi_mode_thermostat::create(node::get(), &thermostat_config, ENDPOINT_FLAG_NONE, (void *)this);
  if (endpoint == nullptr) {
    log_e("Failed to create Thermostat endpoint");
    return false;
  }
  if (_autoMode == THERMOSTAT_AUTO_MODE_ENABLED) {
    cluster_t *cluster = cluster::get(endpoint, Thermostat::Id);
    thermostat_config.thermostat.auto_mode.min_setpoint_dead_band = kDefaultDeadBand;  // fixed by default to 2.5C
    cluster::thermostat::feature::auto_mode::add(cluster, &thermostat_config.thermostat.auto_mode);
  }

  controlSequence = _controlSequence;
  autoMode = _autoMode;
  coolingSetpointTemperature = _coolingSetpointTemperature;
  heatingSetpointTemperature = _heatingSetpointTemperature;
  localTemperature = _localTemperature;
  currentMode = _currentMode;

  setEndPointId(endpoint::get_id(endpoint));
  log_i("Thermostat created with endpoint_id %d", getEndPointId());
  started = true;
  return true;
}

void MatterThermostat::end() {
  started = false;
}

bool MatterThermostat::setMode(ThermostatMode_t _mode) {
  if (!started) {
    log_e("Matter Thermostat device has not begun.");
    return false;
  }

  if (autoMode == THERMOSTAT_AUTO_MODE_DISABLED && _mode == THERMOSTAT_MODE_AUTO) {
    log_e("Thermostat can't set Auto Mode.");
    return false;
  }
  // check if the requested mode is valid based on the control sequence of operation
  // no restrictions for OFF mode
  if (_mode != THERMOSTAT_MODE_OFF) {
    // check HEAT, COOL and AUTO modes
    switch (controlSequence) {
      case THERMOSTAT_SEQ_OP_COOLING:
      case THERMOSTAT_SEQ_OP_COOLING_REHEAT:
        if (_mode == THERMOSTAT_MODE_HEAT || _mode == THERMOSTAT_MODE_AUTO) {
          break;
        }
        log_e("Invalid Thermostat Mode for Cooling Control Sequence of Operation.");
        return false;
      case THERMOSTAT_SEQ_OP_HEATING:
      case THERMOSTAT_SEQ_OP_HEATING_REHEAT:
        if (_mode == THERMOSTAT_MODE_COOL || _mode == THERMOSTAT_MODE_AUTO) {
          break;
        }
        log_e("Invalid Thermostat Mode for Heating Control Sequence of Operation.");
        return false;
      default:
        // compiler warning about not handling all enum values
        break;
    }
  }

  // avoid processing if there was no change
  if (currentMode == _mode) {
    return true;
  }

  esp_matter_attr_val_t modeVal = esp_matter_invalid(NULL);
  if (!getAttributeVal(Thermostat::Id, Thermostat::Attributes::SystemMode::Id, &modeVal)) {
    log_e("Failed to get Thermostat Mode Attribute.");
    return false;
  }
  if (modeVal.val.u8 != _mode) {
    modeVal.val.u8 = _mode;
    bool ret;
    ret = updateAttributeVal(Thermostat::Id, Thermostat::Attributes::SystemMode::Id, &modeVal);
    if (!ret) {
      log_e("Failed to update Thermostat Mode Attribute.");
      return false;
    }
    currentMode = _mode;
  }
  log_v("Thermostat Mode set to %d", _mode);

  return true;
}

bool MatterThermostat::setRawTemperature(int16_t _rawTemperature, uint32_t attribute_id, int16_t *internalValue) {
  if (!started) {
    log_e("Matter Thermostat device has not begun.");
    return false;
  }

  // avoid processing if there was no change
  if (*internalValue == _rawTemperature) {
    return true;
  }

  esp_matter_attr_val_t temperatureVal = esp_matter_invalid(NULL);
  if (!getAttributeVal(Thermostat::Id, attribute_id, &temperatureVal)) {
    log_e("Failed to get Thermostat Temperature or Setpoint Attribute.");
    return false;
  }
  if (temperatureVal.val.i16 != _rawTemperature) {
    temperatureVal.val.i16 = _rawTemperature;
    bool ret;
    ret = updateAttributeVal(Thermostat::Id, attribute_id, &temperatureVal);
    if (!ret) {
      log_e("Failed to update Thermostat Temperature or Setpoint Attribute.");
      return false;
    }
    *internalValue = _rawTemperature;
  }
  log_v("Temperature set to %.01fC", (float)_rawTemperature / 100.00);

  return true;
}

bool MatterThermostat::setCoolingHeatingSetpoints(double _setpointHeatingTemperature, double _setpointCollingTemperature) {
  // at least one of the setpoints must be valid
  bool settingCooling = _setpointCollingTemperature != (float)0xffff;
  bool settingHeating = _setpointHeatingTemperature != (float)0xffff;
  if (!settingCooling && !settingHeating) {
    log_e("Invalid Setpoints values. Set correctly at least one of them in Celsius.");
    return false;
  }
  int16_t _rawHeatValue = static_cast<int16_t>(_setpointHeatingTemperature * 100.0f);
  int16_t _rawCoolValue = static_cast<int16_t>(_setpointCollingTemperature * 100.0f);

  // check limits for the setpoints
  if (settingHeating && (_rawHeatValue < kDefaultMinHeatSetpointLimit || _rawHeatValue > kDefaultMaxHeatSetpointLimit)) {
    log_e(
      "Invalid Heating Setpoint value: %.01fC - valid range %d..%d", _setpointHeatingTemperature, kDefaultMinHeatSetpointLimit / 100,
      kDefaultMaxHeatSetpointLimit / 100
    );
    return false;
  }
  if (settingCooling && (_rawCoolValue < kDefaultMinCoolSetpointLimit || _rawCoolValue > kDefaultMaxCoolSetpointLimit)) {
    log_e(
      "Invalid Cooling Setpoint value: %.01fC - valid range %d..%d", _setpointCollingTemperature, kDefaultMinCoolSetpointLimit / 100,
      kDefaultMaxCoolSetpointLimit / 100
    );
    return false;
  }

  // AUTO mode requires both setpoints to be valid to each other and respect the deadband
  if (currentMode == THERMOSTAT_MODE_AUTO) {
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_ERROR
    float deadband = getDeadBand();
#endif
    // only setting Cooling Setpoint
    if (settingCooling && !settingHeating && _rawCoolValue < (heatingSetpointTemperature + (kDefaultDeadBand * 10))) {
      log_e(
        "AutoMode :: Invalid Cooling Setpoint value: %.01fC - must be higher or equal than %.01fC", _setpointCollingTemperature, getHeatingSetpoint() + deadband
      );
      return false;
    }
    // only setting Heating Setpoint
    if (!settingCooling && settingHeating && _rawHeatValue > (coolingSetpointTemperature - (kDefaultDeadBand * 10))) {
      log_e(
        "AutoMode :: Invalid Heating Setpoint value: %.01fC - must be lower or equal than %.01fC", _setpointHeatingTemperature, getCoolingSetpoint() - deadband
      );
      return false;
    }
    // setting both setpoints
    if (settingCooling && settingHeating && (_rawCoolValue <= _rawHeatValue || _rawCoolValue - _rawHeatValue < kDefaultDeadBand * 10.0)) {
      log_e(
        "AutoMode :: Error - Heating Setpoint %.01fC must be lower than Cooling Setpoint %.01fC with a minimum difference of %0.1fC",
        _setpointHeatingTemperature, _setpointCollingTemperature, deadband
      );
      return false;
    }
  }

  bool ret = true;
  if (settingCooling) {
    ret &= setRawTemperature(_rawCoolValue, Thermostat::Attributes::OccupiedCoolingSetpoint::Id, &coolingSetpointTemperature);
  }
  if (settingHeating) {
    ret &= setRawTemperature(_rawHeatValue, Thermostat::Attributes::OccupiedHeatingSetpoint::Id, &heatingSetpointTemperature);
  }
  return ret;
}

#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
