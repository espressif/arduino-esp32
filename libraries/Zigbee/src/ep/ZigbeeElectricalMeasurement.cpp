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

#include "ZigbeeElectricalMeasurement.h"
#if CONFIG_ZB_ENABLED
#include "ezbee/zha.h"


// NOTE(zb-v2): the v1 header had a wrong DC power attribute name and required a workaround macro
// (ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DCPOWER_ID). v2.x exposes the correctly named
// EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_POWER_ID directly, so the workaround is no longer needed.

ZigbeeElectricalMeasurement::ZigbeeElectricalMeasurement(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = EZB_ZHA_SIMPLE_SENSOR_DEVICE_ID;
  _electrical_measurement_cfg = {
    .measurement_type = 0,
  };

  _ep_config = {.ep_id = _endpoint, .app_profile_id = EZB_AF_HA_PROFILE_ID, .app_device_id = EZB_ZHA_METER_INTERFACE_DEVICE_ID, .app_device_version = 0};
  _ep_desc = ezb_af_create_endpoint_desc(&_ep_config);
  if (_ep_desc == nullptr) {
    log_e("Failed to create electrical measurement endpoint descriptor");
    return;
  }
  ezb_af_endpoint_add_cluster_desc(_ep_desc, ezb_zcl_basic_create_cluster_desc(nullptr, EZB_ZCL_CLUSTER_SERVER));
  ezb_af_endpoint_add_cluster_desc(_ep_desc, ezb_zcl_identify_create_cluster_desc(nullptr, EZB_ZCL_CLUSTER_SERVER));
  ezb_af_endpoint_add_cluster_desc(_ep_desc, ezb_zcl_electrical_measurement_create_cluster_desc(&_electrical_measurement_cfg, EZB_ZCL_CLUSTER_SERVER));
}

/* DC MEASUREMENT */

bool ZigbeeElectricalMeasurement::addDCMeasurement(ZIGBEE_DC_MEASUREMENT_TYPE measurement_type) {
  _electrical_measurement_cfg.measurement_type |= EZB_ZCL_ELECTRICAL_MEASUREMENT_MEASUREMENT_TYPE_DC_MEASUREMENT;
  if (!configureEpClusterAttr(
    "addDCMeasurement", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_MEASUREMENT_TYPE_ID,
    &_electrical_measurement_cfg.measurement_type, ezb_zcl_electrical_measurement_cluster_desc_add_attr
  )) {
    return false;
  }

  _dc_measurement = 0;
  _dc_min = -32767;
  _dc_max = 32767;
  _dc_multiplier = 1;
  _dc_divisor = 1;

  uint16_t attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_VOLTAGE_ID;
  uint16_t attr_min = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_VOLTAGE_MIN_VALUE_ID;
  uint16_t attr_max = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_VOLTAGE_MAX_VALUE_ID;
  uint16_t attr_multiplier = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_VOLTAGE_MULTIPLIER_ID;
  uint16_t attr_divisor = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_VOLTAGE_DIVISOR_ID;

  if (measurement_type == ZIGBEE_DC_MEASUREMENT_TYPE_CURRENT) {
    attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_CURRENT_ID;
    attr_min = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_CURRENT_MIN_VALUE_ID;
    attr_max = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_CURRENT_MAX_VALUE_ID;
    attr_multiplier = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_CURRENT_MULTIPLIER_ID;
    attr_divisor = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_CURRENT_DIVISOR_ID;
  } else if (measurement_type == ZIGBEE_DC_MEASUREMENT_TYPE_POWER) {
    attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_POWER_ID;
    attr_min = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_POWER_MIN_VALUE_ID;
    attr_max = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_POWER_MAX_VALUE_ID;
    attr_multiplier = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_POWER_MULTIPLIER_ID;
    attr_divisor = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_POWER_DIVISOR_ID;
  }

  if (!configureEpClusterAttr("addDCMeasurement", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, attr_id, (void *)&_dc_measurement, ezb_zcl_electrical_measurement_cluster_desc_add_attr)) {
    return false;
  }
  if (!configureEpClusterAttr("addDCMeasurement", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, attr_min, (void *)&_dc_min, ezb_zcl_electrical_measurement_cluster_desc_add_attr)) {
    return false;
  }
  if (!configureEpClusterAttr("addDCMeasurement", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, attr_max, (void *)&_dc_max, ezb_zcl_electrical_measurement_cluster_desc_add_attr)) {
    return false;
  }
  if (!configureEpClusterAttr("addDCMeasurement", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, attr_multiplier, (void *)&_dc_multiplier, ezb_zcl_electrical_measurement_cluster_desc_add_attr)) {
    return false;
  }
  return configureEpClusterAttr("addDCMeasurement", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, attr_divisor, (void *)&_dc_divisor, ezb_zcl_electrical_measurement_cluster_desc_add_attr);
}

bool ZigbeeElectricalMeasurement::setDCMinMaxValue(ZIGBEE_DC_MEASUREMENT_TYPE measurement_type, int16_t min, int16_t max) {
  _dc_min = min;
  _dc_max = max;

  uint16_t attr_min = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_VOLTAGE_MIN_VALUE_ID;
  uint16_t attr_max = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_VOLTAGE_MAX_VALUE_ID;
  if (measurement_type == ZIGBEE_DC_MEASUREMENT_TYPE_CURRENT) {
    attr_min = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_CURRENT_MIN_VALUE_ID;
    attr_max = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_CURRENT_MAX_VALUE_ID;
  } else if (measurement_type == ZIGBEE_DC_MEASUREMENT_TYPE_POWER) {
    attr_min = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_POWER_MIN_VALUE_ID;
    attr_max = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_POWER_MAX_VALUE_ID;
  }

  if (!configureEpClusterAttr("setDCMinMaxValue", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, attr_min, (void *)&_dc_min, ezb_zcl_electrical_measurement_cluster_desc_add_attr)) {
    return false;
  }
  return configureEpClusterAttr("setDCMinMaxValue", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, attr_max, (void *)&_dc_max, ezb_zcl_electrical_measurement_cluster_desc_add_attr);
}

bool ZigbeeElectricalMeasurement::setDCMultiplierDivisor(ZIGBEE_DC_MEASUREMENT_TYPE measurement_type, uint16_t multiplier, uint16_t divisor) {
  _dc_multiplier = multiplier;
  _dc_divisor = divisor;

  uint16_t attr_multiplier = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_VOLTAGE_MULTIPLIER_ID;
  uint16_t attr_divisor = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_VOLTAGE_DIVISOR_ID;

  if (measurement_type == ZIGBEE_DC_MEASUREMENT_TYPE_CURRENT) {
    attr_multiplier = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_CURRENT_MULTIPLIER_ID;
    attr_divisor = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_CURRENT_DIVISOR_ID;
  } else if (measurement_type == ZIGBEE_DC_MEASUREMENT_TYPE_POWER) {
    attr_multiplier = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_POWER_MULTIPLIER_ID;
    attr_divisor = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_POWER_DIVISOR_ID;
  }

  if (!configureEpClusterAttr("setDCMultiplierDivisor", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, attr_multiplier, (void *)&_dc_multiplier, ezb_zcl_electrical_measurement_cluster_desc_add_attr)) {
    return false;
  }
  return configureEpClusterAttr("setDCMultiplierDivisor", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, attr_divisor, (void *)&_dc_divisor, ezb_zcl_electrical_measurement_cluster_desc_add_attr);
}

bool ZigbeeElectricalMeasurement::setDCReporting(ZIGBEE_DC_MEASUREMENT_TYPE measurement_type, uint16_t min_interval, uint16_t max_interval, int16_t delta) {
  uint16_t attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_VOLTAGE_ID;
  if (measurement_type == ZIGBEE_DC_MEASUREMENT_TYPE_CURRENT) {
    attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_CURRENT_ID;
  } else if (measurement_type == ZIGBEE_DC_MEASUREMENT_TYPE_POWER) {
    attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_POWER_ID;
  }

  // NOTE(zb-v2): Reporting is now handle-based (was a caller-populated esp_zb_zcl_reporting_info_t).
  // The reporting record is created by the stack when the endpoint is registered, so this must be
  // called after Zigbee.begin(). We look up the handle, tune the intervals/reportable change, then start.
  ezb_zcl_reporting_info_t reporting_info = ezb_zcl_reporting_info_find(
    _endpoint, EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, attr_id, EZB_ZCL_STD_MANUF_CODE
  );
  if (reporting_info == EZB_ZCL_INVALID_REPORTING_INFO) {
    log_e("Failed to find reporting info for DC measurement");
    return false;
  }
  ezb_zcl_attr_variable_t delta_var = {};
  delta_var.s16 = delta;
  ezb_zcl_reporting_info_update(reporting_info, min_interval, max_interval, &delta_var);
  ezb_zcl_reporting_info_update_default_interval(reporting_info, min_interval, max_interval);
  return setClusterReporting(reporting_info);
}

bool ZigbeeElectricalMeasurement::setDCMeasurement(ZIGBEE_DC_MEASUREMENT_TYPE measurement_type, int16_t measurement) {
  uint16_t attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_VOLTAGE_ID;
  if (measurement_type == ZIGBEE_DC_MEASUREMENT_TYPE_CURRENT) {
    attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_CURRENT_ID;
  } else if (measurement_type == ZIGBEE_DC_MEASUREMENT_TYPE_POWER) {
    attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_POWER_ID;
  }

  _dc_measurement = measurement;
  log_v("Updating DC measurement value...");
  log_d("Setting DC measurement to %d", _dc_measurement);
  return configureEpClusterAttr("setDCMeasurement", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, attr_id, (void *)&_dc_measurement, ezb_zcl_electrical_measurement_cluster_desc_add_attr);
}

bool ZigbeeElectricalMeasurement::reportDC(ZIGBEE_DC_MEASUREMENT_TYPE measurement_type) {
  uint16_t attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_VOLTAGE_ID;
  if (measurement_type == ZIGBEE_DC_MEASUREMENT_TYPE_CURRENT) {
    attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_CURRENT_ID;
  } else if (measurement_type == ZIGBEE_DC_MEASUREMENT_TYPE_POWER) {
    attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_POWER_ID;
  }
  /* Send report attributes command */
  ezb_zcl_report_attr_cmd_t report_attr_cmd;
  memset(&report_attr_cmd, 0, sizeof(report_attr_cmd));
  // No explicit destination: report to bound devices (replaces v1 ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT).
  ezb_address_set_none(&report_attr_cmd.cmd_ctrl.dst_addr);
  report_attr_cmd.cmd_ctrl.src_ep = _endpoint;
  report_attr_cmd.cmd_ctrl.cluster_id = EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT;
  report_attr_cmd.cmd_ctrl.manuf_code = EZB_ZCL_STD_MANUF_CODE;
  report_attr_cmd.cmd_ctrl.fc.direction = EZB_ZCL_CMD_DIRECTION_TO_CLI;
  report_attr_cmd.payload.attr_id = attr_id;

  if (!reportClusterAttribute(&report_attr_cmd)) {
    log_e("Failed to send DC report");
    return false;
  }
  log_v("DC report sent");
  return true;
}

/* AC MEASUREMENT */

bool ZigbeeElectricalMeasurement::addACMeasurement(ZIGBEE_AC_MEASUREMENT_TYPE measurement_type, ZIGBEE_AC_PHASE_TYPE phase_type) {
  switch (phase_type) {
    case ZIGBEE_AC_PHASE_TYPE_NON_SPECIFIC:
    // Non phase specific dont need any bit set
    break;
    case ZIGBEE_AC_PHASE_TYPE_A: _electrical_measurement_cfg.measurement_type |= EZB_ZCL_ELECTRICAL_MEASUREMENT_MEASUREMENT_TYPE_PHASE_A_MEASUREMENT; break;
    case ZIGBEE_AC_PHASE_TYPE_B: _electrical_measurement_cfg.measurement_type |= EZB_ZCL_ELECTRICAL_MEASUREMENT_MEASUREMENT_TYPE_PHASE_B_MEASUREMENT; break;
    case ZIGBEE_AC_PHASE_TYPE_C: _electrical_measurement_cfg.measurement_type |= EZB_ZCL_ELECTRICAL_MEASUREMENT_MEASUREMENT_TYPE_PHASE_C_MEASUREMENT; break;
    default:                     log_e("Invalid phase type"); break;
  }
  // Set Active measurement bit for active power and power factor, otherwise no bit needed for voltage, current
  if (measurement_type == ZIGBEE_AC_MEASUREMENT_TYPE_POWER || measurement_type == ZIGBEE_AC_MEASUREMENT_TYPE_POWER_FACTOR) {
    _electrical_measurement_cfg.measurement_type |= EZB_ZCL_ELECTRICAL_MEASUREMENT_MEASUREMENT_TYPE_ACTIVE_MEASUREMENT_AC;  // Active power is used
  }
  if (!configureEpClusterAttr("addACMeasurement", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_MEASUREMENT_TYPE_ID, &_electrical_measurement_cfg.measurement_type, ezb_zcl_electrical_measurement_cluster_desc_add_attr)) {
    return false;
  }

  _ac_u16_measurement = 0;
  _ac_u16_min = 0;
  _ac_u16_max = 0xffff;
  _ac_s16_measurement = 0;
  _ac_s16_min = -32767;
  _ac_s16_max = 32767;
  _ac_power_factor = 100;
  _ac_multiplier = 1;
  _ac_divisor = 1;

  // AC Frequency
  if (measurement_type == ZIGBEE_AC_MEASUREMENT_TYPE_FREQUENCY) {  // No phase specific
    if (!configureEpClusterAttr("addACMeasurement", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_FREQUENCY_ID, (void *)&_ac_u16_measurement, ezb_zcl_electrical_measurement_cluster_desc_add_attr)) {
      return false;
    }
    if (!configureEpClusterAttr("addACMeasurement", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_FREQUENCY_MIN_VALUE_ID, (void *)&_ac_u16_min, ezb_zcl_electrical_measurement_cluster_desc_add_attr)) {
      return false;
    }
    if (!configureEpClusterAttr("addACMeasurement", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_FREQUENCY_MAX_VALUE_ID, (void *)&_ac_u16_max, ezb_zcl_electrical_measurement_cluster_desc_add_attr)) {
      return false;
    }
    if (!configureEpClusterAttr("addACMeasurement", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_FREQUENCY_MULTIPLIER_ID, (void *)&_ac_multiplier, ezb_zcl_electrical_measurement_cluster_desc_add_attr)) {
      return false;
    }
    return configureEpClusterAttr("addACMeasurement", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_FREQUENCY_DIVISOR_ID, (void *)&_ac_divisor, ezb_zcl_electrical_measurement_cluster_desc_add_attr);
  }
  // Add the AC Voltage attributes
  else if (measurement_type == ZIGBEE_AC_MEASUREMENT_TYPE_VOLTAGE) {
    uint16_t attr_voltage = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_ID;
    uint16_t attr_voltage_min = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_MIN_VALUE_ID;
    uint16_t attr_voltage_max = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_MAX_VALUE_ID;
    switch (phase_type) {
    case ZIGBEE_AC_PHASE_TYPE_A:
      // already set
      break;
    case ZIGBEE_AC_PHASE_TYPE_B:
      attr_voltage = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_PH_B_ID;
      attr_voltage_min = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_MIN_PH_B_ID;
      attr_voltage_max = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_MAX_PH_B_ID;
      break;
    case ZIGBEE_AC_PHASE_TYPE_C:
      attr_voltage = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_PH_C_ID;
      attr_voltage_min = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_MIN_PH_C_ID;
      attr_voltage_max = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_MAX_PH_C_ID;
      break;
    default: log_e("Invalid phase type"); return false;
    }
    if (!configureEpClusterAttr("addACMeasurement", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, attr_voltage, (void *)&_ac_u16_measurement, ezb_zcl_electrical_measurement_cluster_desc_add_attr)) {
      return false;
    }
    if (!configureEpClusterAttr("addACMeasurement", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, attr_voltage_min, (void *)&_ac_u16_min, ezb_zcl_electrical_measurement_cluster_desc_add_attr)) {
      return false;
    }
    if (!configureEpClusterAttr("addACMeasurement", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, attr_voltage_max, (void *)&_ac_u16_max, ezb_zcl_electrical_measurement_cluster_desc_add_attr)) {
      return false;
    }
    if (!ac_volt_mult_div_set) {
    if (!configureEpClusterAttr(
          "addACMeasurement", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_VOLTAGE_MULTIPLIER_ID,
          (void *)&_ac_multiplier, ezb_zcl_electrical_measurement_cluster_desc_add_attr
        )) {
      return false;
    }
    if (!configureEpClusterAttr("addACMeasurement", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_VOLTAGE_DIVISOR_ID, (void *)&_ac_divisor, ezb_zcl_electrical_measurement_cluster_desc_add_attr)) {
      return false;
    }
    ac_volt_mult_div_set = true;  // Set flag to true, so we dont add the attributes again
    }
  }
  // Add the AC Current attributes
  else if (measurement_type == ZIGBEE_AC_MEASUREMENT_TYPE_CURRENT) {
    uint16_t attr_current = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_ID;
    uint16_t attr_current_min = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_MIN_VALUE_ID;
    uint16_t attr_current_max = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_MAX_VALUE_ID;
    switch (phase_type) {
    case ZIGBEE_AC_PHASE_TYPE_A:
      // already set
      break;
    case ZIGBEE_AC_PHASE_TYPE_B:
      attr_current = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_PH_B_ID;
      attr_current_min = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_MIN_PH_B_ID;
      attr_current_max = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_MAX_PH_B_ID;
      break;
    case ZIGBEE_AC_PHASE_TYPE_C:
      attr_current = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_PH_C_ID;
      attr_current_min = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_MIN_PH_C_ID;
      attr_current_max = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_MAX_PH_C_ID;
      break;
    default: log_e("Invalid phase type"); return false;
    }
    if (!configureEpClusterAttr("addACMeasurement", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, attr_current, (void *)&_ac_u16_measurement, ezb_zcl_electrical_measurement_cluster_desc_add_attr)) {
      return false;
    }
    if (!configureEpClusterAttr("addACMeasurement", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, attr_current_min, (void *)&_ac_u16_min, ezb_zcl_electrical_measurement_cluster_desc_add_attr)) {
      return false;
    }
    if (!configureEpClusterAttr("addACMeasurement", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, attr_current_max, (void *)&_ac_u16_max, ezb_zcl_electrical_measurement_cluster_desc_add_attr)) {
      return false;
    }
    if (!ac_current_mult_div_set) {
    if (!configureEpClusterAttr(
          "addACMeasurement", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_CURRENT_MULTIPLIER_ID,
          (void *)&_ac_multiplier, ezb_zcl_electrical_measurement_cluster_desc_add_attr
        )) {
      return false;
    }
    if (!configureEpClusterAttr("addACMeasurement", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_CURRENT_DIVISOR_ID, (void *)&_ac_divisor, ezb_zcl_electrical_measurement_cluster_desc_add_attr)) {
      return false;
    }
    ac_current_mult_div_set = true;  // Set flag to true, so we dont add the attributes again
    }
  }
  // Add the AC Power attributes
  else if (measurement_type == ZIGBEE_AC_MEASUREMENT_TYPE_POWER) {
    uint16_t attr_power = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_ID;
    uint16_t attr_power_min = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_MIN_VALUE_ID;
    uint16_t attr_power_max = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_MAX_VALUE_ID;

    switch (phase_type) {
    case ZIGBEE_AC_PHASE_TYPE_A:
      // already set
      break;
    case ZIGBEE_AC_PHASE_TYPE_B:
      attr_power = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_PH_B_ID;
      attr_power_min = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_MIN_PH_B_ID;
      attr_power_max = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_MAX_PH_B_ID;
      break;
    case ZIGBEE_AC_PHASE_TYPE_C:
      attr_power = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_PH_C_ID;
      attr_power_min = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_MIN_PH_C_ID;
      attr_power_max = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_MAX_PH_C_ID;
      break;
    default: log_e("Invalid phase type"); return false;
    }
    if (!configureEpClusterAttr("addACMeasurement", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, attr_power, (void *)&_ac_u16_measurement, ezb_zcl_electrical_measurement_cluster_desc_add_attr)) {
      return false;
    }
    if (!configureEpClusterAttr("addACMeasurement", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, attr_power_min, (void *)&_ac_u16_min, ezb_zcl_electrical_measurement_cluster_desc_add_attr)) {
      return false;
    }
    if (!configureEpClusterAttr("addACMeasurement", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, attr_power_max, (void *)&_ac_u16_max, ezb_zcl_electrical_measurement_cluster_desc_add_attr)) {
      return false;
    }
    if (!ac_power_mult_div_set) {
    if (!configureEpClusterAttr("addACMeasurement", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_POWER_MULTIPLIER_ID, (void *)&_ac_multiplier, ezb_zcl_electrical_measurement_cluster_desc_add_attr)) {
      return false;
    }
    if (!configureEpClusterAttr("addACMeasurement", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_POWER_DIVISOR_ID, (void *)&_ac_divisor, ezb_zcl_electrical_measurement_cluster_desc_add_attr)) {
      return false;
    }
    ac_power_mult_div_set = true;  // Set flag to true, so we dont add the attributes again
    }
  } else {  //(measurement_type == ZIGBEE_AC_MEASUREMENT_TYPE_POWER_FACTOR)
    uint16_t attr_power_factor = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_POWER_FACTOR_ID;

    switch (phase_type) {
    case ZIGBEE_AC_PHASE_TYPE_A:
      // already set
      break;
    case ZIGBEE_AC_PHASE_TYPE_B: attr_power_factor = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_POWER_FACTOR_PH_B_ID; break;
    case ZIGBEE_AC_PHASE_TYPE_C: attr_power_factor = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_POWER_FACTOR_PH_C_ID; break;
    default:                     log_e("Invalid phase type"); return false;
    }
    if (!configureEpClusterAttr("addACMeasurement", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, attr_power_factor, (void *)&_ac_power_factor, ezb_zcl_electrical_measurement_cluster_desc_add_attr)) {
      return false;
    }
  }
  return true;
}

bool ZigbeeElectricalMeasurement::setACMinMaxValue(
  ZIGBEE_AC_MEASUREMENT_TYPE measurement_type, ZIGBEE_AC_PHASE_TYPE phase_type, int32_t min_value, int32_t max_value
) {
  uint16_t attr_min_id = 0;
  uint16_t attr_max_id = 0;

  // Check min/max values are valid for the measurement type
  switch (measurement_type) {
    case ZIGBEE_AC_MEASUREMENT_TYPE_VOLTAGE:
    case ZIGBEE_AC_MEASUREMENT_TYPE_CURRENT:
    case ZIGBEE_AC_MEASUREMENT_TYPE_FREQUENCY:
    if (min_value < 0 || min_value > UINT16_MAX || max_value < 0 || max_value > UINT16_MAX) {
      log_e("AC measurement min/max values must be between 0 and %u (got min=%u, max=%u)", UINT16_MAX, min_value, max_value);
      return false;
    }
    break;

    case ZIGBEE_AC_MEASUREMENT_TYPE_POWER:
    if (min_value < INT16_MIN || min_value > INT16_MAX || max_value < INT16_MIN || max_value > INT16_MAX) {
      log_e("AC power min/max values must be between %d and %d (got min=%d, max=%d)", INT16_MIN, INT16_MAX, min_value, max_value);
      return false;
    }
    break;

    default: log_e("Invalid measurement type"); return false;
  }

  switch (measurement_type) {
    case ZIGBEE_AC_MEASUREMENT_TYPE_VOLTAGE:
    switch (phase_type) {
      case ZIGBEE_AC_PHASE_TYPE_A:
        attr_min_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_MIN_VALUE_ID;
        attr_max_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_MAX_VALUE_ID;
        break;
      case ZIGBEE_AC_PHASE_TYPE_B:
        attr_min_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_MIN_PH_B_ID;
        attr_max_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_MAX_PH_B_ID;
        break;
      case ZIGBEE_AC_PHASE_TYPE_C:
        attr_min_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_MIN_PH_C_ID;
        attr_max_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_MAX_PH_C_ID;
        break;
      default: log_e("Invalid phase type"); return false;
    }
    break;

    case ZIGBEE_AC_MEASUREMENT_TYPE_CURRENT:
    switch (phase_type) {
      case ZIGBEE_AC_PHASE_TYPE_A:
        attr_min_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_MIN_VALUE_ID;
        attr_max_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_MAX_VALUE_ID;
        break;
      case ZIGBEE_AC_PHASE_TYPE_B:
        attr_min_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_MIN_PH_B_ID;
        attr_max_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_MAX_PH_B_ID;
        break;
      case ZIGBEE_AC_PHASE_TYPE_C:
        attr_min_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_MIN_PH_C_ID;
        attr_max_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_MAX_PH_C_ID;
        break;
      default: log_e("Invalid phase type"); return false;
    }
    break;

    case ZIGBEE_AC_MEASUREMENT_TYPE_POWER:
    switch (phase_type) {
      case ZIGBEE_AC_PHASE_TYPE_A:
        attr_min_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_MIN_VALUE_ID;
        attr_max_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_MAX_VALUE_ID;
        break;
      case ZIGBEE_AC_PHASE_TYPE_B:
        attr_min_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_MIN_PH_B_ID;
        attr_max_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_MAX_PH_B_ID;
        break;
      case ZIGBEE_AC_PHASE_TYPE_C:
        attr_min_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_MIN_PH_C_ID;
        attr_max_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_MAX_PH_C_ID;
        break;
      default: log_e("Invalid phase type"); return false;
    }
    break;

    default: log_e("Invalid measurement type"); return false;
  }

  if (measurement_type == ZIGBEE_AC_MEASUREMENT_TYPE_POWER) {
    _ac_s16_min = (int16_t)min_value;
    _ac_s16_max = (int16_t)max_value;
    if (!configureEpClusterAttr("setACMinMaxValue", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, attr_min_id, (void *)&_ac_s16_min, ezb_zcl_electrical_measurement_cluster_desc_add_attr)) {
      return false;
    }
    return configureEpClusterAttr("setACMinMaxValue", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, attr_max_id, (void *)&_ac_s16_max, ezb_zcl_electrical_measurement_cluster_desc_add_attr);
  }

  _ac_u16_min = (uint16_t)min_value;
  _ac_u16_max = (uint16_t)max_value;
  if (!configureEpClusterAttr("setACMinMaxValue", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, attr_min_id, (void *)&_ac_u16_min, ezb_zcl_electrical_measurement_cluster_desc_add_attr)) {
    return false;
  }
  return configureEpClusterAttr("setACMinMaxValue", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, attr_max_id, (void *)&_ac_u16_max, ezb_zcl_electrical_measurement_cluster_desc_add_attr);
}

bool ZigbeeElectricalMeasurement::setACMultiplierDivisor(ZIGBEE_AC_MEASUREMENT_TYPE measurement_type, uint16_t multiplier, uint16_t divisor) {
  _ac_multiplier = multiplier;
  _ac_divisor = divisor;

  uint16_t attr_multiplier = 0;
  uint16_t attr_divisor = 0;

  switch (measurement_type) {
    case ZIGBEE_AC_MEASUREMENT_TYPE_VOLTAGE:
    attr_multiplier = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_VOLTAGE_MULTIPLIER_ID;
    attr_divisor = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_VOLTAGE_DIVISOR_ID;
    break;
    case ZIGBEE_AC_MEASUREMENT_TYPE_CURRENT:
    attr_multiplier = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_CURRENT_MULTIPLIER_ID;
    attr_divisor = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_CURRENT_DIVISOR_ID;
    break;
    case ZIGBEE_AC_MEASUREMENT_TYPE_POWER:
    attr_multiplier = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_POWER_MULTIPLIER_ID;
    attr_divisor = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_POWER_DIVISOR_ID;
    break;
    case ZIGBEE_AC_MEASUREMENT_TYPE_FREQUENCY:
    attr_multiplier = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_FREQUENCY_MULTIPLIER_ID;
    attr_divisor = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_FREQUENCY_DIVISOR_ID;
    break;
    default: log_e("Invalid measurement type"); return false;
  }

  if (!configureEpClusterAttr("setACMultiplierDivisor", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, attr_multiplier, (void *)&_ac_multiplier, ezb_zcl_electrical_measurement_cluster_desc_add_attr)) {
    return false;
  }
  return configureEpClusterAttr("setACMultiplierDivisor", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, attr_divisor, (void *)&_ac_divisor, ezb_zcl_electrical_measurement_cluster_desc_add_attr);
}

bool ZigbeeElectricalMeasurement::setACPowerFactor(ZIGBEE_AC_PHASE_TYPE phase_type, int8_t power_factor) {
  uint16_t attr_id = 0;

  switch (phase_type) {
    case ZIGBEE_AC_PHASE_TYPE_A:            attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_POWER_FACTOR_ID; break;
    case ZIGBEE_AC_PHASE_TYPE_B:            attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_POWER_FACTOR_PH_B_ID; break;
    case ZIGBEE_AC_PHASE_TYPE_C:            attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_POWER_FACTOR_PH_C_ID; break;
    case ZIGBEE_AC_PHASE_TYPE_NON_SPECIFIC:
    default:                                log_e("Invalid phase type"); return false;
  }

  _ac_power_factor = power_factor;
  return configureEpClusterAttr("setACPowerFactor", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, attr_id, (void *)&_ac_power_factor, ezb_zcl_electrical_measurement_cluster_desc_add_attr);
}

bool ZigbeeElectricalMeasurement::setACMeasurement(ZIGBEE_AC_MEASUREMENT_TYPE measurement_type, ZIGBEE_AC_PHASE_TYPE phase_type, int32_t value) {
  uint16_t attr_id = 0;

  // Check value is valid for the measurement type
  switch (measurement_type) {
    case ZIGBEE_AC_MEASUREMENT_TYPE_VOLTAGE:
    case ZIGBEE_AC_MEASUREMENT_TYPE_CURRENT:
    case ZIGBEE_AC_MEASUREMENT_TYPE_FREQUENCY:
      if (value < 0 || value > UINT16_MAX) {
        log_e("AC measurement value must be between 0 and %u (got %" PRId32 ")", UINT16_MAX, value);
        return false;
      }
      break;

    case ZIGBEE_AC_MEASUREMENT_TYPE_POWER:
      if (value < INT16_MIN || value > INT16_MAX) {
        log_e("AC power value must be between %d and %d (got %" PRId32 ")", INT16_MIN, INT16_MAX, value);
        return false;
      }
      break;

    case ZIGBEE_AC_MEASUREMENT_TYPE_POWER_FACTOR:
      if (value < -100 || value > 100) {
        log_e("AC power factor value must be between -100 and 100 (got %" PRId32 ")", value);
        return false;
      }
      break;

    default: log_e("Invalid measurement type"); return false;
  }

  switch (measurement_type) {
    case ZIGBEE_AC_MEASUREMENT_TYPE_VOLTAGE:
      switch (phase_type) {
        case ZIGBEE_AC_PHASE_TYPE_A:            attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_B:            attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_PH_B_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_C:            attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_PH_C_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_NON_SPECIFIC:
        default:                                log_e("Invalid phase type"); return false;
      }
      _ac_u16_measurement = (uint16_t)value;
      log_v("Updating AC voltage measurement value...");
      log_d("Setting AC voltage to %u", _ac_u16_measurement);
      return configureEpClusterAttr("setACMeasurement", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, attr_id, (void *)&_ac_u16_measurement, ezb_zcl_electrical_measurement_cluster_desc_add_attr);

    case ZIGBEE_AC_MEASUREMENT_TYPE_CURRENT:
      switch (phase_type) {
        case ZIGBEE_AC_PHASE_TYPE_A:            attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_B:            attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_PH_B_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_C:            attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_PH_C_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_NON_SPECIFIC:
        default:                                log_e("Invalid phase type"); return false;
      }
      _ac_u16_measurement = (uint16_t)value;
      log_v("Updating AC current measurement value...");
      log_d("Setting AC current to %u", _ac_u16_measurement);
      return configureEpClusterAttr("setACMeasurement", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, attr_id, (void *)&_ac_u16_measurement, ezb_zcl_electrical_measurement_cluster_desc_add_attr);

    case ZIGBEE_AC_MEASUREMENT_TYPE_POWER:
      switch (phase_type) {
        case ZIGBEE_AC_PHASE_TYPE_A:            attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_B:            attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_PH_B_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_C:            attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_PH_C_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_NON_SPECIFIC:
        default:                                log_e("Invalid phase type"); return false;
      }
      _ac_s16_measurement = (int16_t)value;
      log_v("Updating AC power measurement value...");
      log_d("Setting AC power to %d", _ac_s16_measurement);
      return configureEpClusterAttr("setACMeasurement", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, attr_id, (void *)&_ac_s16_measurement, ezb_zcl_electrical_measurement_cluster_desc_add_attr);

    case ZIGBEE_AC_MEASUREMENT_TYPE_FREQUENCY:
      attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_FREQUENCY_ID;
      _ac_u16_measurement = (uint16_t)value;
      log_v("Updating AC frequency measurement value...");
      log_d("Setting AC frequency to %u", _ac_u16_measurement);
      return configureEpClusterAttr("setACMeasurement", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, attr_id, (void *)&_ac_u16_measurement, ezb_zcl_electrical_measurement_cluster_desc_add_attr);

    case ZIGBEE_AC_MEASUREMENT_TYPE_POWER_FACTOR:
      switch (phase_type) {
        case ZIGBEE_AC_PHASE_TYPE_A:            attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_POWER_FACTOR_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_B:            attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_POWER_FACTOR_PH_B_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_C:            attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_POWER_FACTOR_PH_C_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_NON_SPECIFIC:
        default:                                log_e("Invalid phase type"); return false;
      }
      _ac_power_factor = (int8_t)value;
      log_v("Updating AC power factor measurement value...");
      log_d("Setting AC power factor to %d", _ac_power_factor);
      return configureEpClusterAttr("setACMeasurement", EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, attr_id, (void *)&_ac_power_factor, ezb_zcl_electrical_measurement_cluster_desc_add_attr);

    default: log_e("Invalid measurement type"); return false;
  }
}

bool ZigbeeElectricalMeasurement::setACReporting(
  ZIGBEE_AC_MEASUREMENT_TYPE measurement_type, ZIGBEE_AC_PHASE_TYPE phase_type, uint16_t min_interval, uint16_t max_interval, int32_t delta
) {
  uint16_t attr_id = 0;

  // Convert value to appropriate type based on measurement type
  uint16_t uint16_delta = (uint16_t)delta;
  int16_t int16_delta = (int16_t)delta;

  switch (measurement_type) {
    case ZIGBEE_AC_MEASUREMENT_TYPE_VOLTAGE:
      switch (phase_type) {
        case ZIGBEE_AC_PHASE_TYPE_A:            attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_B:            attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_PH_B_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_C:            attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_PH_C_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_NON_SPECIFIC:
        default:                                log_e("Invalid phase type"); return false;
      }
      break;
    case ZIGBEE_AC_MEASUREMENT_TYPE_CURRENT:
      switch (phase_type) {
        case ZIGBEE_AC_PHASE_TYPE_A:            attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_B:            attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_PH_B_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_C:            attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_PH_C_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_NON_SPECIFIC:
        default:                                log_e("Invalid phase type"); return false;
      }
      break;
    case ZIGBEE_AC_MEASUREMENT_TYPE_POWER:
      switch (phase_type) {
        case ZIGBEE_AC_PHASE_TYPE_A:            attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_B:            attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_PH_B_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_C:            attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_PH_C_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_NON_SPECIFIC:
        default:                                log_e("Invalid phase type"); return false;
      }
      break;
    case ZIGBEE_AC_MEASUREMENT_TYPE_FREQUENCY:    attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_FREQUENCY_ID; break;
    case ZIGBEE_AC_MEASUREMENT_TYPE_POWER_FACTOR: log_e("Power factor attribute reporting not supported by zigbee specification"); return false;
    default:                                      log_e("Invalid measurement type"); return false;
  }

  // NOTE(zb-v2): Reporting is now handle-based (was a caller-populated esp_zb_zcl_reporting_info_t).
  // Look up the reporting handle for the attribute, tune the intervals/reportable change, then start.
  ezb_zcl_reporting_info_t reporting_info = ezb_zcl_reporting_info_find(
    _endpoint, EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, attr_id, EZB_ZCL_STD_MANUF_CODE
  );
  if (reporting_info == EZB_ZCL_INVALID_REPORTING_INFO) {
    log_e("Failed to find reporting info for AC measurement");
    return false;
  }
  ezb_zcl_attr_variable_t delta_var = {};
  if (measurement_type == ZIGBEE_AC_MEASUREMENT_TYPE_POWER) {
    delta_var.s16 = int16_delta;
  } else {
    delta_var.u16 = uint16_delta;
  }
  ezb_zcl_reporting_info_update(reporting_info, min_interval, max_interval, &delta_var);
  ezb_zcl_reporting_info_update_default_interval(reporting_info, min_interval, max_interval);
  return setClusterReporting(reporting_info);
}

bool ZigbeeElectricalMeasurement::reportAC(ZIGBEE_AC_MEASUREMENT_TYPE measurement_type, ZIGBEE_AC_PHASE_TYPE phase_type) {
  uint16_t attr_id = 0;

  switch (measurement_type) {
    case ZIGBEE_AC_MEASUREMENT_TYPE_VOLTAGE:
      switch (phase_type) {
        case ZIGBEE_AC_PHASE_TYPE_A:            attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_B:            attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_PH_B_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_C:            attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_PH_C_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_NON_SPECIFIC:
        default:                                log_e("Invalid phase type"); return false;
      }
      break;
    case ZIGBEE_AC_MEASUREMENT_TYPE_CURRENT:
      switch (phase_type) {
        case ZIGBEE_AC_PHASE_TYPE_A:            attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_B:            attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_PH_B_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_C:            attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_PH_C_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_NON_SPECIFIC:
        default:                                log_e("Invalid phase type"); return false;
      }
      break;
    case ZIGBEE_AC_MEASUREMENT_TYPE_POWER:
      switch (phase_type) {
        case ZIGBEE_AC_PHASE_TYPE_A:            attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_B:            attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_PH_B_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_C:            attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_PH_C_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_NON_SPECIFIC:
        default:                                log_e("Invalid phase type"); return false;
      }
      break;
    case ZIGBEE_AC_MEASUREMENT_TYPE_FREQUENCY:    attr_id = EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_FREQUENCY_ID; break;
    case ZIGBEE_AC_MEASUREMENT_TYPE_POWER_FACTOR: log_e("Power factor attribute reporting not supported by zigbee specification"); return false;
    default:                                      log_e("Invalid measurement type"); return false;
  }
  /* Send report attributes command */
  ezb_zcl_report_attr_cmd_t report_attr_cmd;
  memset(&report_attr_cmd, 0, sizeof(report_attr_cmd));
  // No explicit destination: report to bound devices (replaces v1 ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT).
  ezb_address_set_none(&report_attr_cmd.cmd_ctrl.dst_addr);
  report_attr_cmd.cmd_ctrl.src_ep = _endpoint;
  report_attr_cmd.cmd_ctrl.cluster_id = EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT;
  report_attr_cmd.cmd_ctrl.manuf_code = EZB_ZCL_STD_MANUF_CODE;
  report_attr_cmd.cmd_ctrl.fc.direction = EZB_ZCL_CMD_DIRECTION_TO_CLI;
  report_attr_cmd.payload.attr_id = attr_id;

  if (!reportClusterAttribute(&report_attr_cmd)) {
    log_e("Failed to send AC report");
    return false;
  }
  log_v("AC report sent");
  return true;
}

#endif  // CONFIG_ZB_ENABLED
