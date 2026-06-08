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

#include "ZigbeeOccupancySensor.h"
#if CONFIG_ZB_ENABLED
#include "ezbee/zha.h"


ZigbeeOccupancySensor::ZigbeeOccupancySensor(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = EZB_ZHA_SIMPLE_SENSOR_DEVICE_ID;
  _occupancy_meas_cfg = {
    .occupancy = EZB_ZCL_OCCUPANCY_SENSING_OCCUPANCY_UNOCCUPIED,
    .occupancy_sensor_type = EZB_ZCL_OCCUPANCY_SENSING_OCCUPANCY_SENSOR_TYPE_PIR,
    .occupancy_sensor_type_bitmap = (1 << EZB_ZCL_OCCUPANCY_SENSING_OCCUPANCY_SENSOR_TYPE_PIR),
  };

  _ep_config = {.ep_id = _endpoint, .app_profile_id = EZB_AF_HA_PROFILE_ID, .app_device_id = EZB_ZHA_SIMPLE_SENSOR_DEVICE_ID, .app_device_version = 0};
  _ep_desc = ezb_af_create_endpoint_desc(&_ep_config);
  if (_ep_desc == nullptr) {
    log_e("Failed to create occupancy sensor endpoint descriptor");
    return;
  }
  ezb_af_endpoint_add_cluster_desc(_ep_desc, ezb_zcl_basic_create_cluster_desc(nullptr, EZB_ZCL_CLUSTER_SERVER));
  ezb_af_endpoint_add_cluster_desc(_ep_desc, ezb_zcl_identify_create_cluster_desc(nullptr, EZB_ZCL_CLUSTER_SERVER));
  ezb_af_endpoint_add_cluster_desc(_ep_desc, ezb_zcl_occupancy_sensing_create_cluster_desc(&_occupancy_meas_cfg, EZB_ZCL_CLUSTER_SERVER));
}

bool ZigbeeOccupancySensor::setSensorType(uint8_t sensor_type) {
  _occupancy_meas_cfg.occupancy_sensor_type = sensor_type;
  _occupancy_meas_cfg.occupancy_sensor_type_bitmap = (1 << sensor_type);
  if (!configureEpClusterAttr(
        "setSensorType", EZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_OCCUPANCY_SENSING_OCCUPANCY_SENSOR_TYPE_ID,
        &_occupancy_meas_cfg.occupancy_sensor_type, ezb_zcl_occupancy_sensing_cluster_desc_add_attr
      )) {
    return false;
  }
  return configureEpClusterAttr(
    "setSensorType", EZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_OCCUPANCY_SENSING_OCCUPANCY_SENSOR_TYPE_BITMAP_ID,
    &_occupancy_meas_cfg.occupancy_sensor_type_bitmap, ezb_zcl_occupancy_sensing_cluster_desc_add_attr
  );
}

bool ZigbeeOccupancySensor::setOccupancy(bool occupied) {
  log_v("Updating occupancy sensor value...");
  /* Update occupancy sensor value */
  log_d("Setting occupancy to %d", occupied);
  ezb_zcl_status_t ret =
    setClusterAttribute(EZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_OCCUPANCY_SENSING_OCCUPANCY_ID, &occupied, false);
  if (ret != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set occupancy: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeOccupancySensor::report() {
  /* Send report attributes command */
  ezb_zcl_report_attr_cmd_t report_attr_cmd;
  memset(&report_attr_cmd, 0, sizeof(report_attr_cmd));
  // No explicit destination: report to bound devices (replaces v1 ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT).
  ezb_address_set_none(&report_attr_cmd.cmd_ctrl.dst_addr);
  report_attr_cmd.cmd_ctrl.src_ep = _endpoint;
  report_attr_cmd.cmd_ctrl.cluster_id = EZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING;
  report_attr_cmd.cmd_ctrl.manuf_code = EZB_ZCL_STD_MANUF_CODE;
  report_attr_cmd.cmd_ctrl.fc.direction = EZB_ZCL_CMD_DIRECTION_TO_CLI;
  report_attr_cmd.payload.attr_id = EZB_ZCL_ATTR_OCCUPANCY_SENSING_OCCUPANCY_ID;

  if (!reportClusterAttribute(&report_attr_cmd)) {
    log_e("Failed to send occupancy report");
    return false;
  }
  log_v("Occupancy report sent");
  return true;
}

#endif  // CONFIG_ZB_ENABLED
