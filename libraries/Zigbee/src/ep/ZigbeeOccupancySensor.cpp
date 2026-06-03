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

// v2.x data model: build the endpoint descriptor manually (no dedicated ZHA occupancy-sensor template).
// Basic + Identify + OccupancySensing server clusters, registered as a Simple Sensor device.
ezb_af_ep_desc_t zigbee_occupancy_sensor_create_ep_desc(uint8_t endpoint, zigbee_occupancy_sensor_cfg_t *occupancy_sensor) {
  ezb_af_ep_config_t ep_config = {
    .ep_id = endpoint, .app_profile_id = EZB_AF_HA_PROFILE_ID, .app_device_id = EZB_ZHA_SIMPLE_SENSOR_DEVICE_ID, .app_device_version = 0
  };
  ezb_af_ep_desc_t ep_desc = ezb_af_create_endpoint_desc(&ep_config);
  if (ep_desc == nullptr) {
    log_e("Failed to create occupancy sensor endpoint descriptor");
    return nullptr;
  }

  const void *basic_cfg = occupancy_sensor ? &(occupancy_sensor->basic_cfg) : nullptr;
  const void *identify_cfg = occupancy_sensor ? &(occupancy_sensor->identify_cfg) : nullptr;
  const void *occupancy_meas_cfg = occupancy_sensor ? &(occupancy_sensor->occupancy_meas_cfg) : nullptr;

  ezb_af_endpoint_add_cluster_desc(ep_desc, ezb_zcl_basic_create_cluster_desc(basic_cfg, EZB_ZCL_CLUSTER_SERVER));
  ezb_af_endpoint_add_cluster_desc(ep_desc, ezb_zcl_identify_create_cluster_desc(identify_cfg, EZB_ZCL_CLUSTER_SERVER));
  ezb_af_endpoint_add_cluster_desc(ep_desc, ezb_zcl_occupancy_sensing_create_cluster_desc(occupancy_meas_cfg, EZB_ZCL_CLUSTER_SERVER));
  return ep_desc;
}

ZigbeeOccupancySensor::ZigbeeOccupancySensor(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = EZB_ZHA_SIMPLE_SENSOR_DEVICE_ID;

  //Create custom occupancy sensor configuration
  zigbee_occupancy_sensor_cfg_t occupancy_sensor_cfg = ZIGBEE_DEFAULT_OCCUPANCY_SENSOR_CONFIG();
  _ep_desc = zigbee_occupancy_sensor_create_ep_desc(_endpoint, &occupancy_sensor_cfg);

  _ep_config = {.ep_id = _endpoint, .app_profile_id = EZB_AF_HA_PROFILE_ID, .app_device_id = EZB_ZHA_SIMPLE_SENSOR_DEVICE_ID, .app_device_version = 0};
}

bool ZigbeeOccupancySensor::setSensorType(uint8_t sensor_type) {
  uint8_t sensor_type_bitmap = 1 << sensor_type;
  ezb_zcl_cluster_desc_t occupancy_sens_cluster = ezb_af_endpoint_get_cluster_desc(_ep_desc, EZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING, EZB_ZCL_CLUSTER_SERVER);
  ezb_err_t ret = ezb_zcl_occupancy_sensing_cluster_desc_add_attr(occupancy_sens_cluster, EZB_ZCL_ATTR_OCCUPANCY_SENSING_OCCUPANCY_SENSOR_TYPE_ID, (void *)&sensor_type);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to set sensor type: 0x%x", ret);
    return false;
  }
  ret =
    ezb_zcl_occupancy_sensing_cluster_desc_add_attr(occupancy_sens_cluster, EZB_ZCL_ATTR_OCCUPANCY_SENSING_OCCUPANCY_SENSOR_TYPE_BITMAP_ID, (void *)&sensor_type_bitmap);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to set sensor type bitmap: 0x%x", ret);
    return false;
  }
  return true;
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
