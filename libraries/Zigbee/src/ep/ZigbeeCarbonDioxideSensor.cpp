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

#include "ZigbeeCarbonDioxideSensor.h"
#if CONFIG_ZB_ENABLED

// v2.x data model: build the endpoint descriptor manually (no dedicated ZHA carbon-dioxide-sensor template).
// Basic + Identify + CarbonDioxideMeasurement server clusters, registered as a Simple Sensor device.
ezb_af_ep_desc_t zigbee_carbon_dioxide_sensor_create_ep_desc(uint8_t endpoint, zigbee_carbon_dioxide_sensor_cfg_t *carbon_dioxide_sensor) {
  ezb_af_ep_config_t ep_config = {
    .ep_id = endpoint, .app_profile_id = EZB_AF_HA_PROFILE_ID, .app_device_id = EZB_ZHA_SIMPLE_SENSOR_DEVICE_ID, .app_device_version = 0
  };
  ezb_af_ep_desc_t ep_desc = ezb_af_create_endpoint_desc(&ep_config);
  if (ep_desc == nullptr) {
    log_e("Failed to create carbon dioxide sensor endpoint descriptor");
    return nullptr;
  }

  const void *basic_cfg = carbon_dioxide_sensor ? &(carbon_dioxide_sensor->basic_cfg) : nullptr;
  const void *identify_cfg = carbon_dioxide_sensor ? &(carbon_dioxide_sensor->identify_cfg) : nullptr;
  const void *carbon_dioxide_meas_cfg = carbon_dioxide_sensor ? &(carbon_dioxide_sensor->carbon_dioxide_meas_cfg) : nullptr;

  ezb_af_endpoint_add_cluster_desc(ep_desc, ezb_zcl_basic_create_cluster_desc(basic_cfg, EZB_ZCL_CLUSTER_SERVER));
  ezb_af_endpoint_add_cluster_desc(ep_desc, ezb_zcl_identify_create_cluster_desc(identify_cfg, EZB_ZCL_CLUSTER_SERVER));
  ezb_af_endpoint_add_cluster_desc(ep_desc, ezb_zcl_carbon_dioxide_measurement_create_cluster_desc(carbon_dioxide_meas_cfg, EZB_ZCL_CLUSTER_SERVER));
  return ep_desc;
}

ZigbeeCarbonDioxideSensor::ZigbeeCarbonDioxideSensor(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = EZB_ZHA_SIMPLE_SENSOR_DEVICE_ID;

  //Create custom carbon dioxide sensor configuration
  zigbee_carbon_dioxide_sensor_cfg_t carbon_dioxide_sensor_cfg = ZIGBEE_DEFAULT_CARBON_DIOXIDE_SENSOR_CONFIG();
  _ep_desc = zigbee_carbon_dioxide_sensor_create_ep_desc(_endpoint, &carbon_dioxide_sensor_cfg);

  _ep_config = {.ep_id = _endpoint, .app_profile_id = EZB_AF_HA_PROFILE_ID, .app_device_id = EZB_ZHA_SIMPLE_SENSOR_DEVICE_ID, .app_device_version = 0};
}

bool ZigbeeCarbonDioxideSensor::setDefaultValue(float defaultValue) {
  float zb_default_value = defaultValue / 1000000.0f;
  ezb_zcl_cluster_desc_t carbon_dioxide_measure_cluster =
    ezb_af_endpoint_get_cluster_desc(_ep_desc, EZB_ZCL_CLUSTER_ID_CARBON_DIOXIDE_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER);
  ezb_err_t ret = ezb_zcl_carbon_dioxide_measurement_cluster_desc_add_attr(
    carbon_dioxide_measure_cluster, EZB_ZCL_ATTR_CARBON_DIOXIDE_MEASUREMENT_MEASURED_VALUE_ID, (void *)&zb_default_value
  );
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to set default value: 0x%x", ret);
    return false;
  }
  return true;
}

bool ZigbeeCarbonDioxideSensor::setMinMaxValue(float min, float max) {
  float zb_min = min / 1000000.0f;
  float zb_max = max / 1000000.0f;
  ezb_zcl_cluster_desc_t carbon_dioxide_measure_cluster =
    ezb_af_endpoint_get_cluster_desc(_ep_desc, EZB_ZCL_CLUSTER_ID_CARBON_DIOXIDE_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER);
  ezb_err_t ret =
    ezb_zcl_carbon_dioxide_measurement_cluster_desc_add_attr(carbon_dioxide_measure_cluster, EZB_ZCL_ATTR_CARBON_DIOXIDE_MEASUREMENT_MIN_MEASURED_VALUE_ID, (void *)&zb_min);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to set min value: 0x%x", ret);
    return false;
  }
  ret =
    ezb_zcl_carbon_dioxide_measurement_cluster_desc_add_attr(carbon_dioxide_measure_cluster, EZB_ZCL_ATTR_CARBON_DIOXIDE_MEASUREMENT_MAX_MEASURED_VALUE_ID, (void *)&zb_max);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to set max value: 0x%x", ret);
    return false;
  }
  return true;
}

bool ZigbeeCarbonDioxideSensor::setTolerance(float tolerance) {
  float zb_tolerance = tolerance / 1000000.0f;
  ezb_zcl_cluster_desc_t carbon_dioxide_measure_cluster =
    ezb_af_endpoint_get_cluster_desc(_ep_desc, EZB_ZCL_CLUSTER_ID_CARBON_DIOXIDE_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER);
  ezb_err_t ret =
    ezb_zcl_carbon_dioxide_measurement_cluster_desc_add_attr(carbon_dioxide_measure_cluster, EZB_ZCL_ATTR_CARBON_DIOXIDE_MEASUREMENT_TOLERANCE_ID, (void *)&zb_tolerance);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to set tolerance: 0x%x", ret);
    return false;
  }
  return true;
}

bool ZigbeeCarbonDioxideSensor::setReporting(uint16_t min_interval, uint16_t max_interval, uint16_t delta) {
  // NOTE(zb-v2): Reporting is now handle-based. The reporting record is created by the stack when the
  // endpoint is registered, so this must be called after Zigbee.begin(). We look up the handle, tune the
  // intervals/reportable change, then start the report.
  ezb_zcl_reporting_info_t reporting_info = ezb_zcl_reporting_info_find(
    _endpoint, EZB_ZCL_CLUSTER_ID_CARBON_DIOXIDE_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_CARBON_DIOXIDE_MEASUREMENT_MEASURED_VALUE_ID, EZB_ZCL_STD_MANUF_CODE
  );
  if (reporting_info == EZB_ZCL_INVALID_REPORTING_INFO) {
    log_e("Failed to find reporting info for carbon dioxide measurement");
    return false;
  }
  ezb_zcl_attr_variable_t delta_var = {};
  delta_var.f32 = delta / 1000000.0f;  // measured value is a float fraction
  ezb_zcl_reporting_info_update(reporting_info, min_interval, max_interval, &delta_var);
  ezb_zcl_reporting_info_update_default_interval(reporting_info, min_interval, max_interval);
  return setClusterReporting(reporting_info);
}

bool ZigbeeCarbonDioxideSensor::setCarbonDioxide(float carbon_dioxide) {
  float zb_carbon_dioxide = carbon_dioxide / 1000000.0f;
  log_v("Updating carbon dioxide sensor value...");
  log_d("Setting carbon dioxide to %0.1f", carbon_dioxide);
  ezb_zcl_status_t ret = setClusterAttribute(
    EZB_ZCL_CLUSTER_ID_CARBON_DIOXIDE_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_CARBON_DIOXIDE_MEASUREMENT_MEASURED_VALUE_ID, &zb_carbon_dioxide, false
  );
  if (ret != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set carbon dioxide: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeCarbonDioxideSensor::report() {
  /* Send report attributes command */
  ezb_zcl_report_attr_cmd_t report_attr_cmd;
  memset(&report_attr_cmd, 0, sizeof(report_attr_cmd));
  // No explicit destination: report to bound devices (replaces v1 ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT).
  ezb_address_set_none(&report_attr_cmd.cmd_ctrl.dst_addr);
  report_attr_cmd.cmd_ctrl.src_ep = _endpoint;
  report_attr_cmd.cmd_ctrl.cluster_id = EZB_ZCL_CLUSTER_ID_CARBON_DIOXIDE_MEASUREMENT;
  report_attr_cmd.cmd_ctrl.manuf_code = EZB_ZCL_STD_MANUF_CODE;
  report_attr_cmd.cmd_ctrl.fc.direction = EZB_ZCL_CMD_DIRECTION_TO_CLI;
  report_attr_cmd.payload.attr_id = EZB_ZCL_ATTR_CARBON_DIOXIDE_MEASUREMENT_MEASURED_VALUE_ID;

  if (!reportClusterAttribute(&report_attr_cmd)) {
    log_e("Failed to send carbon dioxide report");
    return false;
  }
  log_v("Carbon dioxide report sent");
  return true;
}

#endif  // CONFIG_ZB_ENABLED
