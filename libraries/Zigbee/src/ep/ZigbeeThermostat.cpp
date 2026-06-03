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

#include "ZigbeeThermostat.h"
#if CONFIG_ZB_ENABLED

static float zb_s16_to_temperature(int16_t value) {
  return 1.0 * value / 100;
}

// v2.x addressing helpers: build the destination address for each addressing style.
// In v2.x there is no APS address_mode enum; the destination is expressed through ezb_address_t and the
// destination endpoint (0 when the endpoint is "not present", i.e. routed to bound devices or a group).
static ezb_address_t make_addr_bound() {
  ezb_address_t a;
  ezb_address_set_none(&a);  // no explicit destination -> routed to bound devices
  return a;
}

static ezb_address_t make_addr_group(uint16_t group_addr) {
  ezb_address_t a = {};
  a.addr_mode = EZB_ADDR_MODE_GROUP;
  a.u.group_addr.group = group_addr;
  a.u.group_addr.bcast = 0;
  return a;
}

static ezb_address_t make_addr_short(uint16_t short_addr) {
  ezb_address_t a;
  ezb_address_set_short(&a, short_addr);
  return a;
}

static ezb_address_t make_addr_ieee(const uint8_t *ieee_addr) {
  ezb_extaddr_t ext;
  memcpy(ext.u8, ieee_addr, sizeof(ext.u8));
  ezb_address_t a;
  ezb_address_set_extended(&a, ext);
  return a;
}

// Initialize the static instance of the class
ZigbeeThermostat *ZigbeeThermostat::_instance = nullptr;

ZigbeeThermostat::ZigbeeThermostat(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = EZB_ZHA_THERMOSTAT_DEVICE_ID;
  _instance = this;   // Set the static pointer to this instance
  _device = nullptr;  // Initialize sensor pointer to null
  _on_temp_receive = nullptr;
  _on_temp_receive_with_source = nullptr;
  _on_temp_config_receive = nullptr;
  _on_humidity_receive = nullptr;
  _on_humidity_receive_with_source = nullptr;
  _on_humidity_config_receive = nullptr;

  ezb_zha_thermostat_config_t thermostat_cfg = EZB_ZHA_THERMOSTAT_CONFIG();
  _ep_desc = ezb_zha_create_thermostat(_endpoint, &thermostat_cfg);

  // NOTE(zb-v2): ezb_zha_create_thermostat() only creates the Basic/Identify/Thermostat (server) clusters.
  // The v1 endpoint manually added Identify (client) plus Temperature/RelHumidity measurement (client)
  // clusters so the thermostat can bind to a remote sensor and accept its reports. Re-add them here with
  // ezb_af_endpoint_add_cluster_desc() (replaces the v1 esp_zb_cluster_list_add_*_cluster() calls).
  if (_ep_desc != nullptr) {
    ezb_af_endpoint_add_cluster_desc(_ep_desc, ezb_zcl_identify_create_cluster_desc(nullptr, EZB_ZCL_CLUSTER_CLIENT));
    ezb_af_endpoint_add_cluster_desc(_ep_desc, ezb_zcl_temperature_measurement_create_cluster_desc(nullptr, EZB_ZCL_CLUSTER_CLIENT));
    ezb_af_endpoint_add_cluster_desc(_ep_desc, ezb_zcl_rel_humidity_measurement_create_cluster_desc(nullptr, EZB_ZCL_CLUSTER_CLIENT));
  } else {
    log_e("Failed to create thermostat endpoint descriptor");
  }

  _ep_config = {.ep_id = _endpoint, .app_profile_id = EZB_AF_HA_PROFILE_ID, .app_device_id = EZB_ZHA_THERMOSTAT_DEVICE_ID, .app_device_version = 0};
}

void ZigbeeThermostat::bindCb(const ezb_zdp_bind_req_result_t *result, void *user_ctx) {
  ZigbeeThermostat *instance = static_cast<ZigbeeThermostat *>(user_ctx);
  bool success = result && result->error == EZB_ERR_NONE && result->rsp && result->rsp->status == EZB_ZDP_STATUS_SUCCESS;
  if (success) {
    log_i("Bound successfully!");
    if (instance->_device) {
      zb_device_params_t *sensor = (zb_device_params_t *)instance->_device;
      log_i("The sensor originating from address(0x%x) on endpoint(%u)", sensor->short_addr, sensor->endpoint);
      log_d("Sensor bound to thermostat on EP %u", instance->_endpoint);
      instance->_bound_devices.push_back(sensor);
    }
    instance->_is_bound = true;
  } else {
    instance->_device = nullptr;
  }
}

void ZigbeeThermostat::bindCbWrapper(const ezb_zdp_bind_req_result_t *result, void *user_ctx) {
  ZigbeeThermostat *instance = static_cast<ZigbeeThermostat *>(user_ctx);
  if (instance) {
    log_d("bindCbWrapper on EP %u", instance->_endpoint);
    instance->bindCb(result, user_ctx);
  }
}

// Static wrapper for findCb
void ZigbeeThermostat::findCbWrapper(const ezb_zdo_match_desc_req_result_t *result, void *user_ctx) {
  ZigbeeThermostat *instance = static_cast<ZigbeeThermostat *>(user_ctx);
  if (instance) {
    log_d("findCbWrapper on EP %u", instance->_endpoint);
    instance->findCb(result, user_ctx);
  }
}

void ZigbeeThermostat::findCb(const ezb_zdo_match_desc_req_result_t *result, void *user_ctx) {
  ZigbeeThermostat *instance = static_cast<ZigbeeThermostat *>(user_ctx);
  // A NULL response marks completion of a broadcast match; nothing matched if we never got a hit.
  if (result == nullptr || result->error != EZB_ERR_NONE || result->rsp == nullptr) {
    log_d("No temperature sensor endpoint found");
    return;
  }
  ezb_zdp_match_desc_rsp_field_t *rsp = result->rsp;
  if (rsp->status != EZB_ZDP_STATUS_SUCCESS || rsp->match_length == 0 || rsp->match_list == nullptr) {
    log_d("No temperature sensor endpoint found");
    return;
  }

  uint16_t addr = rsp->nwk_addr_of_interest;
  uint8_t endpoint = rsp->match_list[0];
  log_i("Found temperature sensor");

  // Store the information of the remote device
  zb_device_params_t *sensor = (zb_device_params_t *)malloc(sizeof(zb_device_params_t));
  sensor->endpoint = endpoint;
  sensor->short_addr = addr;
  ezb_extaddr_t sensor_ext;
  ezb_address_extended_by_short(sensor->short_addr, &sensor_ext);
  memcpy(sensor->ieee_addr, sensor_ext.u8, sizeof(sensor_ext.u8));
  log_d("Temperature sensor found: short address(0x%x), endpoint(%u)", sensor->short_addr, sensor->endpoint);

  // Our own (thermostat) extended address, used as the source/destination of the binding entries.
  ezb_extaddr_t self_ext;
  ezb_get_extended_address(&self_ext);

  ezb_zdo_bind_req_t bind_req;
  memset(&bind_req, 0, sizeof(bind_req));
  bind_req.field.dst_addr_mode = 0x03;  // 64-bit extended address, DstAddress and DstEndp present
  bind_req.cb = ZigbeeThermostat::bindCbWrapper;

  // 1. Ask the sensor to create binding entries pointing at us (this is what enables the sensor to send
  //    Temperature/RelHumidity reports to this thermostat). The request is addressed to the sensor.
  bind_req.dst_nwk_addr = addr;
  bind_req.field.src_addr = sensor_ext;
  bind_req.field.src_ep = endpoint;
  bind_req.field.dst_addr.extended_addr = self_ext;
  bind_req.field.dst_ep = instance->_endpoint;
  bind_req.user_ctx = nullptr;

  bind_req.field.cluster_id = EZB_ZCL_CLUSTER_ID_TEMPERATURE_MEASUREMENT;
  log_i("Request temperature sensor to bind us");
  ezb_zdo_bind_req(&bind_req);

  bind_req.field.cluster_id = EZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT;
  log_i("Request humidity sensor to bind us");
  ezb_zdo_bind_req(&bind_req);

  // 2. Create the matching binding entries on this device (addressed to ourselves), so commands sent from
  //    the thermostat are routed to the sensor.
  bind_req.dst_nwk_addr = ezb_nwk_get_short_address();
  bind_req.field.src_addr = self_ext;
  bind_req.field.src_ep = instance->_endpoint;
  bind_req.field.dst_addr.extended_addr = sensor_ext;
  bind_req.field.dst_ep = endpoint;

  // save sensor params in the class
  instance->_device = sensor;

  bind_req.field.cluster_id = EZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT;
  bind_req.user_ctx = nullptr;
  log_i("Try to bind Humidity Measurement");  // Optional cluster to bind, if fails, continue to bind the temperature measurement cluster
  ezb_zdo_bind_req(&bind_req);

  bind_req.field.cluster_id = EZB_ZCL_CLUSTER_ID_TEMPERATURE_MEASUREMENT;
  bind_req.user_ctx = this;
  log_i("Try to bind Temperature Measurement");  // Mandatory cluster to bind
  ezb_zdo_bind_req(&bind_req);
}

void ZigbeeThermostat::findEndpoint(ezb_zdo_match_desc_req_t *cmd_req) {
  // First num_in_clusters entries are server (input) clusters, then num_out_clusters client (output) clusters.
  static uint16_t cluster_list[] = {EZB_ZCL_CLUSTER_ID_TEMPERATURE_MEASUREMENT, EZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT};
  cmd_req->field.profile_id = EZB_AF_HA_PROFILE_ID;
  cmd_req->field.num_in_clusters = 2;
  cmd_req->field.num_out_clusters = 0;
  cmd_req->field.cluster_list = cluster_list;
  cmd_req->cb = ZigbeeThermostat::findCbWrapper;
  cmd_req->user_ctx = this;
  ezb_zdo_match_desc_req(cmd_req);
}

bool ZigbeeThermostat::sendReadAttributes(uint16_t cluster_id, uint16_t *attributes, uint8_t attr_number, ezb_address_t dst_addr, uint8_t dst_ep) {
  ezb_zcl_read_attr_cmd_t read_req;
  memset(&read_req, 0, sizeof(read_req));
  read_req.cmd_ctrl.dst_addr = dst_addr;
  read_req.cmd_ctrl.dst_ep = dst_ep;
  read_req.cmd_ctrl.src_ep = _endpoint;
  read_req.cmd_ctrl.cluster_id = cluster_id;
  read_req.payload.attr_number = attr_number;
  read_req.payload.attr_field = attributes;
  if (!readClusterAttribute(&read_req)) {
    log_e("Failed to send read attributes command");
    return false;
  }
  return true;
}

void ZigbeeThermostat::sendConfigReport(uint16_t cluster_id, ezb_zcl_config_report_record_t *records, uint16_t record_number, ezb_address_t dst_addr, uint8_t dst_ep) {
  ezb_zcl_config_report_cmd_t report_cmd;
  memset(&report_cmd, 0, sizeof(report_cmd));
  report_cmd.cmd_ctrl.dst_addr = dst_addr;
  report_cmd.cmd_ctrl.dst_ep = dst_ep;
  report_cmd.cmd_ctrl.src_ep = _endpoint;
  report_cmd.cmd_ctrl.cluster_id = cluster_id;
  report_cmd.cmd_ctrl.manuf_code = EZB_ZCL_STD_MANUF_CODE;
  report_cmd.payload.record_number = record_number;
  report_cmd.payload.record_field = records;
  configureClusterReporting(&report_cmd);
}

// NOTE(zb-v2): v1 built one ezb_zcl_config_report_record_t per addressing overload with the same content;
// these two helpers build the temperature (INT16) and humidity (UINT16) records once.
static ezb_zcl_config_report_record_t make_temp_report_record(uint16_t min_interval, uint16_t max_interval, int16_t report_change) {
  ezb_zcl_config_report_record_t record = {};
  record.direction = EZB_ZCL_REPORTING_SEND;
  record.attr_id = EZB_ZCL_ATTR_TEMPERATURE_MEASUREMENT_MEASURED_VALUE_ID;
  record.client.attr_type = EZB_ZCL_ATTR_TYPE_INT16;
  record.client.min_interval = min_interval;
  record.client.max_interval = max_interval;
  record.client.reportable_change.s16 = report_change;
  return record;
}

static ezb_zcl_config_report_record_t make_humidity_report_record(uint16_t min_interval, uint16_t max_interval, int16_t report_change) {
  ezb_zcl_config_report_record_t record = {};
  record.direction = EZB_ZCL_REPORTING_SEND;
  record.attr_id = EZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_MEASURED_VALUE_ID;
  record.client.attr_type = EZB_ZCL_ATTR_TYPE_UINT16;
  record.client.min_interval = min_interval;
  record.client.max_interval = max_interval;
  record.client.reportable_change.u16 = (uint16_t)report_change;
  return record;
}

void ZigbeeThermostat::zbAttributeRead(uint16_t cluster_id, const ezb_zcl_attribute_t *attribute, uint8_t src_endpoint, ezb_address_t src_address) {
  static uint8_t read_config = 0;
  if (cluster_id == EZB_ZCL_CLUSTER_ID_TEMPERATURE_MEASUREMENT) {
    if (attribute->id == EZB_ZCL_ATTR_TEMPERATURE_MEASUREMENT_MEASURED_VALUE_ID && attribute->data.type == EZB_ZCL_ATTR_TYPE_INT16) {
      int16_t value = attribute->data.value ? *(int16_t *)attribute->data.value : 0;
      if (_on_temp_receive) {
        _on_temp_receive(zb_s16_to_temperature(value));
      }
      if (_on_temp_receive_with_source) {
        _on_temp_receive_with_source(zb_s16_to_temperature(value), src_endpoint, src_address);
      }
    }
    if (attribute->id == EZB_ZCL_ATTR_TEMPERATURE_MEASUREMENT_MIN_MEASURED_VALUE_ID && attribute->data.type == EZB_ZCL_ATTR_TYPE_INT16) {
      int16_t min_value = attribute->data.value ? *(int16_t *)attribute->data.value : 0;
      _min_temp = zb_s16_to_temperature(min_value);
      read_config++;
      log_d("Received min temperature: %.2f°C from endpoint %u", _min_temp, src_endpoint);
    }
    if (attribute->id == EZB_ZCL_ATTR_TEMPERATURE_MEASUREMENT_MAX_MEASURED_VALUE_ID && attribute->data.type == EZB_ZCL_ATTR_TYPE_INT16) {
      int16_t max_value = attribute->data.value ? *(int16_t *)attribute->data.value : 0;
      _max_temp = zb_s16_to_temperature(max_value);
      read_config++;
      log_d("Received max temperature: %.2f°C from endpoint %u", _max_temp, src_endpoint);
    }
    if (attribute->id == EZB_ZCL_ATTR_TEMPERATURE_MEASUREMENT_TOLERANCE_ID && attribute->data.type == EZB_ZCL_ATTR_TYPE_UINT16) {
      uint16_t tolerance = attribute->data.value ? *(uint16_t *)attribute->data.value : 0;
      _tolerance_temp = 1.0 * tolerance / 100;
      read_config++;
      log_d("Received tolerance: %.2f°C from endpoint %u", _tolerance_temp, src_endpoint);
    }
    if (read_config == 3) {
      log_d("All temperature config attributes processed");
      read_config = 0;
      xSemaphoreGive(lock);
    }
  }
  static uint8_t read_humidity_config = 0;
  if (cluster_id == EZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT) {
    if (attribute->id == EZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_MEASURED_VALUE_ID && attribute->data.type == EZB_ZCL_ATTR_TYPE_UINT16) {
      uint16_t value = attribute->data.value ? *(uint16_t *)attribute->data.value : 0;
      float humidity = 1.0 * value / 100;
      if (_on_humidity_receive) {
        _on_humidity_receive(humidity);
      }
      if (_on_humidity_receive_with_source) {
        _on_humidity_receive_with_source(humidity, src_endpoint, src_address);
      }
    }
    if (attribute->id == EZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_MIN_MEASURED_VALUE_ID && attribute->data.type == EZB_ZCL_ATTR_TYPE_UINT16) {
      uint16_t min_value = attribute->data.value ? *(uint16_t *)attribute->data.value : 0;
      _min_humidity = 1.0 * min_value / 100;
      read_humidity_config++;
      log_d("Received min humidity: %.2f%% from endpoint %u", _min_humidity, src_endpoint);
    }
    if (attribute->id == EZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_MAX_MEASURED_VALUE_ID && attribute->data.type == EZB_ZCL_ATTR_TYPE_UINT16) {
      uint16_t max_value = attribute->data.value ? *(uint16_t *)attribute->data.value : 0;
      _max_humidity = 1.0 * max_value / 100;
      read_humidity_config++;
      log_d("Received max humidity: %.2f%% from endpoint %u", _max_humidity, src_endpoint);
    }
    if (attribute->id == EZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_TOLERANCE_ID && attribute->data.type == EZB_ZCL_ATTR_TYPE_UINT16) {
      uint16_t tolerance = attribute->data.value ? *(uint16_t *)attribute->data.value : 0;
      _tolerance_humidity = 1.0 * tolerance / 100;
      read_humidity_config++;
      log_d("Received tolerance: %.2f%% from endpoint %u", _tolerance_humidity, src_endpoint);
    }
    if (read_humidity_config == 3) {
      log_d("All humidity config attributes processed");
      read_humidity_config = 0;
      xSemaphoreGive(lock);
    }
  }
}

// Temperature measuring methods

void ZigbeeThermostat::getTemperature() {
  /* Send "read attributes" command to all bound sensors */
  uint16_t attributes[] = {EZB_ZCL_ATTR_TEMPERATURE_MEASUREMENT_MEASURED_VALUE_ID};
  log_i("Sending 'read temperature' command");
  sendReadAttributes(EZB_ZCL_CLUSTER_ID_TEMPERATURE_MEASUREMENT, attributes, ZB_ARRAY_LENGHT(attributes), make_addr_bound(), 0);
}

void ZigbeeThermostat::getTemperature(uint16_t group_addr) {
  /* Send "read attributes" command to the group */
  uint16_t attributes[] = {EZB_ZCL_ATTR_TEMPERATURE_MEASUREMENT_MEASURED_VALUE_ID};
  log_i("Sending 'read temperature' command to group address 0x%x", group_addr);
  sendReadAttributes(EZB_ZCL_CLUSTER_ID_TEMPERATURE_MEASUREMENT, attributes, ZB_ARRAY_LENGHT(attributes), make_addr_group(group_addr), 0);
}

void ZigbeeThermostat::getTemperature(uint8_t endpoint, uint16_t short_addr) {
  /* Send "read attributes" command to specific endpoint */
  uint16_t attributes[] = {EZB_ZCL_ATTR_TEMPERATURE_MEASUREMENT_MEASURED_VALUE_ID};
  log_i("Sending 'read temperature' command to endpoint %u, address 0x%x", endpoint, short_addr);
  sendReadAttributes(EZB_ZCL_CLUSTER_ID_TEMPERATURE_MEASUREMENT, attributes, ZB_ARRAY_LENGHT(attributes), make_addr_short(short_addr), endpoint);
}

void ZigbeeThermostat::getTemperature(uint8_t endpoint, const uint8_t *ieee_addr) {
  /* Send "read attributes" command to specific endpoint */
  uint16_t attributes[] = {EZB_ZCL_ATTR_TEMPERATURE_MEASUREMENT_MEASURED_VALUE_ID};
  log_i(
    "Sending 'read temperature' command to endpoint %u, ieee address %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x", endpoint, ieee_addr[7], ieee_addr[6],
    ieee_addr[5], ieee_addr[4], ieee_addr[3], ieee_addr[2], ieee_addr[1], ieee_addr[0]
  );
  sendReadAttributes(EZB_ZCL_CLUSTER_ID_TEMPERATURE_MEASUREMENT, attributes, ZB_ARRAY_LENGHT(attributes), make_addr_ieee(ieee_addr), endpoint);
}

void ZigbeeThermostat::getTemperatureSettings() {
  /* Send "read attributes" command to all bound sensors */
  uint16_t attributes[] = {
    EZB_ZCL_ATTR_TEMPERATURE_MEASUREMENT_MIN_MEASURED_VALUE_ID, EZB_ZCL_ATTR_TEMPERATURE_MEASUREMENT_MAX_MEASURED_VALUE_ID,
    EZB_ZCL_ATTR_TEMPERATURE_MEASUREMENT_TOLERANCE_ID
  };
  log_i("Sending 'read sensor settings' command");
  if (!sendReadAttributes(EZB_ZCL_CLUSTER_ID_TEMPERATURE_MEASUREMENT, attributes, ZB_ARRAY_LENGHT(attributes), make_addr_bound(), 0)) {
    return;
  }

  //Take semaphore to wait for response of all attributes
  if (xSemaphoreTake(lock, ZB_CMD_TIMEOUT) != pdTRUE) {
    log_e("Error while reading attributes");
    return;
  } else {
    //Call the callback function when all attributes are read
    _on_temp_config_receive(_min_temp, _max_temp, _tolerance_temp);
  }
}

void ZigbeeThermostat::getTemperatureSettings(uint16_t group_addr) {
  /* Send "read attributes" command to the group */
  uint16_t attributes[] = {
    EZB_ZCL_ATTR_TEMPERATURE_MEASUREMENT_MIN_MEASURED_VALUE_ID, EZB_ZCL_ATTR_TEMPERATURE_MEASUREMENT_MAX_MEASURED_VALUE_ID,
    EZB_ZCL_ATTR_TEMPERATURE_MEASUREMENT_TOLERANCE_ID
  };
  log_i("Sending 'read sensor settings' command to group address 0x%x", group_addr);
  if (!sendReadAttributes(EZB_ZCL_CLUSTER_ID_TEMPERATURE_MEASUREMENT, attributes, ZB_ARRAY_LENGHT(attributes), make_addr_group(group_addr), 0)) {
    return;
  }

  //Take semaphore to wait for response of all attributes
  if (xSemaphoreTake(lock, ZB_CMD_TIMEOUT) != pdTRUE) {
    log_e("Error while reading attributes");
    return;
  } else {
    //Call the callback function when all attributes are read
    _on_temp_config_receive(_min_temp, _max_temp, _tolerance_temp);
  }
}

void ZigbeeThermostat::getTemperatureSettings(uint8_t endpoint, uint16_t short_addr) {
  /* Send "read attributes" command to specific endpoint */
  uint16_t attributes[] = {
    EZB_ZCL_ATTR_TEMPERATURE_MEASUREMENT_MIN_MEASURED_VALUE_ID, EZB_ZCL_ATTR_TEMPERATURE_MEASUREMENT_MAX_MEASURED_VALUE_ID,
    EZB_ZCL_ATTR_TEMPERATURE_MEASUREMENT_TOLERANCE_ID
  };
  log_i("Sending 'read sensor settings' command to endpoint %u, address 0x%x", endpoint, short_addr);
  if (!sendReadAttributes(EZB_ZCL_CLUSTER_ID_TEMPERATURE_MEASUREMENT, attributes, ZB_ARRAY_LENGHT(attributes), make_addr_short(short_addr), endpoint)) {
    return;
  }

  //Take semaphore to wait for response of all attributes
  if (xSemaphoreTake(lock, ZB_CMD_TIMEOUT) != pdTRUE) {
    log_e("Error while reading attributes");
    return;
  } else {
    //Call the callback function when all attributes are read
    _on_temp_config_receive(_min_temp, _max_temp, _tolerance_temp);
  }
}

void ZigbeeThermostat::getTemperatureSettings(uint8_t endpoint, const uint8_t *ieee_addr) {
  /* Send "read attributes" command to specific endpoint */
  uint16_t attributes[] = {
    EZB_ZCL_ATTR_TEMPERATURE_MEASUREMENT_MIN_MEASURED_VALUE_ID, EZB_ZCL_ATTR_TEMPERATURE_MEASUREMENT_MAX_MEASURED_VALUE_ID,
    EZB_ZCL_ATTR_TEMPERATURE_MEASUREMENT_TOLERANCE_ID
  };
  log_i(
    "Sending 'read sensor settings' command to endpoint %u, ieee address %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x", endpoint, ieee_addr[7], ieee_addr[6],
    ieee_addr[5], ieee_addr[4], ieee_addr[3], ieee_addr[2], ieee_addr[1], ieee_addr[0]
  );
  if (!sendReadAttributes(EZB_ZCL_CLUSTER_ID_TEMPERATURE_MEASUREMENT, attributes, ZB_ARRAY_LENGHT(attributes), make_addr_ieee(ieee_addr), endpoint)) {
    return;
  }

  //Take semaphore to wait for response of all attributes
  if (xSemaphoreTake(lock, ZB_CMD_TIMEOUT) != pdTRUE) {
    log_e("Error while reading attributes");
    return;
  } else {
    //Call the callback function when all attributes are read
    _on_temp_config_receive(_min_temp, _max_temp, _tolerance_temp);
  }
}

void ZigbeeThermostat::setTemperatureReporting(uint16_t min_interval, uint16_t max_interval, float delta) {
  /* Send "configure report attribute" command to all bound sensors */
  int16_t report_change = (int16_t)delta * 100;
  ezb_zcl_config_report_record_t records[] = {make_temp_report_record(min_interval, max_interval, report_change)};
  log_i("Sending 'configure temperature reporting' command");
  sendConfigReport(EZB_ZCL_CLUSTER_ID_TEMPERATURE_MEASUREMENT, records, ZB_ARRAY_LENGHT(records), make_addr_bound(), 0);
}

void ZigbeeThermostat::setTemperatureReporting(uint16_t group_addr, uint16_t min_interval, uint16_t max_interval, float delta) {
  /* Send "configure report attribute" command to the group */
  int16_t report_change = (int16_t)delta * 100;
  ezb_zcl_config_report_record_t records[] = {make_temp_report_record(min_interval, max_interval, report_change)};
  log_i("Sending 'configure temperature reporting' command to group address 0x%x", group_addr);
  sendConfigReport(EZB_ZCL_CLUSTER_ID_TEMPERATURE_MEASUREMENT, records, ZB_ARRAY_LENGHT(records), make_addr_group(group_addr), 0);
}

void ZigbeeThermostat::setTemperatureReporting(uint8_t endpoint, uint16_t short_addr, uint16_t min_interval, uint16_t max_interval, float delta) {
  /* Send "configure report attribute" command to specific endpoint */
  int16_t report_change = (int16_t)delta * 100;
  ezb_zcl_config_report_record_t records[] = {make_temp_report_record(min_interval, max_interval, report_change)};
  log_i("Sending 'configure temperature reporting' command to endpoint %u, address 0x%x", endpoint, short_addr);
  sendConfigReport(EZB_ZCL_CLUSTER_ID_TEMPERATURE_MEASUREMENT, records, ZB_ARRAY_LENGHT(records), make_addr_short(short_addr), endpoint);
}

void ZigbeeThermostat::setTemperatureReporting(uint8_t endpoint, const uint8_t *ieee_addr, uint16_t min_interval, uint16_t max_interval, float delta) {
  /* Send "configure report attribute" command to specific endpoint */
  int16_t report_change = (int16_t)delta * 100;
  ezb_zcl_config_report_record_t records[] = {make_temp_report_record(min_interval, max_interval, report_change)};
  log_i(
    "Sending 'configure temperature reporting' command to endpoint %u, ieee address %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x", endpoint, ieee_addr[7],
    ieee_addr[6], ieee_addr[5], ieee_addr[4], ieee_addr[3], ieee_addr[2], ieee_addr[1], ieee_addr[0]
  );
  sendConfigReport(EZB_ZCL_CLUSTER_ID_TEMPERATURE_MEASUREMENT, records, ZB_ARRAY_LENGHT(records), make_addr_ieee(ieee_addr), endpoint);
}

// Humidity measuring methods

void ZigbeeThermostat::getHumidity() {
  /* Send "read attributes" command to all bound sensors */
  uint16_t attributes[] = {EZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_MEASURED_VALUE_ID};
  log_i("Sending 'read humidity' command");
  sendReadAttributes(EZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT, attributes, ZB_ARRAY_LENGHT(attributes), make_addr_bound(), 0);
}

void ZigbeeThermostat::getHumidity(uint16_t group_addr) {
  /* Send "read attributes" command to the group */
  uint16_t attributes[] = {EZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_MEASURED_VALUE_ID};
  log_i("Sending 'read humidity' command to group address 0x%x", group_addr);
  sendReadAttributes(EZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT, attributes, ZB_ARRAY_LENGHT(attributes), make_addr_group(group_addr), 0);
}

void ZigbeeThermostat::getHumidity(uint8_t endpoint, uint16_t short_addr) {
  /* Send "read attributes" command to specific endpoint */
  uint16_t attributes[] = {EZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_MEASURED_VALUE_ID};
  log_i("Sending 'read humidity' command to endpoint %u, address 0x%x", endpoint, short_addr);
  sendReadAttributes(EZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT, attributes, ZB_ARRAY_LENGHT(attributes), make_addr_short(short_addr), endpoint);
}

void ZigbeeThermostat::getHumidity(uint8_t endpoint, const uint8_t *ieee_addr) {
  /* Send "read attributes" command to specific endpoint */
  uint16_t attributes[] = {EZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_MEASURED_VALUE_ID};
  log_i(
    "Sending 'read humidity' command to endpoint %u, ieee address %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x", endpoint, ieee_addr[7], ieee_addr[6], ieee_addr[5],
    ieee_addr[4], ieee_addr[3], ieee_addr[2], ieee_addr[1], ieee_addr[0]
  );
  sendReadAttributes(EZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT, attributes, ZB_ARRAY_LENGHT(attributes), make_addr_ieee(ieee_addr), endpoint);
}

void ZigbeeThermostat::getHumiditySettings() {
  /* Send "read attributes" command to all bound sensors */
  uint16_t attributes[] = {
    EZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_MIN_MEASURED_VALUE_ID, EZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_MAX_MEASURED_VALUE_ID,
    EZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_TOLERANCE_ID
  };
  log_i("Sending 'read humidity settings' command");
  if (!sendReadAttributes(EZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT, attributes, ZB_ARRAY_LENGHT(attributes), make_addr_bound(), 0)) {
    return;
  }

  //Take semaphore to wait for response of all attributes
  if (xSemaphoreTake(lock, ZB_CMD_TIMEOUT) != pdTRUE) {
    log_e("Error while reading attributes");
    return;
  } else {
    //Call the callback function when all attributes are read
    _on_humidity_config_receive(_min_humidity, _max_humidity, _tolerance_humidity);
  }
}

void ZigbeeThermostat::getHumiditySettings(uint16_t group_addr) {
  /* Send "read attributes" command to the group */
  uint16_t attributes[] = {
    EZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_MIN_MEASURED_VALUE_ID, EZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_MAX_MEASURED_VALUE_ID,
    EZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_TOLERANCE_ID
  };
  log_i("Sending 'read humidity settings' command to group address 0x%x", group_addr);
  if (!sendReadAttributes(EZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT, attributes, ZB_ARRAY_LENGHT(attributes), make_addr_group(group_addr), 0)) {
    return;
  }

  //Take semaphore to wait for response of all attributes
  if (xSemaphoreTake(lock, ZB_CMD_TIMEOUT) != pdTRUE) {
    log_e("Error while reading attributes");
    return;
  } else {
    //Call the callback function when all attributes are read
    _on_humidity_config_receive(_min_humidity, _max_humidity, _tolerance_humidity);
  }
}

void ZigbeeThermostat::getHumiditySettings(uint8_t endpoint, uint16_t short_addr) {
  /* Send "read attributes" command to specific endpoint */
  uint16_t attributes[] = {
    EZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_MIN_MEASURED_VALUE_ID, EZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_MAX_MEASURED_VALUE_ID,
    EZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_TOLERANCE_ID
  };
  log_i("Sending 'read humidity settings' command to endpoint %u, address 0x%x", endpoint, short_addr);
  if (!sendReadAttributes(EZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT, attributes, ZB_ARRAY_LENGHT(attributes), make_addr_short(short_addr), endpoint)) {
    return;
  }

  //Take semaphore to wait for response of all attributes
  if (xSemaphoreTake(lock, ZB_CMD_TIMEOUT) != pdTRUE) {
    log_e("Error while reading attributes");
    return;
  } else {
    //Call the callback function when all attributes are read
    _on_humidity_config_receive(_min_humidity, _max_humidity, _tolerance_humidity);
  }
}

void ZigbeeThermostat::getHumiditySettings(uint8_t endpoint, const uint8_t *ieee_addr) {
  /* Send "read attributes" command to specific endpoint */
  uint16_t attributes[] = {
    EZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_MIN_MEASURED_VALUE_ID, EZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_MAX_MEASURED_VALUE_ID,
    EZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_TOLERANCE_ID
  };
  log_i(
    "Sending 'read humidity settings' command to endpoint %u, ieee address %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x", endpoint, ieee_addr[7], ieee_addr[6],
    ieee_addr[5], ieee_addr[4], ieee_addr[3], ieee_addr[2], ieee_addr[1], ieee_addr[0]
  );
  if (!sendReadAttributes(EZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT, attributes, ZB_ARRAY_LENGHT(attributes), make_addr_ieee(ieee_addr), endpoint)) {
    return;
  }

  //Take semaphore to wait for response of all attributes
  if (xSemaphoreTake(lock, ZB_CMD_TIMEOUT) != pdTRUE) {
    log_e("Error while reading attributes");
    return;
  } else {
    //Call the callback function when all attributes are read
    _on_humidity_config_receive(_min_humidity, _max_humidity, _tolerance_humidity);
  }
}

void ZigbeeThermostat::setHumidityReporting(uint16_t min_interval, uint16_t max_interval, float delta) {
  /* Send "configure report attribute" command to all bound sensors */
  int16_t report_change = (int16_t)delta * 100;
  ezb_zcl_config_report_record_t records[] = {make_humidity_report_record(min_interval, max_interval, report_change)};
  log_i("Sending 'configure humidity reporting' command");
  sendConfigReport(EZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT, records, ZB_ARRAY_LENGHT(records), make_addr_bound(), 0);
}

void ZigbeeThermostat::setHumidityReporting(uint16_t group_addr, uint16_t min_interval, uint16_t max_interval, float delta) {
  /* Send "configure report attribute" command to the group */
  int16_t report_change = (int16_t)delta * 100;
  ezb_zcl_config_report_record_t records[] = {make_humidity_report_record(min_interval, max_interval, report_change)};
  log_i("Sending 'configure humidity reporting' command to group address 0x%x", group_addr);
  sendConfigReport(EZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT, records, ZB_ARRAY_LENGHT(records), make_addr_group(group_addr), 0);
}

void ZigbeeThermostat::setHumidityReporting(uint8_t endpoint, uint16_t short_addr, uint16_t min_interval, uint16_t max_interval, float delta) {
  /* Send "configure report attribute" command to specific endpoint */
  int16_t report_change = (int16_t)delta * 100;
  ezb_zcl_config_report_record_t records[] = {make_humidity_report_record(min_interval, max_interval, report_change)};
  log_i("Sending 'configure humidity reporting' command to endpoint %u, address 0x%x", endpoint, short_addr);
  sendConfigReport(EZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT, records, ZB_ARRAY_LENGHT(records), make_addr_short(short_addr), endpoint);
}

void ZigbeeThermostat::setHumidityReporting(uint8_t endpoint, const uint8_t *ieee_addr, uint16_t min_interval, uint16_t max_interval, float delta) {
  /* Send "configure report attribute" command to specific endpoint */
  int16_t report_change = (int16_t)delta * 100;
  ezb_zcl_config_report_record_t records[] = {make_humidity_report_record(min_interval, max_interval, report_change)};
  log_i(
    "Sending 'configure humidity reporting' command to endpoint %u, ieee address %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x", endpoint, ieee_addr[7], ieee_addr[6],
    ieee_addr[5], ieee_addr[4], ieee_addr[3], ieee_addr[2], ieee_addr[1], ieee_addr[0]
  );
  sendConfigReport(EZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT, records, ZB_ARRAY_LENGHT(records), make_addr_ieee(ieee_addr), endpoint);
}
#endif  // CONFIG_ZB_ENABLED
