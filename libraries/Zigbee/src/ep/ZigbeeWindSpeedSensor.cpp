#include "ZigbeeWindSpeedSensor.h"
#if SOC_IEEE802154_SUPPORTED

// CLUSTER_FN_ENTRY(wind_speed_measurement, ESP_ZB_ZCL_CLUSTER_ID_WIND_SPEED_MEASUREMENT),

ZigbeeWindSpeedSensor::ZigbeeWindSpeedSensor(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = ESP_ZB_HA_TEMPERATURE_SENSOR_DEVICE_ID;

  esp_zb_windspeed_sensor_cfg_t windspeed_sensor_cfg = ESP_ZB_DEFAULT_TEMPERATURE_SENSOR_CONFIG();
  _cluster_list = esp_zb_windspeed_sensor_clusters_create(&windspeed_sensor_cfg);

  _ep_config = {
    .endpoint = _endpoint, .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID, .app_device_id = ESP_ZB_HA_TEMPERATURE_SENSOR_DEVICE_ID, .app_device_version = 0
  };
}

static int16_t zb_windspeed_to_s16(float windspeed) {
  return (int16_t)(windspeed * 100);
}

/** @brief Wind_Speed_Measurement cluster server attribute identifiers
typedef enum {
    ESP_ZB_ZCL_ATTR_WIND_SPEED_MEASUREMENT_MEASURED_VALUE_ID     = 0x0000, < MeasuredValue Attribute
    ESP_ZB_ZCL_ATTR_WIND_SPEED_MEASUREMENT_MIN_MEASURED_VALUE_ID = 0x0001, < MinMeasuredValue Attribute
    ESP_ZB_ZCL_ATTR_WIND_SPEED_MEASUREMENT_MAX_MEASURED_VALUE_ID = 0x0002, < MaxMeasuredValue Attribute 
    ESP_ZB_ZCL_ATTR_WIND_SPEED_MEASUREMENT_TOLERANCE_ID          = 0x0003, < Tolerance Attribute 
} esp_zb_zcl_wind_speed_measurement_srv_attr_t;
*/
void ZigbeeWindSpeedSensor::setMinMaxValue(float min, float max) {
  int16_t zb_min = zb_windspeed_to_s16(min);
  int16_t zb_max = zb_windspeed_to_s16(max);
  esp_zb_attribute_list_t *windspeed_measure_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_WIND_SPEED_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  // 
  esp_zb_cluster_update_attr(windspeed_measure_cluster, ESP_ZB_ZCL_ATTR_WIND_SPEED_MEASUREMENT_MIN_MEASURED_VALUE_ID, (void *)&zb_min);
  esp_zb_cluster_update_attr(windspeed_measure_cluster, ESP_ZB_ZCL_ATTR_WIND_SPEED_MEASUREMENT_MAX_MEASURED_VALUE_ID, (void *)&zb_max);
}

void ZigbeeWindSpeedSensor::setTolerance(float tolerance) {
  // Convert tolerance to ZCL uint16_t
  uint16_t zb_tolerance = (uint16_t)(tolerance * 100);
  esp_zb_attribute_list_t *windspeed_measure_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_WIND_SPEED_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_windspeed_meas_cluster_add_attr(windspeed_measure_cluster, ESP_ZB_ZCL_ATTR_WIND_SPEED_MEASUREMENT_TOLERANCE_ID, (void *)&zb_tolerance);
}

void ZigbeeWindSpeedSensor::setReporting(uint16_t min_interval, uint16_t max_interval, float delta) {
  esp_zb_zcl_reporting_info_t reporting_info = {
    .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV,
    .ep = _endpoint,
    .cluster_id = ESP_ZB_ZCL_CLUSTER_ID_WIND_SPEED_MEASUREMENT,
    .cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
    .attr_id = ESP_ZB_ZCL_ATTR_WIND_SPEED_MEASUREMENT_MEASURED_VALUE_ID,
    .u =
      {
        .send_info =
          {
            .min_interval = min_interval,
            .max_interval = max_interval,
            .delta =
              {
                .u16 = (uint16_t)(delta * 100),  // Convert delta to ZCL uint16_t
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
  esp_zb_zcl_update_reporting_info(&reporting_info);
}

void ZigbeeWindSpeedSensor::setWindspeed(float windspeed) {
  int16_t zb_windspeed = zb_windspeed_to_s16(windspeed);
  log_v("Updating windspeed sensor value...");
  /* Update windspeed sensor measured value */
  log_d("Setting windspeed to %d", zb_windspeed);
  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_WIND_SPEED_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_WIND_SPEED_MEASUREMENT_MEASURED_VALUE_ID, &zb_windspeed, false
  );
  esp_zb_lock_release();
}

void ZigbeeWindSpeedSensor::reportWindspeed() {
  /* Send report attributes command */
  esp_zb_zcl_report_attr_cmd_t report_attr_cmd;
  report_attr_cmd.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
  report_attr_cmd.attributeID = ESP_ZB_ZCL_ATTR_WIND_SPEED_MEASUREMENT_MEASURED_VALUE_ID;
  report_attr_cmd.cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE;
  report_attr_cmd.clusterID = ESP_ZB_ZCL_CLUSTER_ID_WIND_SPEED_MEASUREMENT;
  report_attr_cmd.zcl_basic_cmd.src_endpoint = _endpoint;

  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_report_attr_cmd_req(&report_attr_cmd);
  esp_zb_lock_release();
  log_v("Temperature report sent");
}

#endif  //SOC_IEEE802154_SUPPORTED
