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

#include "ZigbeeHVAC.h"
#if CONFIG_ZB_ENABLED
#include <cmath>
#include <cstring>
#include "ezbee/zha.h"
#include "ezbee/zcl/cluster/thermostat.h"
#include "ezbee/zcl/cluster/thermostat_desc.h"

static bool hvac_system_mode_valid(uint8_t mode) {
  switch (mode) {
    case HVAC_SYSTEM_MODE_OFF:
    case HVAC_SYSTEM_MODE_AUTO:
    case HVAC_SYSTEM_MODE_COOL:
    case HVAC_SYSTEM_MODE_HEAT:
    case HVAC_SYSTEM_MODE_EMERGENCY_HEAT:
    case HVAC_SYSTEM_MODE_PRECOOLING:
    case HVAC_SYSTEM_MODE_FAN_ONLY:
    case HVAC_SYSTEM_MODE_DRY:
    case HVAC_SYSTEM_MODE_SLEEP:  return true;
    default:                      return false;
  }
}

static bool hvac_sequence_valid(uint8_t sequence) {
  return sequence <= HVAC_SEQ_COOLING_AND_HEATING_WITH_REHEAT;
}

// Thermostat temperatures are int16 in units of 0.01°C. 0x8000 is the spec's "invalid" local temperature,
// and the stack rejects it on a checked write, so callers pass a real temperature instead.
static bool hvac_celsius_to_centi(float celsius, int16_t *centi) {
  if (!std::isfinite(celsius)) {
    log_e("HVAC temperature is not a finite number");
    return false;
  }
  long rounded = std::lround(celsius * 100.0);
  if (rounded < EZB_ZCL_THERMOSTAT_LOCAL_TEMPERATURE_MIN_VALUE || rounded > EZB_ZCL_THERMOSTAT_LOCAL_TEMPERATURE_MAX_VALUE) {
    log_e("HVAC temperature out of range: %.2f°C", celsius);
    return false;
  }
  *centi = (int16_t)rounded;
  return true;
}

static bool hvac_read_u8(const ezb_zcl_set_attr_value_message_t *message, uint8_t *value) {
  if (message->in.attribute.data.value == nullptr || message->in.attribute.data.size < sizeof(uint8_t)) {
    return false;
  }
  memcpy(value, message->in.attribute.data.value, sizeof(uint8_t));
  return true;
}

static bool hvac_read_s16(const ezb_zcl_set_attr_value_message_t *message, int16_t *value) {
  if (message->in.attribute.data.value == nullptr || message->in.attribute.data.size < sizeof(int16_t)) {
    return false;
  }
  memcpy(value, message->in.attribute.data.value, sizeof(int16_t));
  return true;
}

// ZHA configures reporting for these. The stack marks an attribute unreportable unless this bit is set,
// and then Home Assistant never sees later changes.
static bool hvac_enable_reporting(ezb_zcl_cluster_desc_t cluster, uint16_t attr_id) {
  ezb_zcl_attr_desc_t attr = ezb_zcl_cluster_get_attr_desc(cluster, attr_id, EZB_ZCL_STD_MANUF_CODE);
  if (attr == nullptr) {
    log_e("HVAC attribute 0x%04x is missing, reporting stays disabled", attr_id);
    return false;
  }
  ezb_zcl_attr_access_t access = (ezb_zcl_attr_access_t)(ezb_zcl_attr_desc_get_access(attr) | EZB_ZCL_ATTR_ACCESS_REPORTING);
  if (ezb_zcl_attr_desc_set_access(attr, access) != EZB_ERR_NONE) {
    log_e("Failed to enable reporting for HVAC attribute 0x%04x", attr_id);
    return false;
  }
  return true;
}

ZigbeeHVAC::ZigbeeHVAC(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = EZB_ZHA_THERMOSTAT_DEVICE_ID;
  _on_hvac_change = nullptr;
  _system_mode = EZB_ZCL_THERMOSTAT_SYSTEM_MODE_DEFAULT_VALUE;
  _control_sequence = EZB_ZCL_THERMOSTAT_CONTROL_SEQUENCE_OF_OPERATION_DEFAULT_VALUE;
  _occupied_heating = EZB_ZCL_THERMOSTAT_OCCUPIED_HEATING_SETPOINT_DEFAULT_VALUE;
  _occupied_cooling = EZB_ZCL_THERMOSTAT_OCCUPIED_COOLING_SETPOINT_DEFAULT_VALUE;
  _local_temperature = EZB_ZCL_THERMOSTAT_LOCAL_TEMPERATURE_DEFAULT_VALUE;
  _min_dead_band = EZB_ZCL_THERMOSTAT_MIN_SETPOINT_DEAD_BAND_DEFAULT_VALUE;
  _running_state = HVAC_RUNNING_OFF;

  // ezb_zha_create_thermostat() installs LocalTemperature, ControlSequenceOfOperation and SystemMode only.
  // Occupied setpoints are mandatory for a heat+cool server, and SetpointRaiseOrLower asserts if neither exists.
  // MinSetpointDeadBand is read-only; without it the stack does not enforce the 2.5°C gap.
  _ep_config = {
    .ep_id = _endpoint, .app_profile_id = EZB_AF_HA_PROFILE_ID, .app_device_id = EZB_ZHA_THERMOSTAT_DEVICE_ID, .app_device_version = 0, .reserved = 0
  };
  ezb_zha_thermostat_config_t thermostat_cfg = EZB_ZHA_THERMOSTAT_CONFIG();
  _ep_desc = ezb_zha_create_thermostat(_endpoint, &thermostat_cfg);
  if (_ep_desc == nullptr) {
    log_e("Failed to create HVAC endpoint descriptor");
    return;
  }

  ezb_zcl_cluster_desc_t thermostat = ezb_af_endpoint_get_cluster_desc(_ep_desc, EZB_ZCL_CLUSTER_ID_THERMOSTAT, EZB_ZCL_CLUSTER_SERVER);
  if (thermostat == nullptr) {
    log_e("Thermostat cluster not found on HVAC endpoint");
    return;
  }
  if (ezb_zcl_thermostat_cluster_desc_add_attr(thermostat, EZB_ZCL_ATTR_THERMOSTAT_OCCUPIED_HEATING_SETPOINT_ID, &_occupied_heating) != EZB_ERR_NONE) {
    log_e("Failed to add OccupiedHeatingSetpoint");
  }
  if (ezb_zcl_thermostat_cluster_desc_add_attr(thermostat, EZB_ZCL_ATTR_THERMOSTAT_OCCUPIED_COOLING_SETPOINT_ID, &_occupied_cooling) != EZB_ERR_NONE) {
    log_e("Failed to add OccupiedCoolingSetpoint");
  }
  if (ezb_zcl_thermostat_cluster_desc_add_attr(thermostat, EZB_ZCL_ATTR_THERMOSTAT_MIN_SETPOINT_DEAD_BAND_ID, &_min_dead_band) != EZB_ERR_NONE) {
    log_e("Failed to add MinSetpointDeadBand");
  }
  if (ezb_zcl_thermostat_cluster_desc_add_attr(thermostat, EZB_ZCL_ATTR_THERMOSTAT_THERMOSTAT_RUNNING_STATE_ID, &_running_state) != EZB_ERR_NONE) {
    log_e("Failed to add ThermostatRunningState");
  }
  // LocalTemperature is already reportable. System mode and the occupied setpoints are not.
  hvac_enable_reporting(thermostat, EZB_ZCL_ATTR_THERMOSTAT_THERMOSTAT_RUNNING_STATE_ID);
}

bool ZigbeeHVAC::reportAttr(uint16_t attr_id) {
  ezb_zcl_report_attr_cmd_t report_attr_cmd;
  memset(&report_attr_cmd, 0, sizeof(report_attr_cmd));
  ezb_address_set_none(&report_attr_cmd.cmd_ctrl.dst_addr);
  report_attr_cmd.cmd_ctrl.src_ep = _endpoint;
  report_attr_cmd.cmd_ctrl.cluster_id = EZB_ZCL_CLUSTER_ID_THERMOSTAT;
  report_attr_cmd.cmd_ctrl.manuf_code = EZB_ZCL_STD_MANUF_CODE;
  report_attr_cmd.cmd_ctrl.fc.direction = EZB_ZCL_CMD_DIRECTION_TO_CLI;
  report_attr_cmd.payload.attr_id = attr_id;
  return reportClusterAttribute(&report_attr_cmd);
}

bool ZigbeeHVAC::reportLocalTemperature() {
  return reportAttr(EZB_ZCL_ATTR_THERMOSTAT_LOCAL_TEMPERATURE_ID);
}

bool ZigbeeHVAC::report() {
  return reportAttr(EZB_ZCL_ATTR_THERMOSTAT_LOCAL_TEMPERATURE_ID) &&
         reportAttr(EZB_ZCL_ATTR_THERMOSTAT_THERMOSTAT_RUNNING_STATE_ID);
}

bool ZigbeeHVAC::writeAttr(const char *api, uint16_t attr_id, void *value) {
  if (configureEpClusterAttr(
        api, EZB_ZCL_CLUSTER_ID_THERMOSTAT, EZB_ZCL_CLUSTER_SERVER, attr_id, value, ezb_zcl_thermostat_cluster_desc_add_attr
      )) {
    return true;
  }
  log_e("%s: failed to write thermostat attribute 0x%04x", api, attr_id);
  return false;
}

bool ZigbeeHVAC::setControlSequence(ZigbeeHVACControlSequence sequence) {
  if (!hvac_sequence_valid(sequence)) {
    log_e("Invalid HVAC control sequence: 0x%02x", sequence);
    return false;
  }
  uint8_t previous = _control_sequence;
  _control_sequence = (uint8_t)sequence;
  if (!writeAttr("setControlSequence", EZB_ZCL_ATTR_THERMOSTAT_CONTROL_SEQUENCE_OF_OPERATION_ID, &_control_sequence)) {
    _control_sequence = previous;
    return false;
  }
  return true;
}

bool ZigbeeHVAC::setSystemMode(ZigbeeHVACSystemMode mode) {
  if (!hvac_system_mode_valid(mode)) {
    log_e("Invalid HVAC system mode: 0x%02x", mode);
    return false;
  }
  uint8_t previous = _system_mode;
  _system_mode = (uint8_t)mode;
  if (!writeAttr("setSystemMode", EZB_ZCL_ATTR_THERMOSTAT_SYSTEM_MODE_ID, &_system_mode)) {
    _system_mode = previous;
    return false;
  }
  return true;
}

bool ZigbeeHVAC::setOccupiedHeatingSetpoint(float celsius) {
  int16_t centi = 0;
  if (!hvac_celsius_to_centi(celsius, &centi)) {
    return false;
  }
  // Cooling stays at least MinSetpointDeadBand (0.1°C units) above heating.
  if (centi > (int16_t)(_occupied_cooling - (int16_t)_min_dead_band * 10)) {
    log_e(
      "Heating setpoint %.2f°C is within %.1f°C of cooling %.2f°C", celsius, _min_dead_band / 10.0f, _occupied_cooling / 100.0f
    );
    return false;
  }
  int16_t previous = _occupied_heating;
  _occupied_heating = centi;
  if (!writeAttr("setOccupiedHeatingSetpoint", EZB_ZCL_ATTR_THERMOSTAT_OCCUPIED_HEATING_SETPOINT_ID, &_occupied_heating)) {
    _occupied_heating = previous;
    return false;
  }
  return true;
}

bool ZigbeeHVAC::setOccupiedCoolingSetpoint(float celsius) {
  int16_t centi = 0;
  if (!hvac_celsius_to_centi(celsius, &centi)) {
    return false;
  }
  if (centi < (int16_t)(_occupied_heating + (int16_t)_min_dead_band * 10)) {
    log_e(
      "Cooling setpoint %.2f°C is within %.1f°C of heating %.2f°C", celsius, _min_dead_band / 10.0f, _occupied_heating / 100.0f
    );
    return false;
  }
  int16_t previous = _occupied_cooling;
  _occupied_cooling = centi;
  if (!writeAttr("setOccupiedCoolingSetpoint", EZB_ZCL_ATTR_THERMOSTAT_OCCUPIED_COOLING_SETPOINT_ID, &_occupied_cooling)) {
    _occupied_cooling = previous;
    return false;
  }
  return true;
}

bool ZigbeeHVAC::setLocalTemperature(float celsius) {
  int16_t centi = 0;
  if (!hvac_celsius_to_centi(celsius, &centi)) {
    return false;
  }
  int16_t previous = _local_temperature;
  _local_temperature = centi;
  if (!writeAttr("setLocalTemperature", EZB_ZCL_ATTR_THERMOSTAT_LOCAL_TEMPERATURE_ID, &_local_temperature)) {
    _local_temperature = previous;
    return false;
  }
  return true;
}

bool ZigbeeHVAC::setRunningState(uint16_t running_state) {
  uint16_t previous = _running_state;
  _running_state = running_state;
  if (!writeAttr("setRunningState", EZB_ZCL_ATTR_THERMOSTAT_THERMOSTAT_RUNNING_STATE_ID, &_running_state)) {
    _running_state = previous;
    return false;
  }
  return true;
}

void ZigbeeHVAC::hvacChanged() {
  if (_on_hvac_change) {
    _on_hvac_change((ZigbeeHVACSystemMode)_system_mode, _occupied_heating / 100.0f, _occupied_cooling / 100.0f);
  } else {
    log_w("No callback function set for HVAC change");
  }
}

void ZigbeeHVAC::zbAttributeSet(const ezb_zcl_set_attr_value_message_t *message) {
  if (message->info.cluster_id != EZB_ZCL_CLUSTER_ID_THERMOSTAT) {
    log_w("Received message ignored. Cluster ID: %u not supported for HVAC", message->info.cluster_id);
    return;
  }

  switch (message->in.attribute.id) {
    case EZB_ZCL_ATTR_THERMOSTAT_SYSTEM_MODE_ID: {
      uint8_t mode = 0;
      if (message->in.attribute.data.type != EZB_ZCL_ATTR_TYPE_ENUM8 || !hvac_read_u8(message, &mode) || !hvac_system_mode_valid(mode)) {
        log_w("Invalid HVAC system mode attribute");
        return;
      }
      _system_mode = mode;
      hvacChanged();
      break;
    }
    case EZB_ZCL_ATTR_THERMOSTAT_OCCUPIED_HEATING_SETPOINT_ID: {
      int16_t centi = 0;
      if (message->in.attribute.data.type != EZB_ZCL_ATTR_TYPE_INT16 || !hvac_read_s16(message, &centi)) {
        log_w("Invalid occupied heating setpoint attribute");
        return;
      }
      _occupied_heating = centi;
      hvacChanged();
      break;
    }
    case EZB_ZCL_ATTR_THERMOSTAT_OCCUPIED_COOLING_SETPOINT_ID: {
      int16_t centi = 0;
      if (message->in.attribute.data.type != EZB_ZCL_ATTR_TYPE_INT16 || !hvac_read_s16(message, &centi)) {
        log_w("Invalid occupied cooling setpoint attribute");
        return;
      }
      _occupied_cooling = centi;
      hvacChanged();
      break;
    }
    default: log_w("Received message ignored. Attribute ID: 0x%04x not supported for HVAC", message->in.attribute.id); break;
  }
}

void ZigbeeHVAC::zbThermostatSetpoint(uint8_t mode, int16_t heat_centi, int16_t cool_centi) {
  // The stack invokes this before it writes the attributes. Values are already scaled to 0.01°C and clamped.
  switch (mode) {
    case EZB_ZCL_THERMOSTAT_SETPOINT_MODE_HEAT: _occupied_heating = heat_centi; break;
    case EZB_ZCL_THERMOSTAT_SETPOINT_MODE_COOL: _occupied_cooling = cool_centi; break;
    case EZB_ZCL_THERMOSTAT_SETPOINT_MODE_BOTH:
      _occupied_heating = heat_centi;
      _occupied_cooling = cool_centi;
      break;
    default: log_w("Unsupported thermostat setpoint mode: %u", mode); return;
  }
  hvacChanged();
}

#endif  // CONFIG_ZB_ENABLED
