#include "ZigbeeElectricalMeasurement.h"
#if CONFIG_ZB_ENABLED

// Workaround for wrong name in ZCL header
#define ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_POWER_ID ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DCPOWER_ID

esp_zb_cluster_list_t *zigbee_electrical_measurement_clusters_create(zigbee_electrical_measurement_cfg_t *electrical_measurement) {
  esp_zb_basic_cluster_cfg_t *basic_cfg = electrical_measurement ? &(electrical_measurement->basic_cfg) : NULL;
  esp_zb_identify_cluster_cfg_t *identify_cfg = electrical_measurement ? &(electrical_measurement->identify_cfg) : NULL;
  esp_zb_electrical_meas_cluster_cfg_t *electrical_measurement_cfg = electrical_measurement ? &(electrical_measurement->electrical_measurement_cfg) : NULL;
  esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
  esp_zb_cluster_list_add_basic_cluster(cluster_list, esp_zb_basic_cluster_create(basic_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_identify_cluster(cluster_list, esp_zb_identify_cluster_create(identify_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_electrical_meas_cluster(
    cluster_list, esp_zb_electrical_meas_cluster_create(electrical_measurement_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE
  );
  return cluster_list;
}

ZigbeeElectricalMeasurement::ZigbeeElectricalMeasurement(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID;

  //Create custom pressure sensor configuration
  zigbee_electrical_measurement_cfg_t electrical_measurement_cfg = ZIGBEE_DEFAULT_ELECTRICAL_MEASUREMENT_CONFIG();
  _cluster_list = zigbee_electrical_measurement_clusters_create(&electrical_measurement_cfg);

  _ep_config = {
    .endpoint = _endpoint, .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID, .app_device_id = ESP_ZB_HA_METER_INTERFACE_DEVICE_ID, .app_device_version = 0
  };
}

/* DC MEASUREMENT */

bool ZigbeeElectricalMeasurement::addDCMeasurement(ZIGBEE_DC_MEASUREMENT_TYPE measurement_type) {
  esp_zb_attribute_list_t *electrical_measurement_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

  // Set the DC measurement type bit in the measurement type attribute
  measure_type |= ESP_ZB_ZCL_ELECTRICAL_MEASUREMENT_DC_MEASUREMENT;
  esp_zb_cluster_update_attr(electrical_measurement_cluster, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_MEASUREMENT_TYPE_ID, &measure_type);

  int16_t default_min = -32767;
  int16_t default_max = 32767;
  int16_t default_measurement = 0;
  uint16_t default_multiplier = 1;
  uint16_t default_divisor = 1;

  esp_err_t ret = ESP_OK;
  // Add the DC Voltage attributes
  if (measurement_type == ZIGBEE_DC_MEASUREMENT_TYPE_VOLTAGE) {
    ret = esp_zb_electrical_meas_cluster_add_attr(
      electrical_measurement_cluster, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_VOLTAGE_ID, (void *)&default_measurement
    );
    if (ret != ESP_OK) {
      log_e("Failed to add DC voltage: 0x%x: %s", ret, esp_err_to_name(ret));
      return false;
    }
    ret =
      esp_zb_electrical_meas_cluster_add_attr(electrical_measurement_cluster, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_VOLTAGE_MIN_ID, (void *)&default_min);
    if (ret != ESP_OK) {
      log_e("Failed to add DC voltage min: 0x%x: %s", ret, esp_err_to_name(ret));
      return false;
    }
    ret =
      esp_zb_electrical_meas_cluster_add_attr(electrical_measurement_cluster, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_VOLTAGE_MAX_ID, (void *)&default_max);
    if (ret != ESP_OK) {
      log_e("Failed to add DC voltage max: 0x%x: %s", ret, esp_err_to_name(ret));
      return false;
    }
    ret = esp_zb_electrical_meas_cluster_add_attr(
      electrical_measurement_cluster, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_VOLTAGE_MULTIPLIER_ID, (void *)&default_multiplier
    );
    if (ret != ESP_OK) {
      log_e("Failed to add DC voltage multiplier: 0x%x: %s", ret, esp_err_to_name(ret));
      return false;
    }
    ret = esp_zb_electrical_meas_cluster_add_attr(
      electrical_measurement_cluster, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_VOLTAGE_DIVISOR_ID, (void *)&default_divisor
    );
    if (ret != ESP_OK) {
      log_e("Failed to add DC voltage divisor: 0x%x: %s", ret, esp_err_to_name(ret));
      return false;
    }

  } else if (measurement_type == ZIGBEE_DC_MEASUREMENT_TYPE_CURRENT) {
    // Add the DC Current attributes
    ret = esp_zb_electrical_meas_cluster_add_attr(
      electrical_measurement_cluster, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_CURRENT_ID, (void *)&default_measurement
    );
    if (ret != ESP_OK) {
      log_e("Failed to add DC current: 0x%x: %s", ret, esp_err_to_name(ret));
      return false;
    }
    ret =
      esp_zb_electrical_meas_cluster_add_attr(electrical_measurement_cluster, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_CURRENT_MIN_ID, (void *)&default_min);
    if (ret != ESP_OK) {
      log_e("Failed to add DC current min: 0x%x: %s", ret, esp_err_to_name(ret));
      return false;
    }
    ret =
      esp_zb_electrical_meas_cluster_add_attr(electrical_measurement_cluster, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_CURRENT_MAX_ID, (void *)&default_max);
    if (ret != ESP_OK) {
      log_e("Failed to add DC current max: 0x%x: %s", ret, esp_err_to_name(ret));
      return false;
    }
    ret = esp_zb_electrical_meas_cluster_add_attr(
      electrical_measurement_cluster, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_CURRENT_MULTIPLIER_ID, (void *)&default_multiplier
    );
    if (ret != ESP_OK) {
      log_e("Failed to add DC current multiplier: 0x%x: %s", ret, esp_err_to_name(ret));
      return false;
    }
    ret = esp_zb_electrical_meas_cluster_add_attr(
      electrical_measurement_cluster, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_CURRENT_DIVISOR_ID, (void *)&default_divisor
    );
    if (ret != ESP_OK) {
      log_e("Failed to add DC current divisor: 0x%x: %s", ret, esp_err_to_name(ret));
      return false;
    }
  } else {  //(measurement_type == ZIGBEE_DC_MEASUREMENT_TYPE_POWER)
    // Add the DC Power attributes
    ret =
      esp_zb_electrical_meas_cluster_add_attr(electrical_measurement_cluster, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_POWER_ID, (void *)&default_measurement);
    if (ret != ESP_OK) {
      log_e("Failed to add DC power: 0x%x: %s", ret, esp_err_to_name(ret));
      return false;
    }
    ret = esp_zb_electrical_meas_cluster_add_attr(electrical_measurement_cluster, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_POWER_MIN_ID, (void *)&default_min);
    if (ret != ESP_OK) {
      log_e("Failed to add DC power min: 0x%x: %s", ret, esp_err_to_name(ret));
      return false;
    }
    ret = esp_zb_electrical_meas_cluster_add_attr(electrical_measurement_cluster, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_POWER_MAX_ID, (void *)&default_max);
    if (ret != ESP_OK) {
      log_e("Failed to add DC power max: 0x%x: %s", ret, esp_err_to_name(ret));
      return false;
    }
    ret = esp_zb_electrical_meas_cluster_add_attr(
      electrical_measurement_cluster, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_POWER_MULTIPLIER_ID, (void *)&default_multiplier
    );
    if (ret != ESP_OK) {
      log_e("Failed to add DC power multiplier: 0x%x: %s", ret, esp_err_to_name(ret));
      return false;
    }
    ret = esp_zb_electrical_meas_cluster_add_attr(
      electrical_measurement_cluster, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_POWER_DIVISOR_ID, (void *)&default_divisor
    );
    if (ret != ESP_OK) {
      log_e("Failed to add DC power divisor: 0x%x: %s", ret, esp_err_to_name(ret));
      return false;
    }
  }
  return true;
}

bool ZigbeeElectricalMeasurement::setDCMinMaxValue(ZIGBEE_DC_MEASUREMENT_TYPE measurement_type, int16_t min, int16_t max) {
  esp_zb_attribute_list_t *electrical_measurement_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

  esp_zb_zcl_electrical_measurement_attr_t attr_min = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_VOLTAGE_MIN_ID;
  esp_zb_zcl_electrical_measurement_attr_t attr_max = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_VOLTAGE_MAX_ID;
  if (measurement_type == ZIGBEE_DC_MEASUREMENT_TYPE_CURRENT) {
    attr_min = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_CURRENT_MIN_ID;
    attr_max = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_CURRENT_MAX_ID;
  } else if (measurement_type == ZIGBEE_DC_MEASUREMENT_TYPE_POWER) {
    attr_min = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_POWER_MIN_ID;
    attr_max = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_POWER_MAX_ID;
  }
  esp_err_t ret = ESP_OK;
  ret = esp_zb_cluster_update_attr(electrical_measurement_cluster, attr_min, (void *)&min);
  if (ret != ESP_OK) {
    log_e("Failed to set min value: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  ret = esp_zb_cluster_update_attr(electrical_measurement_cluster, attr_max, (void *)&max);
  if (ret != ESP_OK) {
    log_e("Failed to set max value: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeElectricalMeasurement::setDCMultiplierDivisor(ZIGBEE_DC_MEASUREMENT_TYPE measurement_type, uint16_t multiplier, uint16_t divisor) {
  esp_zb_attribute_list_t *electrical_measurement_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

  esp_zb_zcl_electrical_measurement_attr_t attr_multiplier = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_VOLTAGE_MULTIPLIER_ID;
  esp_zb_zcl_electrical_measurement_attr_t attr_divisor = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_VOLTAGE_DIVISOR_ID;

  if (measurement_type == ZIGBEE_DC_MEASUREMENT_TYPE_CURRENT) {
    attr_multiplier = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_CURRENT_MULTIPLIER_ID;
    attr_divisor = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_CURRENT_DIVISOR_ID;
  } else if (measurement_type == ZIGBEE_DC_MEASUREMENT_TYPE_POWER) {
    attr_multiplier = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_POWER_MULTIPLIER_ID;
    attr_divisor = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_POWER_DIVISOR_ID;
  }

  esp_err_t ret = ESP_OK;
  ret = esp_zb_cluster_update_attr(electrical_measurement_cluster, attr_multiplier, (void *)&multiplier);
  if (ret != ESP_OK) {
    log_e("Failed to set multiplier: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  ret = esp_zb_cluster_update_attr(electrical_measurement_cluster, attr_divisor, (void *)&divisor);
  if (ret != ESP_OK) {
    log_e("Failed to set divisor: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeElectricalMeasurement::setDCReporting(ZIGBEE_DC_MEASUREMENT_TYPE measurement_type, uint16_t min_interval, uint16_t max_interval, int16_t delta) {
  uint16_t attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_VOLTAGE_ID;
  if (measurement_type == ZIGBEE_DC_MEASUREMENT_TYPE_CURRENT) {
    attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_CURRENT_ID;
  } else if (measurement_type == ZIGBEE_DC_MEASUREMENT_TYPE_POWER) {
    attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_POWER_ID;
  }

  esp_zb_zcl_reporting_info_t reporting_info;
  memset(&reporting_info, 0, sizeof(esp_zb_zcl_reporting_info_t));
  reporting_info.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV;
  reporting_info.ep = _endpoint;
  reporting_info.cluster_id = ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT;
  reporting_info.cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE;
  reporting_info.attr_id = attr_id;
  reporting_info.u.send_info.min_interval = min_interval;
  reporting_info.u.send_info.max_interval = max_interval;
  reporting_info.u.send_info.def_min_interval = min_interval;
  reporting_info.u.send_info.def_max_interval = max_interval;
  reporting_info.u.send_info.delta.s16 = delta;
  reporting_info.dst.profile_id = ESP_ZB_AF_HA_PROFILE_ID;
  reporting_info.manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC;
  esp_zb_lock_acquire(portMAX_DELAY);
  esp_err_t ret = esp_zb_zcl_update_reporting_info(&reporting_info);
  esp_zb_lock_release();
  if (ret != ESP_OK) {
    log_e("Failed to set reporting: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeElectricalMeasurement::setDCMeasurement(ZIGBEE_DC_MEASUREMENT_TYPE measurement_type, int16_t measurement) {
  esp_zb_zcl_status_t ret = ESP_ZB_ZCL_STATUS_SUCCESS;

  esp_zb_zcl_electrical_measurement_attr_t attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_VOLTAGE_ID;
  if (measurement_type == ZIGBEE_DC_MEASUREMENT_TYPE_CURRENT) {
    attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_CURRENT_ID;
  } else if (measurement_type == ZIGBEE_DC_MEASUREMENT_TYPE_POWER) {
    attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_POWER_ID;
  }

  log_v("Updating DC measurement value...");
  /* Update DC sensor measured value */
  log_d("Setting DC measurement to %d", measurement);
  esp_zb_lock_acquire(portMAX_DELAY);
  ret = esp_zb_zcl_set_attribute_val(_endpoint, ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, attr_id, &measurement, false);
  esp_zb_lock_release();
  if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set DC measurement: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeElectricalMeasurement::reportDC(ZIGBEE_DC_MEASUREMENT_TYPE measurement_type) {
  uint16_t attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_VOLTAGE_ID;
  if (measurement_type == ZIGBEE_DC_MEASUREMENT_TYPE_CURRENT) {
    attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_CURRENT_ID;
  } else if (measurement_type == ZIGBEE_DC_MEASUREMENT_TYPE_POWER) {
    attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_POWER_ID;
  }
  /* Send report attributes command */
  esp_zb_zcl_report_attr_cmd_t report_attr_cmd;
  report_attr_cmd.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
  report_attr_cmd.attributeID = attr_id;
  report_attr_cmd.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI;
  report_attr_cmd.clusterID = ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT;
  report_attr_cmd.zcl_basic_cmd.src_endpoint = _endpoint;
  report_attr_cmd.manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC;

  esp_zb_lock_acquire(portMAX_DELAY);
  esp_err_t ret = esp_zb_zcl_report_attr_cmd_req(&report_attr_cmd);
  esp_zb_lock_release();
  if (ret != ESP_OK) {
    log_e("Failed to send DC report: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  log_v("DC report sent");
  return true;
}

/* AC MEASUREMENT */

bool ZigbeeElectricalMeasurement::addACMeasurement(ZIGBEE_AC_MEASUREMENT_TYPE measurement_type, ZIGBEE_AC_PHASE_TYPE phase_type) {
  esp_zb_attribute_list_t *electrical_measurement_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

  switch (phase_type) {
    case ZIGBEE_AC_PHASE_TYPE_NON_SPECIFIC:
      // Non phase specific dont need any bit set
      break;
    case ZIGBEE_AC_PHASE_TYPE_A: measure_type |= ESP_ZB_ZCL_ELECTRICAL_MEASUREMENT_PHASE_A_MEASUREMENT; break;
    case ZIGBEE_AC_PHASE_TYPE_B: measure_type |= ESP_ZB_ZCL_ELECTRICAL_MEASUREMENT_PHASE_B_MEASUREMENT; break;
    case ZIGBEE_AC_PHASE_TYPE_C: measure_type |= ESP_ZB_ZCL_ELECTRICAL_MEASUREMENT_PHASE_C_MEASUREMENT; break;
    default:                     log_e("Invalid phase type"); break;
  }
  // Set Active measurement bit for active power and power factor, otherwise no bit needed for voltage, current
  if (measurement_type == ZIGBEE_AC_MEASUREMENT_TYPE_POWER || measurement_type == ZIGBEE_AC_MEASUREMENT_TYPE_POWER_FACTOR) {
    measure_type |= ESP_ZB_ZCL_ELECTRICAL_MEASUREMENT_ACTIVE_MEASUREMENT;  // Active power is used
  }
  // Update the measurement type attribute
  esp_zb_cluster_update_attr(electrical_measurement_cluster, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_MEASUREMENT_TYPE_ID, &measure_type);

  // Default values for AC measurements
  [[maybe_unused]]
  int16_t default_ac_power_min = -32767;
  [[maybe_unused]]
  int16_t default_ac_power_max = 32767;
  [[maybe_unused]]
  int16_t default_ac_power_measurement = 0;
  [[maybe_unused]]
  uint16_t default_ac_min = 0x0000;
  [[maybe_unused]]
  uint16_t default_ac_max = 0xffff;
  [[maybe_unused]]
  uint16_t default_ac_measurement = 0x0000;
  [[maybe_unused]]
  uint16_t default_ac_multiplier = 1;
  [[maybe_unused]]
  uint16_t default_ac_divisor = 1;
  [[maybe_unused]]
  int8_t default_ac_power_factor = 100;

  esp_err_t ret = ESP_OK;

  // AC Frequency
  if (measurement_type == ZIGBEE_AC_MEASUREMENT_TYPE_FREQUENCY) {  // No phase specific
    ret = esp_zb_electrical_meas_cluster_add_attr(
      electrical_measurement_cluster, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_FREQUENCY_ID, (void *)&default_ac_measurement
    );
    if (ret != ESP_OK) {
      log_e("Failed to add DC voltage: 0x%x: %s", ret, esp_err_to_name(ret));
      return false;
    }
    ret = esp_zb_electrical_meas_cluster_add_attr(
      electrical_measurement_cluster, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_FREQUENCY_MIN_ID, (void *)&default_ac_min
    );
    if (ret != ESP_OK) {
      log_e("Failed to add DC voltage min: 0x%x: %s", ret, esp_err_to_name(ret));
      return false;
    }
    ret = esp_zb_electrical_meas_cluster_add_attr(
      electrical_measurement_cluster, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_FREQUENCY_MAX_ID, (void *)&default_ac_max
    );
    if (ret != ESP_OK) {
      log_e("Failed to add DC voltage max: 0x%x: %s", ret, esp_err_to_name(ret));
      return false;
    }
    ret = esp_zb_electrical_meas_cluster_add_attr(
      electrical_measurement_cluster, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_FREQUENCY_MULTIPLIER_ID, (void *)&default_ac_multiplier
    );
    if (ret != ESP_OK) {
      log_e("Failed to add DC voltage multiplier: 0x%x: %s", ret, esp_err_to_name(ret));
      return false;
    }
    ret = esp_zb_electrical_meas_cluster_add_attr(
      electrical_measurement_cluster, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_FREQUENCY_DIVISOR_ID, (void *)&default_ac_divisor
    );
    if (ret != ESP_OK) {
      log_e("Failed to add DC voltage divisor: 0x%x: %s", ret, esp_err_to_name(ret));
      return false;
    }
    return true;
  }
  // Add the AC Voltage attributes
  else if (measurement_type == ZIGBEE_AC_MEASUREMENT_TYPE_VOLTAGE) {
    esp_zb_zcl_electrical_measurement_attr_t attr_voltage = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSVOLTAGE_ID;
    esp_zb_zcl_electrical_measurement_attr_t attr_voltage_min = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_MIN_ID;
    esp_zb_zcl_electrical_measurement_attr_t attr_voltage_max = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_MAX_ID;
    switch (phase_type) {
      case ZIGBEE_AC_PHASE_TYPE_A:
        // already set
        break;
      case ZIGBEE_AC_PHASE_TYPE_B:
        attr_voltage = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSVOLTAGE_PHB_ID;
        attr_voltage_min = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_MIN_PH_B_ID;
        attr_voltage_max = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_MAX_PH_B_ID;
        break;
      case ZIGBEE_AC_PHASE_TYPE_C:
        attr_voltage = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSVOLTAGE_PHC_ID;
        attr_voltage_min = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_MIN_PH_C_ID;
        attr_voltage_max = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_MAX_PH_C_ID;
        break;
      default: log_e("Invalid phase type"); return false;
    }
    ret = esp_zb_electrical_meas_cluster_add_attr(electrical_measurement_cluster, attr_voltage, (void *)&default_ac_measurement);
    if (ret != ESP_OK) {
      log_e("Failed to add AC voltage: 0x%x: %s", ret, esp_err_to_name(ret));
      return false;
    }
    ret = esp_zb_electrical_meas_cluster_add_attr(electrical_measurement_cluster, attr_voltage_min, (void *)&default_ac_min);
    if (ret != ESP_OK) {
      log_e("Failed to add AC voltage min: 0x%x: %s", ret, esp_err_to_name(ret));
      return false;
    }
    ret = esp_zb_electrical_meas_cluster_add_attr(electrical_measurement_cluster, attr_voltage_max, (void *)&default_ac_max);
    if (ret != ESP_OK) {
      log_e("Failed to add AC voltage max: 0x%x: %s", ret, esp_err_to_name(ret));
      return false;
    }
    if (!ac_volt_mult_div_set) {
      ret = esp_zb_electrical_meas_cluster_add_attr(
        electrical_measurement_cluster, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACVOLTAGE_MULTIPLIER_ID, (void *)&default_ac_multiplier
      );
      if (ret != ESP_OK) {
        log_e("Failed to add AC voltage multiplier: 0x%x: %s", ret, esp_err_to_name(ret));
        return false;
      }
      ret = esp_zb_electrical_meas_cluster_add_attr(
        electrical_measurement_cluster, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACVOLTAGE_DIVISOR_ID, (void *)&default_ac_divisor
      );
      if (ret != ESP_OK) {
        log_e("Failed to add AC voltage divisor: 0x%x: %s", ret, esp_err_to_name(ret));
        return false;
      }
      ac_volt_mult_div_set = true;  // Set flag to true, so we dont add the attributes again
    }
  }
  // Add the AC Current attributes
  else if (measurement_type == ZIGBEE_AC_MEASUREMENT_TYPE_CURRENT) {
    esp_zb_zcl_electrical_measurement_attr_t attr_current = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSCURRENT_ID;
    esp_zb_zcl_electrical_measurement_attr_t attr_current_min = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_MIN_ID;
    esp_zb_zcl_electrical_measurement_attr_t attr_current_max = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_MAX_ID;
    switch (phase_type) {
      case ZIGBEE_AC_PHASE_TYPE_A:
        // already set
        break;
      case ZIGBEE_AC_PHASE_TYPE_B:
        attr_current = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSCURRENT_PHB_ID;
        attr_current_min = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_MIN_PH_B_ID;
        attr_current_max = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_MAX_PH_B_ID;
        break;
      case ZIGBEE_AC_PHASE_TYPE_C:
        attr_current = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSCURRENT_PHC_ID;
        attr_current_min = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_MIN_PH_C_ID;
        attr_current_max = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_MAX_PH_C_ID;
        break;
      default: log_e("Invalid phase type"); return false;
    }
    ret = esp_zb_electrical_meas_cluster_add_attr(electrical_measurement_cluster, attr_current, (void *)&default_ac_measurement);
    if (ret != ESP_OK) {
      log_e("Failed to add AC current: 0x%x: %s", ret, esp_err_to_name(ret));
      return false;
    }
    ret = esp_zb_electrical_meas_cluster_add_attr(electrical_measurement_cluster, attr_current_min, (void *)&default_ac_min);
    if (ret != ESP_OK) {
      log_e("Failed to add AC current min: 0x%x: %s", ret, esp_err_to_name(ret));
      return false;
    }
    ret = esp_zb_electrical_meas_cluster_add_attr(electrical_measurement_cluster, attr_current_max, (void *)&default_ac_max);
    if (ret != ESP_OK) {
      log_e("Failed to add AC current max: 0x%x: %s", ret, esp_err_to_name(ret));
      return false;
    }
    if (!ac_current_mult_div_set) {
      ret = esp_zb_electrical_meas_cluster_add_attr(
        electrical_measurement_cluster, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACCURRENT_MULTIPLIER_ID, (void *)&default_ac_multiplier
      );
      if (ret != ESP_OK) {
        log_e("Failed to add AC current multiplier: 0x%x: %s", ret, esp_err_to_name(ret));
        return false;
      }
      ret = esp_zb_electrical_meas_cluster_add_attr(
        electrical_measurement_cluster, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACCURRENT_DIVISOR_ID, (void *)&default_ac_divisor
      );
      if (ret != ESP_OK) {
        log_e("Failed to add AC current divisor: 0x%x: %s", ret, esp_err_to_name(ret));
        return false;
      }
      ac_current_mult_div_set = true;  // Set flag to true, so we dont add the attributes again
    }
  }
  // Add the AC Power attributes
  else if (measurement_type == ZIGBEE_AC_MEASUREMENT_TYPE_POWER) {
    esp_zb_zcl_electrical_measurement_attr_t attr_power = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_ID;
    esp_zb_zcl_electrical_measurement_attr_t attr_power_min = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_MIN_ID;
    esp_zb_zcl_electrical_measurement_attr_t attr_power_max = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_MAX_ID;

    switch (phase_type) {
      case ZIGBEE_AC_PHASE_TYPE_A:
        // already set
        break;
      case ZIGBEE_AC_PHASE_TYPE_B:
        attr_power = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_PHB_ID;
        attr_power_min = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_MIN_PH_B_ID;
        attr_power_max = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_MAX_PH_B_ID;
        break;
      case ZIGBEE_AC_PHASE_TYPE_C:
        attr_power = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_PHC_ID;
        attr_power_min = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_MIN_PH_C_ID;
        attr_power_max = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_MAX_PH_C_ID;
        break;
      default: log_e("Invalid phase type"); return false;
    }
    ret = esp_zb_electrical_meas_cluster_add_attr(electrical_measurement_cluster, attr_power, (void *)&default_ac_measurement);
    if (ret != ESP_OK) {
      log_e("Failed to add AC power: 0x%x: %s", ret, esp_err_to_name(ret));
      return false;
    }
    ret = esp_zb_electrical_meas_cluster_add_attr(electrical_measurement_cluster, attr_power_min, (void *)&default_ac_min);
    if (ret != ESP_OK) {
      log_e("Failed to add AC power min: 0x%x: %s", ret, esp_err_to_name(ret));
      return false;
    }
    ret = esp_zb_electrical_meas_cluster_add_attr(electrical_measurement_cluster, attr_power_max, (void *)&default_ac_max);
    if (ret != ESP_OK) {
      log_e("Failed to add AC power max: 0x%x: %s", ret, esp_err_to_name(ret));
      return false;
    }
    if (!ac_power_mult_div_set) {
      ret = esp_zb_electrical_meas_cluster_add_attr(
        electrical_measurement_cluster, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACPOWER_MULTIPLIER_ID, (void *)&default_ac_multiplier
      );
      if (ret != ESP_OK) {
        log_e("Failed to add AC power multiplier: 0x%x: %s", ret, esp_err_to_name(ret));
        return false;
      }
      ret = esp_zb_electrical_meas_cluster_add_attr(
        electrical_measurement_cluster, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACPOWER_DIVISOR_ID, (void *)&default_ac_divisor
      );
      if (ret != ESP_OK) {
        log_e("Failed to add AC power divisor: 0x%x: %s", ret, esp_err_to_name(ret));
        return false;
      }
      ac_power_mult_div_set = true;  // Set flag to true, so we dont add the attributes again
    }
  } else {  //(measurement_type == ZIGBEE_AC_MEASUREMENT_TYPE_POWER_FACTOR)
    esp_zb_zcl_electrical_measurement_attr_t attr_power_factor = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_POWER_FACTOR_ID;

    switch (phase_type) {
      case ZIGBEE_AC_PHASE_TYPE_A:
        // already set
        break;
      case ZIGBEE_AC_PHASE_TYPE_B: attr_power_factor = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_POWER_FACTOR_PH_B_ID; break;
      case ZIGBEE_AC_PHASE_TYPE_C: attr_power_factor = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_POWER_FACTOR_PH_C_ID; break;
      default:                     log_e("Invalid phase type"); return false;
    }
    ret = esp_zb_electrical_meas_cluster_add_attr(electrical_measurement_cluster, attr_power_factor, (void *)&default_ac_power_factor);
    if (ret != ESP_OK) {
      log_e("Failed to add AC power factor: 0x%x: %s", ret, esp_err_to_name(ret));
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
        log_e("AC measurement min/max values must be between 0 and %u (got min=%d, max=%d)", UINT16_MAX, min_value, max_value);
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

  [[maybe_unused]]
  int16_t int16_min_value = (int16_t)min_value;
  [[maybe_unused]]
  int16_t int16_max_value = (int16_t)max_value;
  [[maybe_unused]]
  uint16_t uint16_min_value = (uint16_t)min_value;
  [[maybe_unused]]
  uint16_t uint16_max_value = (uint16_t)max_value;

  //TODO: Log info about min and max values for different measurement types
  switch (measurement_type) {
    case ZIGBEE_AC_MEASUREMENT_TYPE_VOLTAGE:
      switch (phase_type) {
        case ZIGBEE_AC_PHASE_TYPE_A:
          attr_min_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_MIN_ID;
          attr_max_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_MAX_ID;
          break;
        case ZIGBEE_AC_PHASE_TYPE_B:
          attr_min_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_MIN_PH_B_ID;
          attr_max_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_MAX_PH_B_ID;
          break;
        case ZIGBEE_AC_PHASE_TYPE_C:
          attr_min_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_MIN_PH_C_ID;
          attr_max_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_MAX_PH_C_ID;
          break;
        default: log_e("Invalid phase type"); return false;
      }
      break;

    case ZIGBEE_AC_MEASUREMENT_TYPE_CURRENT:
      switch (phase_type) {
        case ZIGBEE_AC_PHASE_TYPE_A:
          attr_min_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_MIN_ID;
          attr_max_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_MAX_ID;
          break;
        case ZIGBEE_AC_PHASE_TYPE_B:
          attr_min_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_MIN_PH_B_ID;
          attr_max_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_MAX_PH_B_ID;
          break;
        case ZIGBEE_AC_PHASE_TYPE_C:
          attr_min_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_MIN_PH_C_ID;
          attr_max_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_MAX_PH_C_ID;
          break;
        default: log_e("Invalid phase type"); return false;
      }
      break;

    case ZIGBEE_AC_MEASUREMENT_TYPE_POWER:
      switch (phase_type) {
        case ZIGBEE_AC_PHASE_TYPE_A:
          attr_min_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_MIN_ID;
          attr_max_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_MAX_ID;
          break;
        case ZIGBEE_AC_PHASE_TYPE_B:
          attr_min_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_MIN_PH_B_ID;
          attr_max_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_MAX_PH_B_ID;
          break;
        case ZIGBEE_AC_PHASE_TYPE_C:
          attr_min_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_MIN_PH_C_ID;
          attr_max_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_MAX_PH_C_ID;
          break;
        default: log_e("Invalid phase type"); return false;
      }
      break;

    default: log_e("Invalid measurement type"); return false;
  }
  esp_zb_attribute_list_t *electrical_measurement_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

  esp_err_t ret = ESP_OK;
  ret = esp_zb_cluster_update_attr(electrical_measurement_cluster, attr_min_id, (void *)&min_value);
  if (ret != ESP_OK) {
    log_e("Failed to set min value: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  ret = esp_zb_cluster_update_attr(electrical_measurement_cluster, attr_max_id, (void *)&max_value);
  if (ret != ESP_OK) {
    log_e("Failed to set max value: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeElectricalMeasurement::setACMultiplierDivisor(ZIGBEE_AC_MEASUREMENT_TYPE measurement_type, uint16_t multiplier, uint16_t divisor) {
  uint16_t attr_multiplier = 0;
  uint16_t attr_divisor = 0;

  switch (measurement_type) {
    case ZIGBEE_AC_MEASUREMENT_TYPE_VOLTAGE:
      attr_multiplier = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACVOLTAGE_MULTIPLIER_ID;
      attr_divisor = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACVOLTAGE_DIVISOR_ID;
      break;
    case ZIGBEE_AC_MEASUREMENT_TYPE_CURRENT:
      attr_multiplier = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACCURRENT_MULTIPLIER_ID;
      attr_divisor = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACCURRENT_DIVISOR_ID;
      break;
    case ZIGBEE_AC_MEASUREMENT_TYPE_POWER:
      attr_multiplier = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACPOWER_MULTIPLIER_ID;
      attr_divisor = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACPOWER_DIVISOR_ID;
      break;
    case ZIGBEE_AC_MEASUREMENT_TYPE_FREQUENCY:
      attr_multiplier = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_FREQUENCY_MULTIPLIER_ID;
      attr_divisor = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_FREQUENCY_DIVISOR_ID;
      break;
    default: log_e("Invalid measurement type"); return false;
  }
  esp_zb_attribute_list_t *electrical_measurement_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

  esp_err_t ret = ESP_OK;
  ret = esp_zb_cluster_update_attr(electrical_measurement_cluster, attr_multiplier, (void *)&multiplier);
  if (ret != ESP_OK) {
    log_e("Failed to set multiplier: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  ret = esp_zb_cluster_update_attr(electrical_measurement_cluster, attr_divisor, (void *)&divisor);
  if (ret != ESP_OK) {
    log_e("Failed to set divisor: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeElectricalMeasurement::setACPowerFactor(ZIGBEE_AC_PHASE_TYPE phase_type, int8_t power_factor) {
  uint16_t attr_id = 0;

  switch (phase_type) {
    case ZIGBEE_AC_PHASE_TYPE_A:            attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_POWER_FACTOR_ID; break;
    case ZIGBEE_AC_PHASE_TYPE_B:            attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_POWER_FACTOR_PH_B_ID; break;
    case ZIGBEE_AC_PHASE_TYPE_C:            attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_POWER_FACTOR_PH_C_ID; break;
    case ZIGBEE_AC_PHASE_TYPE_NON_SPECIFIC:
    default:                                log_e("Invalid phase type"); return false;
  }

  esp_zb_attribute_list_t *electrical_measurement_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

  esp_err_t ret = ESP_OK;
  ret = esp_zb_cluster_update_attr(electrical_measurement_cluster, attr_id, (void *)&power_factor);
  if (ret != ESP_OK) {
    log_e("Failed to set power factor: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  return true;
}
bool ZigbeeElectricalMeasurement::setACMeasurement(ZIGBEE_AC_MEASUREMENT_TYPE measurement_type, ZIGBEE_AC_PHASE_TYPE phase_type, int32_t value) {
  esp_zb_zcl_status_t ret = ESP_ZB_ZCL_STATUS_SUCCESS;
  uint16_t attr_id = 0;

  // Check value is valid for the measurement type
  switch (measurement_type) {
    case ZIGBEE_AC_MEASUREMENT_TYPE_VOLTAGE:
    case ZIGBEE_AC_MEASUREMENT_TYPE_CURRENT:
    case ZIGBEE_AC_MEASUREMENT_TYPE_FREQUENCY:
      if (value < 0 || value > UINT16_MAX) {
        log_e("AC measurement value must be between 0 and %u (got %d)", UINT16_MAX, value);
        return false;
      }
      break;

    case ZIGBEE_AC_MEASUREMENT_TYPE_POWER:
      if (value < INT16_MIN || value > INT16_MAX) {
        log_e("AC power value must be between %d and %d (got %d)", INT16_MIN, INT16_MAX, value);
        return false;
      }
      break;

    case ZIGBEE_AC_MEASUREMENT_TYPE_POWER_FACTOR:
      if (value < -100 || value > 100) {
        log_e("AC power factor value must be between -100 and 100 (got %d)", value);
        return false;
      }
      break;

    default: log_e("Invalid measurement type"); return false;
  }
  // Convert value to appropriate type based on measurement type
  uint16_t uint16_value = (uint16_t)value;
  int16_t int16_value = (int16_t)value;
  int8_t int8_value = (int8_t)value;

  switch (measurement_type) {
    case ZIGBEE_AC_MEASUREMENT_TYPE_VOLTAGE:
      switch (phase_type) {
        case ZIGBEE_AC_PHASE_TYPE_A:            attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSVOLTAGE_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_B:            attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSVOLTAGE_PHB_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_C:            attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSVOLTAGE_PHC_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_NON_SPECIFIC:
        default:                                log_e("Invalid phase type"); return false;
      }
      // Use uint16_t for voltage
      log_v("Updating AC voltage measurement value...");
      log_d("Setting AC voltage to %u", uint16_value);
      esp_zb_lock_acquire(portMAX_DELAY);
      ret =
        esp_zb_zcl_set_attribute_val(_endpoint, ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, attr_id, &uint16_value, false);
      esp_zb_lock_release();
      break;

    case ZIGBEE_AC_MEASUREMENT_TYPE_CURRENT:
      switch (phase_type) {
        case ZIGBEE_AC_PHASE_TYPE_A:            attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSCURRENT_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_B:            attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSCURRENT_PHB_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_C:            attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSCURRENT_PHC_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_NON_SPECIFIC:
        default:                                log_e("Invalid phase type"); return false;
      }
      // Use uint16_t for current
      log_v("Updating AC current measurement value...");
      log_d("Setting AC current to %u", uint16_value);
      esp_zb_lock_acquire(portMAX_DELAY);
      ret =
        esp_zb_zcl_set_attribute_val(_endpoint, ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, attr_id, &uint16_value, false);
      esp_zb_lock_release();
      break;

    case ZIGBEE_AC_MEASUREMENT_TYPE_POWER:
      switch (phase_type) {
        case ZIGBEE_AC_PHASE_TYPE_A:            attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_B:            attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_PHB_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_C:            attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_PHC_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_NON_SPECIFIC:
        default:                                log_e("Invalid phase type"); return false;
      }
      // Use int16_t for power
      log_v("Updating AC power measurement value...");
      log_d("Setting AC power to %d", int16_value);
      esp_zb_lock_acquire(portMAX_DELAY);
      ret = esp_zb_zcl_set_attribute_val(_endpoint, ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, attr_id, &int16_value, false);
      esp_zb_lock_release();
      break;

    case ZIGBEE_AC_MEASUREMENT_TYPE_FREQUENCY:
      attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_FREQUENCY_ID;
      // Use uint16_t for frequency
      log_v("Updating AC frequency measurement value...");
      log_d("Setting AC frequency to %u", uint16_value);
      esp_zb_lock_acquire(portMAX_DELAY);
      ret =
        esp_zb_zcl_set_attribute_val(_endpoint, ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, attr_id, &uint16_value, false);
      esp_zb_lock_release();
      break;
    case ZIGBEE_AC_MEASUREMENT_TYPE_POWER_FACTOR:
      switch (phase_type) {
        case ZIGBEE_AC_PHASE_TYPE_A:            attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_POWER_FACTOR_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_B:            attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_POWER_FACTOR_PH_B_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_C:            attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_POWER_FACTOR_PH_C_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_NON_SPECIFIC:
        default:                                log_e("Invalid phase type"); return false;
      }
      // Use int8_t for power factor
      log_v("Updating AC power factor measurement value...");
      log_d("Setting AC power factor to %d", int8_value);
      esp_zb_lock_acquire(portMAX_DELAY);
      ret = esp_zb_zcl_set_attribute_val(_endpoint, ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, attr_id, &int8_value, false);
      esp_zb_lock_release();
      break;
    default: log_e("Invalid measurement type"); return false;
  }

  if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set AC measurement: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  return true;
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
        case ZIGBEE_AC_PHASE_TYPE_A:            attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSVOLTAGE_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_B:            attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSVOLTAGE_PHB_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_C:            attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSVOLTAGE_PHC_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_NON_SPECIFIC:
        default:                                log_e("Invalid phase type"); return false;
      }
      break;
    case ZIGBEE_AC_MEASUREMENT_TYPE_CURRENT:
      switch (phase_type) {
        case ZIGBEE_AC_PHASE_TYPE_A:            attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSCURRENT_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_B:            attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSCURRENT_PHB_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_C:            attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSCURRENT_PHC_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_NON_SPECIFIC:
        default:                                log_e("Invalid phase type"); return false;
      }
      break;
    case ZIGBEE_AC_MEASUREMENT_TYPE_POWER:
      switch (phase_type) {
        case ZIGBEE_AC_PHASE_TYPE_A:            attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_B:            attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_PHB_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_C:            attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_PHC_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_NON_SPECIFIC:
        default:                                log_e("Invalid phase type"); return false;
      }
      break;
    case ZIGBEE_AC_MEASUREMENT_TYPE_FREQUENCY:    attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_FREQUENCY_ID; break;
    case ZIGBEE_AC_MEASUREMENT_TYPE_POWER_FACTOR: log_e("Power factor attribute reporting not supported by zigbee specification"); return false;
    default:                                      log_e("Invalid measurement type"); return false;
  }

  esp_zb_zcl_reporting_info_t reporting_info;
  memset(&reporting_info, 0, sizeof(esp_zb_zcl_reporting_info_t));
  reporting_info.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV;
  reporting_info.ep = _endpoint;
  reporting_info.cluster_id = ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT;
  reporting_info.cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE;
  reporting_info.attr_id = attr_id;
  reporting_info.u.send_info.min_interval = min_interval;
  reporting_info.u.send_info.max_interval = max_interval;
  reporting_info.u.send_info.def_min_interval = min_interval;
  reporting_info.u.send_info.def_max_interval = max_interval;
  if (measurement_type == ZIGBEE_AC_MEASUREMENT_TYPE_POWER) {
    reporting_info.u.send_info.delta.s16 = int16_delta;
  } else {
    reporting_info.u.send_info.delta.u16 = uint16_delta;
  }
  reporting_info.dst.profile_id = ESP_ZB_AF_HA_PROFILE_ID;
  reporting_info.manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC;
  esp_zb_lock_acquire(portMAX_DELAY);
  esp_err_t ret = esp_zb_zcl_update_reporting_info(&reporting_info);
  esp_zb_lock_release();
  if (ret != ESP_OK) {
    log_e("Failed to set reporting: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeElectricalMeasurement::reportAC(ZIGBEE_AC_MEASUREMENT_TYPE measurement_type, ZIGBEE_AC_PHASE_TYPE phase_type) {
  uint16_t attr_id = 0;

  switch (measurement_type) {
    case ZIGBEE_AC_MEASUREMENT_TYPE_VOLTAGE:
      switch (phase_type) {
        case ZIGBEE_AC_PHASE_TYPE_A:            attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSVOLTAGE_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_B:            attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSVOLTAGE_PHB_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_C:            attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSVOLTAGE_PHC_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_NON_SPECIFIC:
        default:                                log_e("Invalid phase type"); return false;
      }
      break;
    case ZIGBEE_AC_MEASUREMENT_TYPE_CURRENT:
      switch (phase_type) {
        case ZIGBEE_AC_PHASE_TYPE_A:            attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSCURRENT_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_B:            attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSCURRENT_PHB_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_C:            attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSCURRENT_PHC_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_NON_SPECIFIC:
        default:                                log_e("Invalid phase type"); return false;
      }
      break;
    case ZIGBEE_AC_MEASUREMENT_TYPE_POWER:
      switch (phase_type) {
        case ZIGBEE_AC_PHASE_TYPE_A:            attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_B:            attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_PHB_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_C:            attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_PHC_ID; break;
        case ZIGBEE_AC_PHASE_TYPE_NON_SPECIFIC:
        default:                                log_e("Invalid phase type"); return false;
      }
      break;
    case ZIGBEE_AC_MEASUREMENT_TYPE_FREQUENCY:    attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_FREQUENCY_ID; break;
    case ZIGBEE_AC_MEASUREMENT_TYPE_POWER_FACTOR: log_e("Power factor attribute reporting not supported by zigbee specification"); return false;
    default:                                      log_e("Invalid measurement type"); return false;
  }
  /* Send report attributes command */
  esp_zb_zcl_report_attr_cmd_t report_attr_cmd;
  report_attr_cmd.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
  report_attr_cmd.attributeID = attr_id;
  report_attr_cmd.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI;
  report_attr_cmd.clusterID = ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT;
  report_attr_cmd.zcl_basic_cmd.src_endpoint = _endpoint;
  report_attr_cmd.manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC;

  esp_zb_lock_acquire(portMAX_DELAY);
  esp_err_t ret = esp_zb_zcl_report_attr_cmd_req(&report_attr_cmd);
  esp_zb_lock_release();
  if (ret != ESP_OK) {
    log_e("Failed to send AC report: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  log_v("AC report sent");
  return true;
}

#endif  // CONFIG_ZB_ENABLED
