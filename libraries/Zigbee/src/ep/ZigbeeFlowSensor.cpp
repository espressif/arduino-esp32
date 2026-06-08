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

#include "ZigbeeFlowSensor.h"
#if CONFIG_ZB_ENABLED
#include "ezbee/zha.h"


ZigbeeFlowSensor::ZigbeeFlowSensor(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = EZB_ZHA_SIMPLE_SENSOR_DEVICE_ID;
  _flow_meas_cfg = {
    .measured_value = 0,
    .min_measured_value = 0,
    .max_measured_value = 0x7FFF,
  };
  _tolerance = 0;

  _ep_config = {.ep_id = _endpoint, .app_profile_id = EZB_AF_HA_PROFILE_ID, .app_device_id = EZB_ZHA_SIMPLE_SENSOR_DEVICE_ID, .app_device_version = 0};
  _ep_desc = ezb_af_create_endpoint_desc(&_ep_config);
  if (_ep_desc == nullptr) {
    log_e("Failed to create flow sensor endpoint descriptor");
    return;
  }
  ezb_af_endpoint_add_cluster_desc(_ep_desc, ezb_zcl_basic_create_cluster_desc(nullptr, EZB_ZCL_CLUSTER_SERVER));
  ezb_af_endpoint_add_cluster_desc(_ep_desc, ezb_zcl_identify_create_cluster_desc(nullptr, EZB_ZCL_CLUSTER_SERVER));
  ezb_af_endpoint_add_cluster_desc(_ep_desc, ezb_zcl_flow_measurement_create_cluster_desc(&_flow_meas_cfg, EZB_ZCL_CLUSTER_SERVER));
}

bool ZigbeeFlowSensor::setDefaultValue(float defaultValue) {
  _flow_meas_cfg.measured_value = (uint16_t)(defaultValue * 10);
  return configureEpClusterAttr(
    "setDefaultValue", EZB_ZCL_CLUSTER_ID_FLOW_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_FLOW_MEASUREMENT_MEASURED_VALUE_ID,
    &_flow_meas_cfg.measured_value, ezb_zcl_flow_measurement_cluster_desc_add_attr
  );
}

bool ZigbeeFlowSensor::setMinMaxValue(float min, float max) {
  _flow_meas_cfg.min_measured_value = (uint16_t)(min * 10);
  _flow_meas_cfg.max_measured_value = (uint16_t)(max * 10);
  if (!configureEpClusterAttr(
        "setMinMaxValue", EZB_ZCL_CLUSTER_ID_FLOW_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_FLOW_MEASUREMENT_MIN_MEASURED_VALUE_ID,
        &_flow_meas_cfg.min_measured_value, ezb_zcl_flow_measurement_cluster_desc_add_attr
      )) {
    return false;
  }
  return configureEpClusterAttr(
    "setMinMaxValue", EZB_ZCL_CLUSTER_ID_FLOW_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_FLOW_MEASUREMENT_MAX_MEASURED_VALUE_ID,
    &_flow_meas_cfg.max_measured_value, ezb_zcl_flow_measurement_cluster_desc_add_attr
  );
}

bool ZigbeeFlowSensor::setTolerance(float tolerance) {
  _tolerance = (uint16_t)(tolerance * 10);
  return configureEpClusterAttr(
    "setTolerance", EZB_ZCL_CLUSTER_ID_FLOW_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_FLOW_MEASUREMENT_TOLERANCE_ID, &_tolerance,
    ezb_zcl_flow_measurement_cluster_desc_add_attr
  );
}

bool ZigbeeFlowSensor::setReporting(uint16_t min_interval, uint16_t max_interval, float delta) {
  // NOTE(zb-v2): Reporting is now handle-based. The reporting record is created by the stack when the
  // endpoint is registered, so this must be called after Zigbee.begin(). We look up the handle, tune the
  // intervals/reportable change, then start the report.
  ezb_zcl_reporting_info_t reporting_info = ezb_zcl_reporting_info_find(
    _endpoint, EZB_ZCL_CLUSTER_ID_FLOW_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_FLOW_MEASUREMENT_MEASURED_VALUE_ID, EZB_ZCL_STD_MANUF_CODE
  );
  if (reporting_info == EZB_ZCL_INVALID_REPORTING_INFO) {
    log_e("Failed to find reporting info for flow measurement");
    return false;
  }
  ezb_zcl_attr_variable_t delta_var = {};
  delta_var.u16 = (uint16_t)(delta * 10);  // Convert delta to ZCL uint16_t
  ezb_zcl_reporting_info_update(reporting_info, min_interval, max_interval, &delta_var);
  ezb_zcl_reporting_info_update_default_interval(reporting_info, min_interval, max_interval);
  return setClusterReporting(reporting_info);
}

bool ZigbeeFlowSensor::setFlow(float flow) {
  uint16_t zb_flow = (uint16_t)(flow * 10);
  log_v("Updating flow sensor value...");
  log_d("Setting flow to %u", zb_flow);
  ezb_zcl_status_t ret =
    setClusterAttribute(EZB_ZCL_CLUSTER_ID_FLOW_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_FLOW_MEASUREMENT_MEASURED_VALUE_ID, &zb_flow, false);
  if (ret != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set flow value: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeFlowSensor::report() {
  /* Send report attributes command */
  ezb_zcl_report_attr_cmd_t report_attr_cmd;
  memset(&report_attr_cmd, 0, sizeof(report_attr_cmd));
  // No explicit destination: report to bound devices (replaces v1 ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT).
  ezb_address_set_none(&report_attr_cmd.cmd_ctrl.dst_addr);
  report_attr_cmd.cmd_ctrl.src_ep = _endpoint;
  report_attr_cmd.cmd_ctrl.cluster_id = EZB_ZCL_CLUSTER_ID_FLOW_MEASUREMENT;
  report_attr_cmd.cmd_ctrl.manuf_code = EZB_ZCL_STD_MANUF_CODE;
  report_attr_cmd.cmd_ctrl.fc.direction = EZB_ZCL_CMD_DIRECTION_TO_CLI;
  report_attr_cmd.payload.attr_id = EZB_ZCL_ATTR_FLOW_MEASUREMENT_MEASURED_VALUE_ID;

  if (!reportClusterAttribute(&report_attr_cmd)) {
    log_e("Failed to send flow report");
    return false;
  }
  log_v("Flow report sent");
  return true;
}

#endif  // CONFIG_ZB_ENABLED
