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

/* Common Class for Zigbee End Point */

#include "Arduino.h"
#include "ZigbeeEP.h"

#if CONFIG_ZB_ENABLED

// NOTE(zb-v2): The v1 ZCL helper headers (esp_zigbee_cluster.h, zcl/esp_zigbee_zcl_power_config.h) are
// gone. All v2.x ZCL data-model/command APIs and per-cluster attribute IDs are pulled in through the
// umbrella "esp_zigbee.h" (included by ZigbeeCore.h -> ZigbeeEP.h).

#include <stdint.h>

/* ZigBee ZCL UTCTime: seconds since 2000-01-01 00:00:00 UTC (not Unix 1970 epoch). */
static constexpr int64_t ZIGBEE_UTCTIME_UNIX_OFFSET_SEC = 946684800LL;
static constexpr uint32_t ZIGBEE_UTCTIME_INVALID = UINT32_MAX;

static uint32_t zb_utctime_from_unix(time_t unix_ts) {
  if (unix_ts == (time_t)-1) {
    return ZIGBEE_UTCTIME_INVALID;
  }
  int64_t sec = (int64_t)unix_ts - ZIGBEE_UTCTIME_UNIX_OFFSET_SEC;
  if (sec < 0) {
    return ZIGBEE_UTCTIME_INVALID;
  }
  if (sec > (int64_t)UINT32_MAX) {
    return ZIGBEE_UTCTIME_INVALID;
  }
  return (uint32_t)sec;
}

static time_t unix_time_from_zb_utctime(uint32_t zb_sec) {
  if (zb_sec == ZIGBEE_UTCTIME_INVALID) {
    return (time_t)-1;
  }
  return (time_t)(ZIGBEE_UTCTIME_UNIX_OFFSET_SEC + (int64_t)zb_sec);
}

/* Zigbee End Device Class */
ZigbeeEP::ZigbeeEP(uint8_t endpoint) {
  _endpoint = endpoint;
  log_v("Endpoint: %u", _endpoint);
  _ep_config.ep_id = 0;
  _ep_desc = nullptr;
  _on_identify = nullptr;
  _on_ota_state_change = nullptr;
  _on_default_response = nullptr;
  _on_privilege_command = nullptr;
  _on_custom_cluster_command = nullptr;
  _read_model = NULL;
  _read_manufacturer = NULL;
  _time_status = 0;
  _is_bound = false;
  _use_manual_binding = false;
  _allow_multiple_binding = false;
  if (!lock) {
    lock = xSemaphoreCreateBinary();
    if (lock == NULL) {
      log_e("Semaphore creation failed");
    }
  }
}

ezb_zcl_status_t ZigbeeEP::setClusterAttribute(uint16_t cluster_id, uint8_t cluster_role, uint16_t attr_id, void *value, bool check) {
  if (!Zigbee.initialized()) {
    log_w("Cannot set attribute: Zigbee stack not initialized");
    return EZB_ZCL_STATUS_FAIL;
  }
  if (!esp_zigbee_lock_acquire(portMAX_DELAY)) {
    log_w("Cannot set attribute: failed to acquire Zigbee lock");
    return EZB_ZCL_STATUS_FAIL;
  }
  // v2.x set takes an explicit manufacturer code (standard attributes use EZB_ZCL_STD_MANUF_CODE).
  ezb_zcl_status_t ret = ezb_zcl_set_attr_value(_endpoint, cluster_id, cluster_role, attr_id, EZB_ZCL_STD_MANUF_CODE, value, check);
  esp_zigbee_lock_release();
  return ret;
}

bool ZigbeeEP::getClusterAttribute(uint16_t cluster_id, uint8_t cluster_role, uint16_t attr_id, void *value, uint16_t value_size) {
  if (value == nullptr || value_size == 0) {
    log_e("Get attribute value buffer is invalid");
    return false;
  }
  if (!Zigbee.initialized()) {
    log_w("Cannot get attribute: Zigbee stack not initialized");
    return false;
  }
  if (!esp_zigbee_lock_acquire(portMAX_DELAY)) {
    log_w("Cannot get attribute: failed to acquire Zigbee lock");
    return false;
  }
  // v2.x: attributes are opaque descriptors; fetch the descriptor and copy its value into the caller buffer.
  ezb_zcl_attr_desc_t attr = ezb_zcl_get_attr_desc(_endpoint, cluster_id, cluster_role, attr_id, EZB_ZCL_STD_MANUF_CODE);
  if (attr == nullptr) {
    esp_zigbee_lock_release();
    log_w("Cannot get attribute: attribute not found");
    return false;
  }
  ezb_err_t err = ezb_zcl_attr_desc_get_value(attr, value);
  esp_zigbee_lock_release();
  if (err != EZB_ERR_NONE) {
    log_w("Cannot get attribute: failed to read value (0x%x)", err);
    return false;
  }
  return true;
}

bool ZigbeeEP::reportClusterAttribute(ezb_zcl_report_attr_cmd_t *report_attr_cmd) {
  if (report_attr_cmd == nullptr) {
    log_e("Report attribute command is null");
    return false;
  }
  if (!Zigbee.started()) {
    log_w("Cannot report attribute: Zigbee stack not started");
    return false;
  }
  if (!esp_zigbee_lock_acquire(portMAX_DELAY)) {
    log_w("Cannot report attribute: failed to acquire Zigbee lock");
    return false;
  }
  ezb_err_t ret = ezb_zcl_report_attr_cmd_req(report_attr_cmd);
  esp_zigbee_lock_release();
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to report attribute: 0x%x", ret);
    return false;
  }
  return true;
}

bool ZigbeeEP::readClusterAttribute(ezb_zcl_read_attr_cmd_t *read_req) {
  if (read_req == nullptr) {
    log_e("Read attribute command is null");
    return false;
  }
  if (!Zigbee.started()) {
    log_w("Cannot read attribute: Zigbee stack not started");
    return false;
  }
  if (!esp_zigbee_lock_acquire(portMAX_DELAY)) {
    log_w("Cannot read attribute: failed to acquire Zigbee lock");
    return false;
  }
  ezb_err_t ret = ezb_zcl_read_attr_cmd_req(read_req);
  esp_zigbee_lock_release();
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to read attribute: 0x%x", ret);
    return false;
  }
  return true;
}

bool ZigbeeEP::setClusterReporting(ezb_zcl_reporting_info_t reporting_info) {
  // NOTE(zb-v2): The v1 esp_zb_zcl_update_reporting_info(struct*) call is replaced by the handle-based
  // reporting API. The caller is expected to obtain the handle via ezb_zcl_reporting_info_find() (and
  // tune it with ezb_zcl_reporting_info_update()) before starting it here.
  if (reporting_info == EZB_ZCL_INVALID_REPORTING_INFO) {
    log_e("Reporting info handle is invalid");
    return false;
  }
  if (!Zigbee.initialized()) {
    log_w("Cannot configure reporting: Zigbee stack not initialized");
    return false;
  }
  if (!esp_zigbee_lock_acquire(portMAX_DELAY)) {
    log_w("Cannot configure reporting: failed to acquire Zigbee lock");
    return false;
  }
  ezb_err_t ret = ezb_zcl_reporting_start_attr_report(reporting_info);
  esp_zigbee_lock_release();
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to start attribute reporting: 0x%x", ret);
    return false;
  }
  return true;
}

bool ZigbeeEP::configureClusterReporting(ezb_zcl_config_report_cmd_t *report_cmd) {
  if (report_cmd == nullptr) {
    log_e("Configure report command is null");
    return false;
  }
  if (!Zigbee.started()) {
    log_w("Cannot configure cluster reporting: Zigbee stack not started");
    return false;
  }
  if (!esp_zigbee_lock_acquire(portMAX_DELAY)) {
    log_w("Cannot configure cluster reporting: failed to acquire Zigbee lock");
    return false;
  }
  ezb_err_t ret = ezb_zcl_config_report_cmd_req(report_cmd);
  esp_zigbee_lock_release();
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to configure cluster reporting: 0x%x", ret);
    return false;
  }
  return true;
}

bool ZigbeeEP::acquireCommandLock() {
  if (!Zigbee.initialized()) {
    log_w("Cannot send command: Zigbee stack not initialized");
    return false;
  }
  if (!esp_zigbee_lock_acquire(portMAX_DELAY)) {
    log_w("Cannot send command: failed to acquire Zigbee lock");
    return false;
  }
  return true;
}

void ZigbeeEP::releaseCommandLock() {
  esp_zigbee_lock_release();
}

void ZigbeeEP::setVersion(uint8_t version) {
  _ep_config.app_device_version = version;
  // The application device version is part of the simple descriptor; keep the endpoint descriptor in sync.
  ezb_af_ep_desc_set_app_version(_ep_desc, version);

  ezb_zcl_cluster_desc_t basic_cluster = ezb_af_endpoint_get_cluster_desc(_ep_desc, EZB_ZCL_CLUSTER_ID_BASIC, EZB_ZCL_CLUSTER_SERVER);
  if (basic_cluster == nullptr) {
    log_e("Failed to get basic cluster for application version");
    return;
  }
  ezb_err_t ret = ezb_zcl_basic_cluster_desc_add_attr(basic_cluster, EZB_ZCL_ATTR_BASIC_APPLICATION_VERSION_ID, (void *)&version);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to add application version to basic cluster: 0x%x", ret);
  }
}

void ZigbeeEP::setHardwareVersion(uint8_t version) {
  ezb_zcl_cluster_desc_t basic_cluster = ezb_af_endpoint_get_cluster_desc(_ep_desc, EZB_ZCL_CLUSTER_ID_BASIC, EZB_ZCL_CLUSTER_SERVER);
  if (basic_cluster == nullptr) {
    log_e("Failed to get basic cluster for hardware version");
    return;
  }
  ezb_err_t ret = ezb_zcl_basic_cluster_desc_add_attr(basic_cluster, EZB_ZCL_ATTR_BASIC_HW_VERSION_ID, (void *)&version);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to add hardware version to basic cluster: 0x%x", ret);
  }
}

bool ZigbeeEP::setManufacturerAndModel(const char *name, const char *model) {
  // Allocate a new array of size length + 2 (1 for the length, 1 for null terminator)
  char zb_name[ZB_MAX_NAME_LENGTH + 2];
  char zb_model[ZB_MAX_NAME_LENGTH + 2];

  // Convert manufacturer to ZCL string
  size_t name_length = strlen(name);
  size_t model_length = strlen(model);
  if (name_length > ZB_MAX_NAME_LENGTH || model_length > ZB_MAX_NAME_LENGTH) {
    log_e("Manufacturer or model name is too long");
    return false;
  }
  // Get and check the basic cluster
  ezb_zcl_cluster_desc_t basic_cluster = ezb_af_endpoint_get_cluster_desc(_ep_desc, EZB_ZCL_CLUSTER_ID_BASIC, EZB_ZCL_CLUSTER_SERVER);
  if (basic_cluster == nullptr) {
    log_e("Failed to get basic cluster");
    return false;
  }
  // Store the length as the first element
  zb_name[0] = static_cast<char>(name_length);  // Cast size_t to char
  zb_model[0] = static_cast<char>(model_length);
  // Use memcpy to copy the characters to the result array
  memcpy(zb_name + 1, name, name_length);
  memcpy(zb_model + 1, model, model_length);
  // Null-terminate the array
  zb_name[name_length + 1] = '\0';
  zb_model[model_length + 1] = '\0';
  // Update the manufacturer and model attributes
  ezb_err_t ret_name = ezb_zcl_basic_cluster_desc_add_attr(basic_cluster, EZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, (void *)zb_name);
  if (ret_name != EZB_ERR_NONE) {
    log_e("Failed to set manufacturer: 0x%x", ret_name);
  }
  ezb_err_t ret_model = ezb_zcl_basic_cluster_desc_add_attr(basic_cluster, EZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, (void *)zb_model);
  if (ret_model != EZB_ERR_NONE) {
    log_e("Failed to set model: 0x%x", ret_model);
  }
  return ret_name == EZB_ERR_NONE && ret_model == EZB_ERR_NONE;
}

bool ZigbeeEP::setPowerSource(zb_power_source_t power_source, uint8_t battery_percentage, uint8_t battery_voltage) {
  ezb_zcl_cluster_desc_t basic_cluster = ezb_af_endpoint_get_cluster_desc(_ep_desc, EZB_ZCL_CLUSTER_ID_BASIC, EZB_ZCL_CLUSTER_SERVER);
  // v2.x descriptor model: adding an attribute that already exists updates its value (replaces esp_zb_cluster_update_attr).
  ezb_err_t ret = ezb_zcl_basic_cluster_desc_add_attr(basic_cluster, EZB_ZCL_ATTR_BASIC_POWER_SOURCE_ID, (void *)&power_source);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to set power source: 0x%x", ret);
    return false;
  }

  if (power_source == ZB_POWER_SOURCE_BATTERY) {
    // Add power config cluster and battery percentage attribute
    if (battery_percentage > 100) {
      battery_percentage = 100;
    }
    battery_percentage = battery_percentage * 2;
    ezb_zcl_cluster_desc_t power_config_cluster = ezb_zcl_power_config_create_cluster_desc(nullptr, EZB_ZCL_CLUSTER_SERVER);
    ret = ezb_zcl_power_config_cluster_desc_add_attr(power_config_cluster, EZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID, (void *)&battery_percentage);
    if (ret != EZB_ERR_NONE) {
      log_e("Failed to add battery percentage attribute: 0x%x", ret);
      return false;
    }
    ret = ezb_zcl_power_config_cluster_desc_add_attr(power_config_cluster, EZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID, (void *)&battery_voltage);
    if (ret != EZB_ERR_NONE) {
      log_e("Failed to add battery voltage attribute: 0x%x", ret);
      return false;
    }
    ret = ezb_af_endpoint_add_cluster_desc(_ep_desc, power_config_cluster);
    if (ret != EZB_ERR_NONE) {
      log_e("Failed to add power config cluster: 0x%x", ret);
      return false;
    }
  }
  _power_source = power_source;
  return true;
}

bool ZigbeeEP::setBatteryPercentage(uint8_t percentage) {
  // 100% = 200 in decimal, 0% = 0
  // Convert percentage to 0-200 range
  ezb_zcl_status_t ret = EZB_ZCL_STATUS_SUCCESS;
  if (percentage > 100) {
    percentage = 100;
  }
  percentage = percentage * 2;
  ret = setClusterAttribute(
    EZB_ZCL_CLUSTER_ID_POWER_CONFIG, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID, &percentage, false
  );
  if (ret != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set battery percentage: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  log_v("Battery percentage updated");
  return true;
}

bool ZigbeeEP::setBatteryVoltage(uint8_t voltage) {
  ezb_zcl_status_t ret =
    setClusterAttribute(EZB_ZCL_CLUSTER_ID_POWER_CONFIG, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID, &voltage, false);
  if (ret != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set battery voltage: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  log_v("Battery voltage updated");
  return true;
}

bool ZigbeeEP::reportBatteryPercentage() {
  /* Send report attributes command */
  ezb_zcl_report_attr_cmd_t report_attr_cmd;
  memset(&report_attr_cmd, 0, sizeof(report_attr_cmd));
  // No explicit destination: report to bound devices (replaces v1 ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT).
  ezb_address_set_none(&report_attr_cmd.cmd_ctrl.dst_addr);
  report_attr_cmd.cmd_ctrl.src_ep = _endpoint;
  report_attr_cmd.cmd_ctrl.cluster_id = EZB_ZCL_CLUSTER_ID_POWER_CONFIG;
  report_attr_cmd.cmd_ctrl.manuf_code = EZB_ZCL_STD_MANUF_CODE;
  report_attr_cmd.cmd_ctrl.fc.direction = EZB_ZCL_CMD_DIRECTION_TO_CLI;
  report_attr_cmd.payload.attr_id = EZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID;

  if (!reportClusterAttribute(&report_attr_cmd)) {
    return false;
  }
  log_v("Battery percentage reported");
  return true;
}

char *ZigbeeEP::readManufacturer(uint8_t endpoint, uint16_t short_addr, const uint8_t *ieee_addr) {
  /* Read peer Manufacture Name & Model Identifier */
  ezb_zcl_read_attr_cmd_t read_req;
  memset(&read_req, 0, sizeof(read_req));

  if (short_addr != 0) {
    ezb_address_set_short(&read_req.cmd_ctrl.dst_addr, short_addr);
  } else {
    ezb_extaddr_t ext;
    memcpy(ext.u8, ieee_addr, sizeof(ext.u8));
    ezb_address_set_extended(&read_req.cmd_ctrl.dst_addr, ext);
  }

  read_req.cmd_ctrl.src_ep = _endpoint;
  read_req.cmd_ctrl.dst_ep = endpoint;
  read_req.cmd_ctrl.cluster_id = EZB_ZCL_CLUSTER_ID_BASIC;

  uint16_t attributes[] = {
    EZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID,
  };
  read_req.payload.attr_number = ZB_ARRAY_LENGHT(attributes);
  read_req.payload.attr_field = attributes;

  if (_read_manufacturer != NULL) {
    free(_read_manufacturer);
  }
  _read_manufacturer = NULL;

  if (!readClusterAttribute(&read_req)) {
    return NULL;
  }

  //Wait for response or timeout
  if (xSemaphoreTake(lock, ZB_CMD_TIMEOUT) != pdTRUE) {
    log_e("Error while reading manufacturer");
  }
  return _read_manufacturer;
}

char *ZigbeeEP::readModel(uint8_t endpoint, uint16_t short_addr, const uint8_t *ieee_addr) {
  /* Read peer Manufacture Name & Model Identifier */
  ezb_zcl_read_attr_cmd_t read_req;
  memset(&read_req, 0, sizeof(read_req));

  if (short_addr != 0) {
    ezb_address_set_short(&read_req.cmd_ctrl.dst_addr, short_addr);
  } else {
    ezb_extaddr_t ext;
    memcpy(ext.u8, ieee_addr, sizeof(ext.u8));
    ezb_address_set_extended(&read_req.cmd_ctrl.dst_addr, ext);
  }

  read_req.cmd_ctrl.src_ep = _endpoint;
  read_req.cmd_ctrl.dst_ep = endpoint;
  read_req.cmd_ctrl.cluster_id = EZB_ZCL_CLUSTER_ID_BASIC;

  uint16_t attributes[] = {
    EZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID,
  };
  read_req.payload.attr_number = ZB_ARRAY_LENGHT(attributes);
  read_req.payload.attr_field = attributes;

  if (_read_model != NULL) {
    free(_read_model);
  }
  _read_model = NULL;

  if (!readClusterAttribute(&read_req)) {
    return NULL;
  }

  //Wait for response or timeout
  if (xSemaphoreTake(lock, ZB_CMD_TIMEOUT) != pdTRUE) {
    log_e("Error while reading model");
  }
  return _read_model;
}

void ZigbeeEP::printBoundDevices() {
  log_i("Bound devices:");
  for ([[maybe_unused]]
       const auto &device : _bound_devices) {
    log_i(
      "Device on endpoint %u, short address: 0x%x, ieee address: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x", device->endpoint, device->short_addr,
      device->ieee_addr[7], device->ieee_addr[6], device->ieee_addr[5], device->ieee_addr[4], device->ieee_addr[3], device->ieee_addr[2], device->ieee_addr[1],
      device->ieee_addr[0]
    );
  }
}

void ZigbeeEP::printBoundDevices(Print &print) {
  print.println("Bound devices:");
  for ([[maybe_unused]]
       const auto &device : _bound_devices) {
    print.printf(
      "Device on endpoint %u, short address: 0x%x, ieee address: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\r\n", device->endpoint, device->short_addr,
      device->ieee_addr[7], device->ieee_addr[6], device->ieee_addr[5], device->ieee_addr[4], device->ieee_addr[3], device->ieee_addr[2], device->ieee_addr[1],
      device->ieee_addr[0]
    );
  }
}

void ZigbeeEP::zbReadBasicCluster(const ezb_zcl_attribute_t *attribute) {
  /* Basic cluster attributes */
  if (attribute->id == EZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID && attribute->data.type == EZB_ZCL_ATTR_TYPE_STRING && attribute->data.value) {
    zbstring_t *zbstr = (zbstring_t *)attribute->data.value;
    _read_manufacturer = (char *)malloc(zbstr->len + 1);
    if (_read_manufacturer == NULL) {
      log_e("Failed to allocate memory for manufacturer data");
      xSemaphoreGive(lock);
      return;
    }
    memcpy(_read_manufacturer, zbstr->data, zbstr->len);
    _read_manufacturer[zbstr->len] = '\0';
    log_i("Peer Manufacturer is \"%s\"", _read_manufacturer);
    xSemaphoreGive(lock);
  }
  if (attribute->id == EZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID && attribute->data.type == EZB_ZCL_ATTR_TYPE_STRING && attribute->data.value) {
    zbstring_t *zbstr = (zbstring_t *)attribute->data.value;
    _read_model = (char *)malloc(zbstr->len + 1);
    if (_read_model == NULL) {
      log_e("Failed to allocate memory for model data");
      xSemaphoreGive(lock);
      return;
    }
    memcpy(_read_model, zbstr->data, zbstr->len);
    _read_model[zbstr->len] = '\0';
    log_i("Peer Model is \"%s\"", _read_model);
    xSemaphoreGive(lock);
  }
}

void ZigbeeEP::zbIdentify(const ezb_zcl_set_attr_value_message_t *message) {
  // NOTE(zb-v2): set-attr messages nest the attribute under .in; the IdentifyTime attribute (0x0000) is
  // matched here. Confirm EZB_ZCL_ATTR_IDENTIFY_IDENTIFY_TIME_ID against ezbee/zcl/cluster/identify_desc.h.
  if (message->in.attribute.id == EZB_ZCL_ATTR_IDENTIFY_IDENTIFY_TIME_ID && message->in.attribute.data.type == EZB_ZCL_ATTR_TYPE_UINT16) {
    if (_on_identify != NULL) {
      _on_identify(*(uint16_t *)message->in.attribute.data.value);
    }
  } else {
    log_w("Other identify commands are not implemented yet.");
  }
}

void ZigbeeEP::zbOTAState(bool otaActive) {
  if (_on_ota_state_change != NULL) {
    _on_ota_state_change(otaActive);
  }
}

bool ZigbeeEP::addTimeCluster(tm time, int32_t gmt_offset) {
  uint32_t zb_utctime = 0;
  // Check if time is set
  if (time.tm_year > 0) {
    time_t unix_ts = mktime(&time);
    if (unix_ts == (time_t)-1) {
      log_e("Invalid calendar time");
      return false;
    }
    zb_utctime = zb_utctime_from_unix(unix_ts);
  }

  // Create time cluster server attributes
  ezb_zcl_cluster_desc_t time_cluster_server = ezb_zcl_time_create_cluster_desc(nullptr, EZB_ZCL_CLUSTER_SERVER);
  ezb_err_t ret = ezb_zcl_time_cluster_desc_add_attr(time_cluster_server, EZB_ZCL_ATTR_TIME_TIME_ZONE_ID, (void *)&gmt_offset);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to add time zone attribute: 0x%x", ret);
    return false;
  }
  ret = ezb_zcl_time_cluster_desc_add_attr(time_cluster_server, EZB_ZCL_ATTR_TIME_TIME_ID, (void *)&zb_utctime);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to add time attribute: 0x%x", ret);
    return false;
  }
  ret = ezb_zcl_time_cluster_desc_add_attr(time_cluster_server, EZB_ZCL_ATTR_TIME_TIME_STATUS_ID, (void *)&_time_status);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to add time status attribute: 0x%x", ret);
    return false;
  }
  // Create time cluster client attributes
  ezb_zcl_cluster_desc_t time_cluster_client = ezb_zcl_time_create_cluster_desc(nullptr, EZB_ZCL_CLUSTER_CLIENT);
  // Add time clusters to the endpoint descriptor
  ret = ezb_af_endpoint_add_cluster_desc(_ep_desc, time_cluster_server);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to add time cluster (server role): 0x%x", ret);
    return false;
  }
  ret = ezb_af_endpoint_add_cluster_desc(_ep_desc, time_cluster_client);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to add time cluster (client role): 0x%x", ret);
    return false;
  }
  return true;
}

bool ZigbeeEP::setTime(tm time) {
  ezb_zcl_status_t ret = EZB_ZCL_STATUS_SUCCESS;
  time_t unix_ts = mktime(&time);
  if (unix_ts == (time_t)-1) {
    log_e("Invalid calendar time");
    return false;
  }
  uint32_t zb_utctime = zb_utctime_from_unix(unix_ts);
  log_d("Setting ZCL UTCTime to %" PRIu32 " s since 2000-01-01 UTC", zb_utctime);
  ret = setClusterAttribute(EZB_ZCL_CLUSTER_ID_TIME, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_TIME_TIME_ID, &zb_utctime, false);
  if (ret != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set time: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeEP::setTimezone(int32_t gmt_offset) {
  ezb_zcl_status_t ret = EZB_ZCL_STATUS_SUCCESS;
  log_d("Setting timezone to %" PRId32, gmt_offset);
  ret = setClusterAttribute(EZB_ZCL_CLUSTER_ID_TIME, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_TIME_TIME_ZONE_ID, &gmt_offset, false);
  if (ret != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set timezone: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  return true;
}

tm ZigbeeEP::getTime(uint8_t endpoint, int32_t short_addr, const uint8_t *ieee_addr) {
  /* Read peer time */
  ezb_zcl_read_attr_cmd_t read_req;
  memset(&read_req, 0, sizeof(read_req));

  if (short_addr >= 0) {
    ezb_address_set_short(&read_req.cmd_ctrl.dst_addr, (uint16_t)short_addr);
  } else {
    ezb_extaddr_t ext;
    memcpy(ext.u8, ieee_addr, sizeof(ext.u8));
    ezb_address_set_extended(&read_req.cmd_ctrl.dst_addr, ext);
  }

  uint16_t attributes[] = {EZB_ZCL_ATTR_TIME_TIME_ID};
  read_req.payload.attr_number = ZB_ARRAY_LENGHT(attributes);
  read_req.payload.attr_field = attributes;

  read_req.cmd_ctrl.cluster_id = EZB_ZCL_CLUSTER_ID_TIME;

  read_req.cmd_ctrl.dst_ep = endpoint;
  read_req.cmd_ctrl.src_ep = _endpoint;

  // clear read time
  _read_time = 0;

  log_v("Reading time from endpoint %u", endpoint);
  if (!readClusterAttribute(&read_req)) {
    return tm();
  }

  //Wait for response or timeout
  if (xSemaphoreTake(lock, ZB_CMD_TIMEOUT) != pdTRUE) {
    log_e("Error while reading time");
    return tm();
  }

  time_t unix_ts = unix_time_from_zb_utctime((uint32_t)_read_time);
  struct tm *timeinfo = localtime(&unix_ts);
  if (timeinfo) {
    // Update time
    setTime(*timeinfo);
    // Update time status to synced
    _time_status |= 0x02;
    setClusterAttribute(EZB_ZCL_CLUSTER_ID_TIME, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_TIME_TIME_STATUS_ID, &_time_status, false);

    return *timeinfo;
  } else {
    log_e("Error while converting time");
    return tm();
  }
}

int32_t ZigbeeEP::getTimezone(uint8_t endpoint, int32_t short_addr, const uint8_t *ieee_addr) {
  /* Read peer timezone */
  ezb_zcl_read_attr_cmd_t read_req;
  memset(&read_req, 0, sizeof(read_req));

  if (short_addr >= 0) {
    ezb_address_set_short(&read_req.cmd_ctrl.dst_addr, (uint16_t)short_addr);
  } else {
    ezb_extaddr_t ext;
    memcpy(ext.u8, ieee_addr, sizeof(ext.u8));
    ezb_address_set_extended(&read_req.cmd_ctrl.dst_addr, ext);
  }

  uint16_t attributes[] = {EZB_ZCL_ATTR_TIME_TIME_ZONE_ID};
  read_req.payload.attr_number = ZB_ARRAY_LENGHT(attributes);
  read_req.payload.attr_field = attributes;

  read_req.cmd_ctrl.cluster_id = EZB_ZCL_CLUSTER_ID_TIME;

  read_req.cmd_ctrl.dst_ep = endpoint;
  read_req.cmd_ctrl.src_ep = _endpoint;

  // clear read timezone
  _read_timezone = 0;

  log_v("Reading timezone from endpoint %u", endpoint);
  if (!readClusterAttribute(&read_req)) {
    return 0;
  }

  //Wait for response or timeout
  if (xSemaphoreTake(lock, ZB_CMD_TIMEOUT) != pdTRUE) {
    log_e("Error while reading timezone");
    return 0;
  }
  setTimezone(_read_timezone);
  return _read_timezone;
}

void ZigbeeEP::zbReadTimeCluster(const ezb_zcl_attribute_t *attribute) {
  /* Time cluster attributes */
  if (attribute->id == EZB_ZCL_ATTR_TIME_TIME_ID && attribute->data.type == EZB_ZCL_ATTR_TYPE_UTC) {
    log_v("Time attribute received");
    log_v("Time: %" PRIu32, *(uint32_t *)attribute->data.value);
    _read_time = *(uint32_t *)attribute->data.value;
    xSemaphoreGive(lock);
  } else if (attribute->id == EZB_ZCL_ATTR_TIME_TIME_ZONE_ID && attribute->data.type == EZB_ZCL_ATTR_TYPE_INT32) {
    log_v("Timezone attribute received");
    log_v("Timezone: %" PRId32, *(int32_t *)attribute->data.value);
    _read_timezone = *(int32_t *)attribute->data.value;
    xSemaphoreGive(lock);
  }
}

bool ZigbeeEP::addOTAClient(
  uint32_t file_version, uint32_t downloaded_file_ver, uint16_t hw_version, uint16_t manufacturer, uint16_t image_type, uint8_t max_data_size
) {
  // NOTE(zb-v2): The OTA Upgrade cluster client is now described by ezb_zcl_ota_upgrade_cluster_client_config_t
  // (UpgradeServerID/FileOffset/ImageUpgradeStatus/ManufacturerID/ImageTypeID). The v1
  // esp_zb_zcl_ota_upgrade_client_variable_t (timer_query/hw_version/max_data_size) and the explicit
  // server address/endpoint attributes no longer exist; those runtime parameters are managed by the OTA
  // client itself in v2.x. hw_version/max_data_size are accepted for API compatibility but unused here.
  (void)hw_version;
  (void)max_data_size;

  ezb_zcl_ota_upgrade_cluster_client_config_t ota_cfg = {};
  ota_cfg.upgrade_server_id = EZB_ZCL_OTA_UPGRADE_UPGRADE_SERVER_ID_DEFAULT_VALUE;
  ota_cfg.file_offset = EZB_ZCL_OTA_UPGRADE_FILE_OFFSET_DEFAULT_VALUE;
  ota_cfg.image_upgrade_status = EZB_ZCL_OTA_UPGRADE_IMAGE_UPGRADE_STATUS_NORMAL;
  ota_cfg.manufacturer_id = manufacturer;
  ota_cfg.image_type_id = image_type;

  ezb_zcl_cluster_desc_t ota_cluster = ezb_zcl_ota_upgrade_create_cluster_desc(&ota_cfg, EZB_ZCL_CLUSTER_CLIENT);
  if (ota_cluster == nullptr) {
    log_e("Failed to create OTA upgrade cluster descriptor");
    return false;
  }

  ezb_err_t ret = ezb_zcl_ota_upgrade_cluster_desc_add_attr(ota_cluster, EZB_ZCL_ATTR_OTA_UPGRADE_CURRENT_FILE_VERSION_ID, (void *)&file_version);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to add OTA current file version: 0x%x", ret);
    return false;
  }
  ret = ezb_zcl_ota_upgrade_cluster_desc_add_attr(ota_cluster, EZB_ZCL_ATTR_OTA_UPGRADE_DOWNLOADED_FILE_VERSION_ID, (void *)&downloaded_file_ver);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to add OTA downloaded file version: 0x%x", ret);
    return false;
  }
  ret = ezb_af_endpoint_add_cluster_desc(_ep_desc, ota_cluster);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to add OTA cluster: 0x%x", ret);
    return false;
  }
  return true;
}

void ZigbeeEP::requestOTAUpdate() {
  // TODO(zb-v2): Reimplement OTA server discovery + image query for v2.x. The v1 flow used
  // esp_zb_zdo_match_cluster() + esp_zb_ota_upgrade_client_query_interval_set()/query_image_req(), which
  // are replaced by the ezb ZDO Match_Desc request (ezbee/zdo/zdo_dev_srv_disc.h) and the v2.x OTA client
  // query API (ezbee/zcl/cluster/ota_upgrade.h). These are not yet wired up.
  log_w("requestOTAUpdate() is not yet implemented for ESP-ZIGBEE-SDK v2.x");
}

void ZigbeeEP::removeBoundDevice(uint8_t endpoint, const uint8_t *ieee_addr) {
  log_d(
    "Attempting to remove device with endpoint %u and IEEE address %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x", endpoint, ieee_addr[7], ieee_addr[6], ieee_addr[5],
    ieee_addr[4], ieee_addr[3], ieee_addr[2], ieee_addr[1], ieee_addr[0]
  );

  for (std::list<zb_device_params_t *>::iterator it = _bound_devices.begin(); it != _bound_devices.end(); ++it) {
    if ((*it)->endpoint == endpoint && memcmp((*it)->ieee_addr, ieee_addr, sizeof((*it)->ieee_addr)) == 0) {
      log_d("Found matching device, removing it");
      _bound_devices.erase(it);
      if (_bound_devices.empty()) {
        _is_bound = false;
      }
      return;
    }
  }
  log_w("No matching device found for removal");
}

void ZigbeeEP::removeBoundDevice(zb_device_params_t *device) {
  if (!device) {
    log_e("Invalid device parameters provided");
    return;
  }

  log_d(
    "Attempting to remove device with endpoint %u, short address 0x%x, IEEE address %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x", device->endpoint,
    device->short_addr, device->ieee_addr[7], device->ieee_addr[6], device->ieee_addr[5], device->ieee_addr[4], device->ieee_addr[3], device->ieee_addr[2],
    device->ieee_addr[1], device->ieee_addr[0]
  );

  for (std::list<zb_device_params_t *>::iterator it = _bound_devices.begin(); it != _bound_devices.end(); ++it) {
    bool endpoint_matches = ((*it)->endpoint == device->endpoint);
    bool short_addr_matches = (device->short_addr != 0xFFFF && (*it)->short_addr == device->short_addr);
    bool ieee_addr_matches = (memcmp((*it)->ieee_addr, device->ieee_addr, sizeof((*it)->ieee_addr)) == 0);

    if (endpoint_matches && (short_addr_matches || ieee_addr_matches)) {
      log_d("Found matching device by %s, removing it", short_addr_matches ? "short address" : "IEEE address");
      _bound_devices.erase(it);
      if (_bound_devices.empty()) {
        _is_bound = false;
      }
      return;
    }
  }
  log_w("No matching device found for removal");
}

void ZigbeeEP::zbDefaultResponse(const ezb_zcl_cmd_default_rsp_message_t *message) {
  log_v("Default response received for endpoint %u", _endpoint);
  log_v("Status code: %s", esp_zb_zcl_status_to_name(message->in.status_code));
  log_v("Response to command: %u", message->in.rsp_to_cmd);
  if (_on_default_response) {
    _on_default_response((zb_cmd_type_t)message->in.rsp_to_cmd, message->in.status_code);
  }
}

void ZigbeeEP::addPrivilegeCommand(uint16_t cluster_id, uint16_t command_id) {
  // TODO(zb-v2): esp_zb_zcl_add_privilege_command() has no v2.x equivalent. Command interception is now
  // done through the manufacturer-specific command callback (EZB_ZCL_CORE_MANUF_SPEC_CMD_CB_ID) and/or
  // the raw frame handler (ezb_zcl_raw_command_handler_register()). This needs a redesign in ZigbeeHandlers.
  (void)cluster_id;
  (void)command_id;
  log_w("addPrivilegeCommand() is not yet implemented for ESP-ZIGBEE-SDK v2.x");
}

void ZigbeeEP::zbPrivilegeCommand(const ezb_zcl_manuf_spec_cmd_message_t *message) {
  if (_on_privilege_command) {
    _on_privilege_command(message);
  }
}

void ZigbeeEP::zbCustomClusterCommand(const ezb_zcl_manuf_spec_cmd_message_t *message) {
  if (_on_custom_cluster_command) {
    _on_custom_cluster_command(message);
  }
}

// Global function implementation
// NOTE(zb-v2): The v2.x ZCL status set (ezb_zcl_status_e) is smaller than v1; statuses that no longer
// exist (e.g. UNSUP_GEN_CMD, UNSUP_MANUF_*, DUPE_EXISTS, WRITE_ONLY, HW/SW_FAIL, LIMIT_REACHED) were
// dropped, and a few were renamed (INSUFF_SPACE -> INSUFFICIENT_SPACE, UNREPORTABLE_ATTRIB ->
// UNREPORTBLE_ATTRIB, CALIB_ERR -> CALIBRATION_ERROR, UNSUP_CLUST -> UNSUPPORTED_CLUSTER).
const char *esp_zb_zcl_status_to_name(ezb_zcl_status_t status) {
  switch (status) {
    case EZB_ZCL_STATUS_SUCCESS:              return "Success";
    case EZB_ZCL_STATUS_FAIL:                 return "Fail";
    case EZB_ZCL_STATUS_NOT_AUTHORIZED:       return "Not authorized";
    case EZB_ZCL_STATUS_MALFORMED_CMD:        return "Malformed command";
    case EZB_ZCL_STATUS_UNSUP_CMD:            return "Unsupported command";
    case EZB_ZCL_STATUS_INVALID_FIELD:        return "Invalid field";
    case EZB_ZCL_STATUS_UNSUP_ATTRIB:         return "Unsupported attribute";
    case EZB_ZCL_STATUS_INVALID_VALUE:        return "Invalid value";
    case EZB_ZCL_STATUS_READ_ONLY:            return "Read only";
    case EZB_ZCL_STATUS_INSUFFICIENT_SPACE:   return "Insufficient space";
    case EZB_ZCL_STATUS_NOT_FOUND:            return "Not found";
    case EZB_ZCL_STATUS_UNREPORTBLE_ATTRIB:   return "Unreportable attribute";
    case EZB_ZCL_STATUS_INVALID_TYPE:         return "Invalid type";
    case EZB_ZCL_STATUS_INCONSISTENT:         return "Inconsistent";
    case EZB_ZCL_STATUS_ACTION_DENIED:        return "Action denied";
    case EZB_ZCL_STATUS_TIMEOUT:              return "Timeout";
    case EZB_ZCL_STATUS_ABORT:                return "Abort";
    case EZB_ZCL_STATUS_INVALID_IMAGE:        return "Invalid OTA upgrade image";
    case EZB_ZCL_STATUS_WAIT_FOR_DATA:        return "Server does not have data block available yet";
    case EZB_ZCL_STATUS_NO_IMAGE_AVAILABLE:   return "No image available";
    case EZB_ZCL_STATUS_REQUIRE_MORE_IMAGE:   return "Require more image";
    case EZB_ZCL_STATUS_NOTIFICATION_PENDING: return "Notification pending";
    case EZB_ZCL_STATUS_CALIBRATION_ERROR:    return "Calibration error";
    case EZB_ZCL_STATUS_UNSUPPORTED_CLUSTER:  return "Cluster is not found on the target endpoint";
    default:                                  return "Unknown status";
  }
}

#endif  // CONFIG_ZB_ENABLED
