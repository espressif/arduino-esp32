#include "ZigbeeFlowSensor.h"
#if SOC_IEEE802154_SUPPORTED && CONFIG_ZB_ENABLED

esp_zb_cluster_list_t *zigbee_flow_sensor_clusters_create(zigbee_flow_sensor_cfg_t *flow_sensor) {
  esp_zb_basic_cluster_cfg_t *basic_cfg = flow_sensor ? &(flow_sensor->basic_cfg) : NULL;
  esp_zb_identify_cluster_cfg_t *identify_cfg = flow_sensor ? &(flow_sensor->identify_cfg) : NULL;
  esp_zb_flow_meas_cluster_cfg_t *flow_meas_cfg = flow_sensor ? &(flow_sensor->flow_meas_cfg) : NULL;
  esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
  esp_zb_cluster_list_add_basic_cluster(cluster_list, esp_zb_basic_cluster_create(basic_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_identify_cluster(cluster_list, esp_zb_identify_cluster_create(identify_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_flow_meas_cluster(cluster_list, esp_zb_flow_meas_cluster_create(flow_meas_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_identify_cluster(cluster_list, esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  return cluster_list;
}

ZigbeeFlowSensor::ZigbeeFlowSensor(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID;

  //Create custom pressure sensor configuration
  zigbee_flow_sensor_cfg_t flow_sensor_cfg = ZIGBEE_DEFAULT_FLOW_SENSOR_CONFIG();
  _cluster_list = zigbee_flow_sensor_clusters_create(&flow_sensor_cfg);

  _ep_config = {.endpoint = _endpoint, .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID, .app_device_id = ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID, .app_device_version = 0};
}

void ZigbeeFlowSensor::setMinMaxValue(float min, float max) {
  uint16_t zb_min = (uint16_t)(min * 10);
  uint16_t zb_max = (uint16_t)(max * 10);
  esp_zb_attribute_list_t *flow_measure_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_FLOW_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_update_attr(flow_measure_cluster, ESP_ZB_ZCL_ATTR_FLOW_MEASUREMENT_MIN_VALUE_ID, (void *)&zb_min);
  esp_zb_cluster_update_attr(flow_measure_cluster, ESP_ZB_ZCL_ATTR_FLOW_MEASUREMENT_MAX_VALUE_ID, (void *)&zb_max);
}

void ZigbeeFlowSensor::setTolerance(float tolerance) {
  // Convert tolerance to ZCL uint16_t
  uint16_t zb_tolerance = (uint16_t)(tolerance * 10);
  esp_zb_attribute_list_t *flow_measure_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_FLOW_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_flow_meas_cluster_add_attr(flow_measure_cluster, ESP_ZB_ZCL_ATTR_FLOW_MEASUREMENT_TOLERANCE_ID, (void *)&zb_tolerance);
}

void ZigbeeFlowSensor::setReporting(uint16_t min_interval, uint16_t max_interval, float delta) {
  // clang-format off
  esp_zb_zcl_reporting_info_t reporting_info = {
    .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV,
    .ep = _endpoint,
    .cluster_id = ESP_ZB_ZCL_CLUSTER_ID_FLOW_MEASUREMENT,
    .cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
    .attr_id = ESP_ZB_ZCL_ATTR_FLOW_MEASUREMENT_VALUE_ID,
    .u =
      {
        .send_info =
          {
            .min_interval = min_interval,
            .max_interval = max_interval,
            .delta =
              {
                .u16 = (uint16_t)(delta * 10),  // Convert delta to ZCL uint16_t
              },
            .def_min_interval = min_interval,
            .def_max_interval = max_interval,
          },
      },
    .dst =
      {
        .profile_id = ESP_ZB_AF_HA_PROFILE_ID,
      },
    .manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC,
  };
  // clang-format on
  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_update_reporting_info(&reporting_info);
  esp_zb_lock_release();
}

void ZigbeeFlowSensor::setFlow(float flow) {
  uint16_t zb_flow = (uint16_t)(flow * 10);
  log_v("Updating flow sensor value...");
  /* Update temperature sensor measured value */
  log_d("Setting flow to %d", zb_flow);
  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_FLOW_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_FLOW_MEASUREMENT_VALUE_ID, &zb_flow, false
  );
  esp_zb_lock_release();
}

void ZigbeeFlowSensor::report() {
  /* Send report attributes command */
  esp_zb_zcl_report_attr_cmd_t report_attr_cmd;
  report_attr_cmd.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
  report_attr_cmd.attributeID = ESP_ZB_ZCL_ATTR_FLOW_MEASUREMENT_VALUE_ID;
  report_attr_cmd.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI;
  report_attr_cmd.clusterID = ESP_ZB_ZCL_CLUSTER_ID_FLOW_MEASUREMENT;
  report_attr_cmd.zcl_basic_cmd.src_endpoint = _endpoint;

  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_report_attr_cmd_req(&report_attr_cmd);
  esp_zb_lock_release();
  log_v("Flow report sent");
}

#endif  //SOC_IEEE802154_SUPPORTED && CONFIG_ZB_ENABLED
