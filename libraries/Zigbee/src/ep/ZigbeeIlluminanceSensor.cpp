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

#include "ZigbeeIlluminanceSensor.h"
#if CONFIG_ZB_ENABLED

ZigbeeIlluminanceSensor::ZigbeeIlluminanceSensor(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = EZB_ZHA_LIGHT_SENSOR_DEVICE_ID;

  // v2.x data model: the ZHA Light Sensor template builds the full endpoint descriptor (basic, identify,
  // illuminance measurement clusters) instead of the v1 cluster-list factory.
  ezb_zha_light_sensor_config_t light_sensor_cfg = EZB_ZHA_LIGHT_SENSOR_CONFIG();
  _ep_desc = ezb_zha_create_light_sensor(endpoint, &light_sensor_cfg);
  _ep_config = {.ep_id = endpoint, .app_profile_id = EZB_AF_HA_PROFILE_ID, .app_device_id = EZB_ZHA_LIGHT_SENSOR_DEVICE_ID, .app_device_version = 0};
  log_v("Illuminance sensor endpoint created %u", _endpoint);
}

bool ZigbeeIlluminanceSensor::setDefaultValue(uint16_t defaultValue) {
  ezb_zcl_cluster_desc_t light_measure_cluster = ezb_af_endpoint_get_cluster_desc(_ep_desc, EZB_ZCL_CLUSTER_ID_ILLUMINANCE_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER);
  ezb_err_t ret = ezb_zcl_illuminance_measurement_cluster_desc_add_attr(light_measure_cluster, EZB_ZCL_ATTR_ILLUMINANCE_MEASUREMENT_MEASURED_VALUE_ID, (void *)&defaultValue);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to set default value: 0x%x", ret);
    return false;
  }
  return true;
}

bool ZigbeeIlluminanceSensor::setMinMaxValue(uint16_t min, uint16_t max) {
  ezb_zcl_cluster_desc_t light_measure_cluster = ezb_af_endpoint_get_cluster_desc(_ep_desc, EZB_ZCL_CLUSTER_ID_ILLUMINANCE_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER);
  ezb_err_t ret = ezb_zcl_illuminance_measurement_cluster_desc_add_attr(light_measure_cluster, EZB_ZCL_ATTR_ILLUMINANCE_MEASUREMENT_MIN_MEASURED_VALUE_ID, (void *)&min);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to set min value: 0x%x", ret);
    return false;
  }
  ret = ezb_zcl_illuminance_measurement_cluster_desc_add_attr(light_measure_cluster, EZB_ZCL_ATTR_ILLUMINANCE_MEASUREMENT_MAX_MEASURED_VALUE_ID, (void *)&max);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to set max value: 0x%x", ret);
    return false;
  }
  return true;
}

bool ZigbeeIlluminanceSensor::setTolerance(uint16_t tolerance) {
  ezb_zcl_cluster_desc_t light_measure_cluster = ezb_af_endpoint_get_cluster_desc(_ep_desc, EZB_ZCL_CLUSTER_ID_ILLUMINANCE_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER);
  ezb_err_t ret = ezb_zcl_illuminance_measurement_cluster_desc_add_attr(light_measure_cluster, EZB_ZCL_ATTR_ILLUMINANCE_MEASUREMENT_TOLERANCE_ID, (void *)&tolerance);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to set tolerance: 0x%x", ret);
    return false;
  }
  return true;
}

bool ZigbeeIlluminanceSensor::setReporting(uint16_t min_interval, uint16_t max_interval, uint16_t delta) {
  // NOTE(zb-v2): Reporting is now handle-based. The reporting record is created by the stack when the
  // endpoint is registered, so this must be called after Zigbee.begin(). We look up the handle, tune the
  // intervals/reportable change, then start the report.
  ezb_zcl_reporting_info_t reporting_info = ezb_zcl_reporting_info_find(
    _endpoint, EZB_ZCL_CLUSTER_ID_ILLUMINANCE_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_ILLUMINANCE_MEASUREMENT_MEASURED_VALUE_ID, EZB_ZCL_STD_MANUF_CODE
  );
  if (reporting_info == EZB_ZCL_INVALID_REPORTING_INFO) {
    log_e("Failed to find reporting info for illuminance measurement");
    return false;
  }
  ezb_zcl_attr_variable_t delta_var = {};
  delta_var.u16 = delta;
  ezb_zcl_reporting_info_update(reporting_info, min_interval, max_interval, &delta_var);
  ezb_zcl_reporting_info_update_default_interval(reporting_info, min_interval, max_interval);
  return setClusterReporting(reporting_info);
}

bool ZigbeeIlluminanceSensor::setIlluminance(uint16_t illuminanceValue) {
  log_v("Updating Illuminance...");
  log_d("Setting Illuminance to %u", illuminanceValue);
  ezb_zcl_status_t ret = setClusterAttribute(
    EZB_ZCL_CLUSTER_ID_ILLUMINANCE_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_ILLUMINANCE_MEASUREMENT_MEASURED_VALUE_ID, &illuminanceValue, false
  );
  if (ret != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set illuminance: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeIlluminanceSensor::report() {
  /* Send report attributes command */
  ezb_zcl_report_attr_cmd_t report_attr_cmd;
  memset(&report_attr_cmd, 0, sizeof(report_attr_cmd));
  // No explicit destination: report to bound devices (replaces v1 ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT).
  ezb_address_set_none(&report_attr_cmd.cmd_ctrl.dst_addr);
  report_attr_cmd.cmd_ctrl.src_ep = _endpoint;
  report_attr_cmd.cmd_ctrl.cluster_id = EZB_ZCL_CLUSTER_ID_ILLUMINANCE_MEASUREMENT;
  report_attr_cmd.cmd_ctrl.manuf_code = EZB_ZCL_STD_MANUF_CODE;
  report_attr_cmd.cmd_ctrl.fc.direction = EZB_ZCL_CMD_DIRECTION_TO_CLI;
  report_attr_cmd.payload.attr_id = EZB_ZCL_ATTR_ILLUMINANCE_MEASUREMENT_MEASURED_VALUE_ID;

  if (!reportClusterAttribute(&report_attr_cmd)) {
    log_e("Failed to send illuminance report");
    return false;
  }
  log_v("Illuminance report sent");
  return true;
}

#endif  // CONFIG_ZB_ENABLED
