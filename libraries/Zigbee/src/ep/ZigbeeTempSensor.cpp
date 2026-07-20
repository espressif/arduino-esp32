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
#include "ezbee/zha.h"
#include "ezbee/zcl/cluster/rel_humidity_measurement_desc.h"


static int16_t zb_float_to_s16(float temp) {
  return (int16_t)(temp * 100);
}

ZigbeeTempSensor::ZigbeeTempSensor(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = EZB_ZHA_TEMPERATURE_SENSOR_DEVICE_ID;
  _temp_meas_cfg = {
    .measured_value = 0,
    .min_measured_value = 0,
    .max_measured_value = 0,
  };
  _tolerance = 0;
  _humidity_sensor = false;

  _ep_config = {
    .ep_id = _endpoint, .app_profile_id = EZB_AF_HA_PROFILE_ID, .app_device_id = EZB_ZHA_TEMPERATURE_SENSOR_DEVICE_ID, .app_device_version = 0
  };
  _ep_desc = ezb_af_create_endpoint_desc(&_ep_config);
  if (_ep_desc == nullptr) {
    log_e("Failed to create temperature sensor endpoint descriptor");
    return;
  }
  ezb_af_endpoint_add_cluster_desc(_ep_desc, ezb_zcl_basic_create_cluster_desc(nullptr, EZB_ZCL_CLUSTER_SERVER));
  ezb_af_endpoint_add_cluster_desc(_ep_desc, ezb_zcl_identify_create_cluster_desc(nullptr, EZB_ZCL_CLUSTER_SERVER));
  ezb_af_endpoint_add_cluster_desc(_ep_desc, ezb_zcl_temperature_measurement_create_cluster_desc(&_temp_meas_cfg, EZB_ZCL_CLUSTER_SERVER));
}

bool ZigbeeTempSensor::setMinMaxValue(float min, float max) {
  _temp_meas_cfg.min_measured_value = zb_float_to_s16(min);
  _temp_meas_cfg.max_measured_value = zb_float_to_s16(max);
  if (!configureEpClusterAttr(
        "setMinMaxValue", EZB_ZCL_CLUSTER_ID_TEMPERATURE_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_TEMPERATURE_MEASUREMENT_MIN_MEASURED_VALUE_ID,
        &_temp_meas_cfg.min_measured_value, ezb_zcl_temperature_measurement_cluster_desc_add_attr
      )) {
    return false;
  }
  return configureEpClusterAttr(
    "setMinMaxValue", EZB_ZCL_CLUSTER_ID_TEMPERATURE_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_TEMPERATURE_MEASUREMENT_MAX_MEASURED_VALUE_ID,
    &_temp_meas_cfg.max_measured_value, ezb_zcl_temperature_measurement_cluster_desc_add_attr
  );
}

bool ZigbeeTempSensor::setDefaultValue(float defaultValue) {
  _temp_meas_cfg.measured_value = zb_float_to_s16(defaultValue);
  return configureEpClusterAttr(
    "setDefaultValue", EZB_ZCL_CLUSTER_ID_TEMPERATURE_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_TEMPERATURE_MEASUREMENT_MEASURED_VALUE_ID,
    &_temp_meas_cfg.measured_value, ezb_zcl_temperature_measurement_cluster_desc_add_attr
  );
}

bool ZigbeeTempSensor::setTolerance(float tolerance) {
  _tolerance = (uint16_t)(tolerance * 100);
  return configureEpClusterAttr(
    "setTolerance", EZB_ZCL_CLUSTER_ID_TEMPERATURE_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_TEMPERATURE_MEASUREMENT_TOLERANCE_ID, &_tolerance,
    ezb_zcl_temperature_measurement_cluster_desc_add_attr
  );
}

bool ZigbeeTempSensor::setReporting(uint16_t min_interval, uint16_t max_interval, float delta) {
  ezb_zcl_reporting_info_t reporting_info = ezb_zcl_reporting_info_find(
    _endpoint, EZB_ZCL_CLUSTER_ID_TEMPERATURE_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_TEMPERATURE_MEASUREMENT_MEASURED_VALUE_ID, EZB_ZCL_STD_MANUF_CODE
  );
  if (reporting_info == EZB_ZCL_INVALID_REPORTING_INFO) {
    log_e("Failed to find reporting info for temperature measurement");
    return false;
  }
  ezb_zcl_attr_variable_t delta_var = {};
  delta_var.s16 = (int16_t)(delta * 100);
  ezb_zcl_reporting_info_update(reporting_info, min_interval, max_interval, &delta_var);
  ezb_zcl_reporting_info_update_default_interval(reporting_info, min_interval, max_interval);
  return setClusterReporting(reporting_info);
}

bool ZigbeeTempSensor::setTemperature(float temperature) {
  int16_t zb_temperature = zb_float_to_s16(temperature);
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
  ezb_zcl_report_attr_cmd_t report_attr_cmd;
  memset(&report_attr_cmd, 0, sizeof(report_attr_cmd));
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
  return true;
}

void ZigbeeTempSensor::addHumiditySensor(float min, float max, float tolerance, float defaultValue) {
  if (!requireBeforeAddEndpoint("addHumiditySensor")) {
    return;
  }
  if (_humidity_sensor) {
    return;
  }

  uint16_t zb_min = (uint16_t)(min * 100);
  uint16_t zb_max = (uint16_t)(max * 100);
  uint16_t zb_tolerance = (uint16_t)(tolerance * 100);
  uint16_t default_hum = (uint16_t)(defaultValue * 100);
  ezb_zcl_rel_humidity_measurement_cluster_server_config_t hum_cfg = {
    .measured_value = default_hum,
    .min_measured_value = zb_min,
    .max_measured_value = zb_max,
  };
  ezb_zcl_cluster_desc_t humidity_cluster = ezb_zcl_rel_humidity_measurement_create_cluster_desc(&hum_cfg, EZB_ZCL_CLUSTER_SERVER);
  ezb_zcl_rel_humidity_measurement_cluster_desc_add_attr(
    humidity_cluster, EZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_TOLERANCE_ID, (void *)&zb_tolerance
  );
  if (ezb_af_endpoint_add_cluster_desc(_ep_desc, humidity_cluster) != EZB_ERR_NONE) {
    log_e("Failed to add humidity cluster");
    return;
  }
  _humidity_sensor = true;
}

bool ZigbeeTempSensor::setHumidity(float humidity) {
  uint16_t zb_humidity = (uint16_t)(humidity * 100);
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
  ezb_zcl_report_attr_cmd_t report_attr_cmd;
  memset(&report_attr_cmd, 0, sizeof(report_attr_cmd));
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
  return true;
}

bool ZigbeeTempSensor::setHumidityReporting(uint16_t min_interval, uint16_t max_interval, float delta) {
  ezb_zcl_reporting_info_t reporting_info = ezb_zcl_reporting_info_find(
    _endpoint, EZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_MEASURED_VALUE_ID,
    EZB_ZCL_STD_MANUF_CODE
  );
  if (reporting_info == EZB_ZCL_INVALID_REPORTING_INFO) {
    log_e("Failed to find reporting info for humidity measurement");
    return false;
  }
  ezb_zcl_attr_variable_t delta_var = {};
  delta_var.u16 = (uint16_t)(delta * 100);
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
