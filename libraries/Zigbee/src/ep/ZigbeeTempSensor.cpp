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

#include "ZigbeeTempSensor.h"
#if CONFIG_ZB_ENABLED

ZigbeeTempSensor::ZigbeeTempSensor(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = EZB_ZHA_TEMPERATURE_SENSOR_DEVICE_ID;
  _humidity_sensor = false;

  // v2.x data model: the ZHA template builds the full endpoint descriptor (basic, identify,
  // temperature_measurement clusters) instead of the v1 cluster-list factory.
  ezb_zha_temperature_sensor_config_t temp_sensor_cfg = EZB_ZHA_TEMPERATURE_SENSOR_CONFIG();
  _ep_desc = ezb_zha_create_temperature_sensor(_endpoint, &temp_sensor_cfg);

  // Set default (initial) value for the temperature sensor to 0.0°C
  setDefaultValue(0.0);

  _ep_config = {
    .ep_id = _endpoint, .app_profile_id = EZB_AF_HA_PROFILE_ID, .app_device_id = EZB_ZHA_TEMPERATURE_SENSOR_DEVICE_ID, .app_device_version = 0
  };
}

static int16_t zb_float_to_s16(float temp) {
  return (int16_t)(temp * 100);
}

bool ZigbeeTempSensor::setMinMaxValue(float min, float max) {
  int16_t zb_min = zb_float_to_s16(min);
  int16_t zb_max = zb_float_to_s16(max);
  // v2.x descriptor model: adding an attribute that already exists updates its value (replaces esp_zb_cluster_update_attr).
  ezb_zcl_cluster_desc_t temp_measure_cluster =
    ezb_af_endpoint_get_cluster_desc(_ep_desc, EZB_ZCL_CLUSTER_ID_TEMPERATURE_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER);
  ezb_err_t ret = ezb_zcl_temperature_measurement_cluster_desc_add_attr(temp_measure_cluster, EZB_ZCL_ATTR_TEMPERATURE_MEASUREMENT_MIN_MEASURED_VALUE_ID, (void *)&zb_min);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to set min value: 0x%x", ret);
    return false;
  }
  ret = ezb_zcl_temperature_measurement_cluster_desc_add_attr(temp_measure_cluster, EZB_ZCL_ATTR_TEMPERATURE_MEASUREMENT_MAX_MEASURED_VALUE_ID, (void *)&zb_max);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to set max value: 0x%x", ret);
    return false;
  }
  return true;
}

bool ZigbeeTempSensor::setDefaultValue(float defaultValue) {
  int16_t zb_default_value = zb_float_to_s16(defaultValue);
  ezb_zcl_cluster_desc_t temp_measure_cluster =
    ezb_af_endpoint_get_cluster_desc(_ep_desc, EZB_ZCL_CLUSTER_ID_TEMPERATURE_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER);
  ezb_err_t ret =
    ezb_zcl_temperature_measurement_cluster_desc_add_attr(temp_measure_cluster, EZB_ZCL_ATTR_TEMPERATURE_MEASUREMENT_MEASURED_VALUE_ID, (void *)&zb_default_value);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to set default value: 0x%x", ret);
    return false;
  }
  return true;
}

bool ZigbeeTempSensor::setTolerance(float tolerance) {
  // Convert tolerance to ZCL uint16_t
  uint16_t zb_tolerance = (uint16_t)(tolerance * 100);
  ezb_zcl_cluster_desc_t temp_measure_cluster =
    ezb_af_endpoint_get_cluster_desc(_ep_desc, EZB_ZCL_CLUSTER_ID_TEMPERATURE_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER);
  ezb_err_t ret = ezb_zcl_temperature_measurement_cluster_desc_add_attr(temp_measure_cluster, EZB_ZCL_ATTR_TEMPERATURE_MEASUREMENT_TOLERANCE_ID, (void *)&zb_tolerance);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to set tolerance: 0x%x", ret);
    return false;
  }
  return true;
}

bool ZigbeeTempSensor::setReporting(uint16_t min_interval, uint16_t max_interval, float delta) {
  // NOTE(zb-v2): Reporting is now handle-based. The reporting record is created by the stack when the
  // endpoint is registered, so this must be called after Zigbee.begin(). We look up the handle, tune the
  // intervals/reportable change, then start the report.
  ezb_zcl_reporting_info_t reporting_info = ezb_zcl_reporting_info_find(
    _endpoint, EZB_ZCL_CLUSTER_ID_TEMPERATURE_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_TEMPERATURE_MEASUREMENT_MEASURED_VALUE_ID, EZB_ZCL_STD_MANUF_CODE
  );
  if (reporting_info == EZB_ZCL_INVALID_REPORTING_INFO) {
    log_e("Failed to find reporting info for temperature measurement");
    return false;
  }
  ezb_zcl_attr_variable_t delta_var = {};
  delta_var.s16 = (int16_t)(delta * 100);  // MeasuredValue is a signed 16-bit attribute
  ezb_zcl_reporting_info_update(reporting_info, min_interval, max_interval, &delta_var);
  ezb_zcl_reporting_info_update_default_interval(reporting_info, min_interval, max_interval);
  return setClusterReporting(reporting_info);
}

bool ZigbeeTempSensor::setTemperature(float temperature) {
  int16_t zb_temperature = zb_float_to_s16(temperature);
  log_v("Updating temperature sensor value...");
  log_d("Setting temperature to %d", zb_temperature);
  ezb_zcl_status_t ret = setClusterAttribute(
    EZB_ZCL_CLUSTER_ID_TEMPERATURE_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_TEMPERATURE_MEASUREMENT_MEASURED_VALUE_ID, &zb_temperature, false
  );
  if (ret != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set temperature: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeTempSensor::reportTemperature() {
  /* Send report attributes command */
  ezb_zcl_report_attr_cmd_t report_attr_cmd;
  memset(&report_attr_cmd, 0, sizeof(report_attr_cmd));
  // No explicit destination: report to bound devices (replaces v1 ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT).
  ezb_address_set_none(&report_attr_cmd.cmd_ctrl.dst_addr);
  report_attr_cmd.cmd_ctrl.src_ep = _endpoint;
  report_attr_cmd.cmd_ctrl.cluster_id = EZB_ZCL_CLUSTER_ID_TEMPERATURE_MEASUREMENT;
  report_attr_cmd.cmd_ctrl.manuf_code = EZB_ZCL_STD_MANUF_CODE;
  report_attr_cmd.cmd_ctrl.fc.direction = EZB_ZCL_CMD_DIRECTION_TO_CLI;
  report_attr_cmd.payload.attr_id = EZB_ZCL_ATTR_TEMPERATURE_MEASUREMENT_MEASURED_VALUE_ID;

  if (!reportClusterAttribute(&report_attr_cmd)) {
    log_e("Failed to send temperature report");
    return false;
  }
  log_v("Temperature report sent");
  return true;
}

void ZigbeeTempSensor::addHumiditySensor(float min, float max, float tolerance, float defaultValue) {
  uint16_t zb_min = (uint16_t)(min * 100);
  uint16_t zb_max = (uint16_t)(max * 100);
  uint16_t zb_tolerance = (uint16_t)(tolerance * 100);
  uint16_t default_hum = (uint16_t)(defaultValue * 100);
  // v2.x: create the rel_humidity_measurement server cluster descriptor (mandatory attributes via config),
  // add the optional Tolerance attribute, then attach it to the existing endpoint descriptor.
  ezb_zcl_rel_humidity_measurement_cluster_server_config_t hum_cfg = {
    .measured_value = default_hum,
    .min_measured_value = zb_min,
    .max_measured_value = zb_max,
  };
  ezb_zcl_cluster_desc_t humidity_cluster = ezb_zcl_rel_humidity_measurement_create_cluster_desc(&hum_cfg, EZB_ZCL_CLUSTER_SERVER);
  ezb_zcl_rel_humidity_measurement_cluster_desc_add_attr(humidity_cluster, EZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_TOLERANCE_ID, (void *)&zb_tolerance);
  ezb_af_endpoint_add_cluster_desc(_ep_desc, humidity_cluster);
  _humidity_sensor = true;
}

bool ZigbeeTempSensor::setHumidity(float humidity) {
  uint16_t zb_humidity = (uint16_t)(humidity * 100);
  log_v("Updating humidity sensor value...");
  log_d("Setting humidity to %u", zb_humidity);
  ezb_zcl_status_t ret = setClusterAttribute(
    EZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_MEASURED_VALUE_ID, &zb_humidity, false
  );
  if (ret != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set humidity: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeTempSensor::reportHumidity() {
  /* Send report attributes command */
  ezb_zcl_report_attr_cmd_t report_attr_cmd;
  memset(&report_attr_cmd, 0, sizeof(report_attr_cmd));
  // No explicit destination: report to bound devices (replaces v1 ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT).
  ezb_address_set_none(&report_attr_cmd.cmd_ctrl.dst_addr);
  report_attr_cmd.cmd_ctrl.src_ep = _endpoint;
  report_attr_cmd.cmd_ctrl.cluster_id = EZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT;
  report_attr_cmd.cmd_ctrl.manuf_code = EZB_ZCL_STD_MANUF_CODE;
  report_attr_cmd.cmd_ctrl.fc.direction = EZB_ZCL_CMD_DIRECTION_TO_CLI;
  report_attr_cmd.payload.attr_id = EZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_MEASURED_VALUE_ID;

  if (!reportClusterAttribute(&report_attr_cmd)) {
    log_e("Failed to send humidity report");
    return false;
  }
  log_v("Humidity report sent");
  return true;
}

bool ZigbeeTempSensor::setHumidityReporting(uint16_t min_interval, uint16_t max_interval, float delta) {
  // NOTE(zb-v2): handle-based reporting, see setReporting(). Must be called after Zigbee.begin().
  ezb_zcl_reporting_info_t reporting_info = ezb_zcl_reporting_info_find(
    _endpoint, EZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_MEASURED_VALUE_ID,
    EZB_ZCL_STD_MANUF_CODE
  );
  if (reporting_info == EZB_ZCL_INVALID_REPORTING_INFO) {
    log_e("Failed to find reporting info for humidity measurement");
    return false;
  }
  ezb_zcl_attr_variable_t delta_var = {};
  delta_var.u16 = (uint16_t)(delta * 100);  // MeasuredValue is an unsigned 16-bit attribute
  ezb_zcl_reporting_info_update(reporting_info, min_interval, max_interval, &delta_var);
  ezb_zcl_reporting_info_update_default_interval(reporting_info, min_interval, max_interval);
  return setClusterReporting(reporting_info);
}

bool ZigbeeTempSensor::report() {
  bool temp_ret = reportTemperature();
  bool hum_ret = true;
  if (_humidity_sensor) {
    hum_ret = reportHumidity();
  }
  return temp_ret && hum_ret;
}

#endif  // CONFIG_ZB_ENABLED
