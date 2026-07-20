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
#include "ZigbeeCore.h"

#if CONFIG_ZB_ENABLED

#include "esp_zigbee.h"
#include "ezbee/zcl/zcl_desc.h"
#include "ezbee/zcl/cluster/time.h"

/* ZigBee ZCL UTCTime: seconds since 2000-01-01 00:00:00 UTC (not Unix 1970 epoch). */
static constexpr int64_t ZIGBEE_UTCTIME_UNIX_OFFSET_SEC = 946684800LL;
static constexpr uint32_t ZIGBEE_UTCTIME_INVALID = UINT32_MAX;

/* Time cluster server interface (SDK v2.x): the Time attribute (0x0000) is no longer served from stored
 * attribute memory — the server reads it through a registered interface callback (see
 * zcl_time_cluster_read_time() in the SDK, which returns ZCL_STATUS_NOT_FOUND when no interface is set).
 * A single device-wide wall clock backs all Time server endpoints; it is anchored to a millis() baseline
 * so the served time advances between updates. The clock is updated automatically whenever the Time
 * attribute is written (the server's write hook calls set_utc_time). */
static uint32_t s_time_server_utc_base = 0;  // ZCL UTCTime (s since 2000-01-01) captured at the anchor
static uint32_t s_time_server_ms_base = 0;   // millis() captured at the anchor
static bool s_time_server_clock_valid = false;

static uint32_t zb_time_server_get_utc_time(void) {
  if (!s_time_server_clock_valid) {
    return ZIGBEE_UTCTIME_INVALID;
  }
  return s_time_server_utc_base + (uint32_t)((millis() - s_time_server_ms_base) / 1000U);
}

static void zb_time_server_set_utc_time(uint32_t utc_time) {
  s_time_server_utc_base = utc_time;
  s_time_server_ms_base = millis();
  s_time_server_clock_valid = (utc_time != ZIGBEE_UTCTIME_INVALID);
}

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
  _app_version = 0;
  _hw_version = 0;
  _power_config_cfg = {
    .mains_voltage = EZB_ZCL_POWER_CONFIG_MAINS_VOLTAGE_DEFAULT_VALUE,
    .mains_voltage_min_threshold = EZB_ZCL_POWER_CONFIG_MAINS_VOLTAGE_MIN_THRESHOLD_DEFAULT_VALUE,
    .mains_voltage_max_threshold = EZB_ZCL_POWER_CONFIG_MAINS_VOLTAGE_MAX_THRESHOLD_DEFAULT_VALUE,
  };
  _battery_percentage = 0;
  _battery_voltage = 0xff;
  _time_server_cfg = {
    .time = 0,
    .time_status = EZB_ZCL_TIME_TIME_STATUS_DEFAULT_VALUE,
  };
  _time_gmt_offset = EZB_ZCL_TIME_TIME_ZONE_DEFAULT_VALUE;
  _ota_client_cfg = {
    .upgrade_server_id = EZB_ZCL_OTA_UPGRADE_UPGRADE_SERVER_ID_DEFAULT_VALUE,
    .file_offset = EZB_ZCL_OTA_UPGRADE_FILE_OFFSET_DEFAULT_VALUE,
    .image_upgrade_status = EZB_ZCL_OTA_UPGRADE_IMAGE_UPGRADE_STATUS_DEFAULT_VALUE,
    .manufacturer_id = 0,
    .image_type_id = 0,
  };
  _ota_current_file_version = 0;
  _ota_downloaded_file_version = 0;
  _on_identify = nullptr;
  _on_ota_state_change = nullptr;
  _on_default_response = nullptr;
  _on_privilege_command = nullptr;
  _on_custom_cluster_command = nullptr;
  _read_model = NULL;
  _read_manufacturer = NULL;
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
  if (!Zigbee.stackRunning()) {
    log_w("Cannot report attribute: Zigbee stack not running");
    return false;
  }
  if (Zigbee.paused()) {
    log_w("Cannot report attribute: Zigbee stack is paused");
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
  if (!Zigbee.stackRunning()) {
    log_w("Cannot read attribute: Zigbee stack not running");
    return false;
  }
  if (Zigbee.paused()) {
    log_w("Cannot read attribute: Zigbee stack is paused");
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
  if (!Zigbee.stackRunning() || Zigbee.paused()) {
    log_w("Cannot configure cluster reporting: Zigbee stack not running");
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

bool ZigbeeEP::requireBeforeAddEndpoint(const char *api) const {
  if (Zigbee.endpointsRegistered()) {
    log_e("%s() must be called before Zigbee.addEndpoint()", api);
    return false;
  }
  if (_ep_desc == nullptr) {
    log_e("%s(): endpoint descriptor not created", api);
    return false;
  }
  if (!Zigbee.initialized()) {
    log_e("%s(): Zigbee.role() must be called before configuring endpoints", api);
    return false;
  }
  return true;
}

ezb_zcl_cluster_desc_t ZigbeeEP::getEpClusterDesc(uint16_t cluster_id, uint8_t cluster_role) const {
  if (_ep_desc == nullptr) {
    return nullptr;
  }
  return ezb_af_endpoint_get_cluster_desc(_ep_desc, cluster_id, cluster_role);
}

ezb_zcl_attr_desc_t ZigbeeEP::getEpClusterAttrDesc(uint16_t cluster_id, uint8_t cluster_role, uint16_t attr_id) const {
  ezb_zcl_cluster_desc_t cluster = getEpClusterDesc(cluster_id, cluster_role);
  if (cluster == nullptr) {
    return nullptr;
  }
  return ezb_zcl_cluster_get_attr_desc(cluster, attr_id, EZB_ZCL_STD_MANUF_CODE);
}

bool ZigbeeEP::setEpClusterAttrValue(uint16_t cluster_id, uint8_t cluster_role, uint16_t attr_id, const void *value) const {
  if (value == nullptr) {
    return false;
  }
  ezb_zcl_attr_desc_t attr = getEpClusterAttrDesc(cluster_id, cluster_role, attr_id);
  if (attr == nullptr) {
    return false;
  }
  return ezb_zcl_attr_desc_set_value(attr, value) == EZB_ERR_NONE;
}

bool ZigbeeEP::addOrSetEpClusterAttr(
  uint16_t cluster_id, uint8_t cluster_role, uint16_t attr_id, const void *value, ep_cluster_desc_add_attr_fn add_attr
) const {
  if (value == nullptr || add_attr == nullptr) {
    return false;
  }
  ezb_zcl_cluster_desc_t cluster = getEpClusterDesc(cluster_id, cluster_role);
  if (cluster == nullptr) {
    return false;
  }
  ezb_zcl_attr_desc_t attr = ezb_zcl_cluster_get_attr_desc(cluster, attr_id, EZB_ZCL_STD_MANUF_CODE);
  if (attr != nullptr) {
    return ezb_zcl_attr_desc_set_value(attr, value) == EZB_ERR_NONE;
  }
  return add_attr(cluster, attr_id, value) == EZB_ERR_NONE;
}

bool ZigbeeEP::configureEpClusterAttr(
  const char *api, uint16_t cluster_id, uint8_t cluster_role, uint16_t attr_id, void *value, ep_cluster_desc_add_attr_fn add_attr
) {
  if (value == nullptr) {
    return false;
  }
  if (Zigbee.endpointsRegistered()) {
    return setClusterAttribute(cluster_id, cluster_role, attr_id, value, false) == EZB_ZCL_STATUS_SUCCESS;
  }
  if (!requireBeforeAddEndpoint(api)) {
    return false;
  }
  return addOrSetEpClusterAttr(cluster_id, cluster_role, attr_id, value, add_attr);
}

void ZigbeeEP::setVersion(uint8_t version) {
  if (!requireBeforeAddEndpoint("setVersion")) {
    return;
  }
  _app_version = version;
  _ep_config.app_device_version = version;
  ezb_af_ep_desc_set_app_version(_ep_desc, version);
  ezb_zcl_cluster_desc_t basic_cluster = ezb_af_endpoint_get_cluster_desc(_ep_desc, EZB_ZCL_CLUSTER_ID_BASIC, EZB_ZCL_CLUSTER_SERVER);
  if (basic_cluster == nullptr) {
    log_e("Basic cluster not found");
    return;
  }
  if (ezb_zcl_basic_cluster_desc_add_attr(basic_cluster, EZB_ZCL_ATTR_BASIC_APPLICATION_VERSION_ID, (void *)&_app_version) != EZB_ERR_NONE) {
    log_e("Failed to set application version");
  }
}

void ZigbeeEP::setHardwareVersion(uint8_t version) {
  if (!requireBeforeAddEndpoint("setHardwareVersion")) {
    return;
  }
  _hw_version = version;
  ezb_zcl_cluster_desc_t basic_cluster = ezb_af_endpoint_get_cluster_desc(_ep_desc, EZB_ZCL_CLUSTER_ID_BASIC, EZB_ZCL_CLUSTER_SERVER);
  if (basic_cluster == nullptr) {
    log_e("Basic cluster not found");
    return;
  }
  if (ezb_zcl_basic_cluster_desc_add_attr(basic_cluster, EZB_ZCL_ATTR_BASIC_HW_VERSION_ID, (void *)&_hw_version) != EZB_ERR_NONE) {
    log_e("Failed to set hardware version");
  }
}

bool ZigbeeEP::setManufacturerAndModel(const char *name, const char *model) {
  if (!requireBeforeAddEndpoint("setManufacturerAndModel")) {
    return false;
  }
  size_t name_length = strlen(name);
  size_t model_length = strlen(model);
  if (name_length > ZB_MAX_NAME_LENGTH || model_length > ZB_MAX_NAME_LENGTH) {
    log_e("Manufacturer or model name is too long");
    return false;
  }

  _zb_manufacturer[0] = static_cast<char>(name_length);
  _zb_model[0] = static_cast<char>(model_length);
  memcpy(_zb_manufacturer + 1, name, name_length);
  memcpy(_zb_model + 1, model, model_length);
  _zb_manufacturer[name_length + 1] = '\0';
  _zb_model[model_length + 1] = '\0';

  ezb_zcl_cluster_desc_t basic_cluster = ezb_af_endpoint_get_cluster_desc(_ep_desc, EZB_ZCL_CLUSTER_ID_BASIC, EZB_ZCL_CLUSTER_SERVER);
  if (basic_cluster == nullptr) {
    log_e("Basic cluster not found");
    return false;
  }
  ezb_err_t ret = ezb_zcl_basic_cluster_desc_add_attr(basic_cluster, EZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, (void *)_zb_manufacturer);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to set manufacturer name: 0x%x", ret);
    return false;
  }
  ret = ezb_zcl_basic_cluster_desc_add_attr(basic_cluster, EZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, (void *)_zb_model);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to set model identifier: 0x%x", ret);
    return false;
  }
  return true;
}

bool ZigbeeEP::setPowerSource(zb_power_source_t power_source, uint8_t battery_percentage, uint8_t battery_voltage) {
  if (!requireBeforeAddEndpoint("setPowerSource")) {
    return false;
  }
  _power_source = power_source;
  ezb_zcl_cluster_desc_t basic_cluster = ezb_af_endpoint_get_cluster_desc(_ep_desc, EZB_ZCL_CLUSTER_ID_BASIC, EZB_ZCL_CLUSTER_SERVER);
  if (basic_cluster == nullptr) {
    log_e("Basic cluster not found");
    return false;
  }
  if (ezb_zcl_basic_cluster_desc_add_attr(basic_cluster, EZB_ZCL_ATTR_BASIC_POWER_SOURCE_ID, (void *)&_power_source) != EZB_ERR_NONE) {
    log_e("Failed to set power source");
    return false;
  }

  if (power_source == ZB_POWER_SOURCE_BATTERY) {
    if (battery_percentage > 100) {
      battery_percentage = 100;
    }
    _battery_percentage = battery_percentage * 2;
    _battery_voltage = battery_voltage;
    ezb_zcl_cluster_desc_t power_config_cluster = ezb_zcl_power_config_create_cluster_desc(&_power_config_cfg, EZB_ZCL_CLUSTER_SERVER);
    ezb_err_t ret = ezb_zcl_power_config_cluster_desc_add_attr(
      power_config_cluster, EZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID, (void *)&_battery_percentage
    );
    if (ret == EZB_ERR_NONE) {
      ret = ezb_zcl_power_config_cluster_desc_add_attr(
        power_config_cluster, EZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID, (void *)&_battery_voltage
      );
    }
    if (ret == EZB_ERR_NONE) {
      ret = ezb_af_endpoint_add_cluster_desc(_ep_desc, power_config_cluster);
    }
    if (ret != EZB_ERR_NONE) {
      log_e("Failed to add power config cluster: 0x%x", ret);
      return false;
    }
  }
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
  _battery_percentage = percentage;
  ret = setClusterAttribute(
    EZB_ZCL_CLUSTER_ID_POWER_CONFIG, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID, &_battery_percentage, false
  );
  if (ret != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set battery percentage: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  log_v("Battery percentage updated");
  return true;
}

bool ZigbeeEP::setBatteryVoltage(uint8_t voltage) {
  _battery_voltage = voltage;
  ezb_zcl_status_t ret = setClusterAttribute(
    EZB_ZCL_CLUSTER_ID_POWER_CONFIG, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID, &_battery_voltage, false
  );
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
  if (!requireBeforeAddEndpoint("addTimeCluster")) {
    return false;
  }
  if (ezb_af_endpoint_get_cluster_desc(_ep_desc, EZB_ZCL_CLUSTER_ID_TIME, EZB_ZCL_CLUSTER_SERVER) != nullptr) {
    log_w("Time cluster already added");
    return true;
  }

  _time_server_cfg.time = 0;
  _time_server_cfg.time_status = EZB_ZCL_TIME_TIME_STATUS_DEFAULT_VALUE;
  if (time.tm_year > 0) {
    time_t unix_ts = mktime(&time);
    if (unix_ts == (time_t)-1) {
      log_e("Invalid calendar time");
      return false;
    }
    _time_server_cfg.time = zb_utctime_from_unix(unix_ts);
    // Seed the device-wide wall clock so the Time attribute (0x0000) is served once the interface is
    // registered (after the stack starts). Without a provided time the clock stays invalid until the
    // Time attribute is written (e.g. after getTime() synchronizes from another time server).
    zb_time_server_set_utc_time(_time_server_cfg.time);
  }
  _time_gmt_offset = gmt_offset;

  ezb_zcl_cluster_desc_t time_cluster_server = ezb_zcl_time_create_cluster_desc(&_time_server_cfg, EZB_ZCL_CLUSTER_SERVER);
  ezb_err_t ret = ezb_zcl_time_cluster_desc_add_attr(time_cluster_server, EZB_ZCL_ATTR_TIME_TIME_ZONE_ID, (void *)&_time_gmt_offset);
  if (ret == EZB_ERR_NONE) {
    ret = ezb_af_endpoint_add_cluster_desc(_ep_desc, time_cluster_server);
  }
  if (ret == EZB_ERR_NONE) {
    ezb_zcl_cluster_desc_t time_cluster_client = ezb_zcl_time_create_cluster_desc(nullptr, EZB_ZCL_CLUSTER_CLIENT);
    ret = ezb_af_endpoint_add_cluster_desc(_ep_desc, time_cluster_client);
  }
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to add time cluster: 0x%x", ret);
    return false;
  }
  _time_server = true;
  return true;
}

bool ZigbeeEP::registerTimeServer() {
  // v2.x serves the Time attribute (0x0000) through this interface; the per-endpoint time context is
  // created when the stack starts, so this must be called after Zigbee.begin().
  if (!_time_server) {
    log_e("registerTimeServer: no Time cluster on this endpoint, call addTimeCluster() first");
    return false;
  }
  if (_time_server_registered) {
    return true;
  }
  ezb_zcl_time_interface_t interface = {
    .get_utc_time = zb_time_server_get_utc_time,
    .set_utc_time = zb_time_server_set_utc_time,
  };
  ezb_err_t ret = ezb_zcl_time_server_interface_register(_endpoint, interface);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to register time server on endpoint %u: 0x%x (call after Zigbee.begin())", _endpoint, ret);
    return false;
  }
  _time_server_registered = true;
  return true;
}

bool ZigbeeEP::setTime(tm time) {
  // Writing the Time attribute (0x0000) goes through the SDK write hook, which requires the time
  // server interface to be registered. Guard here so a missing registerTimeServer() reports an error
  // instead of triggering an SDK assert (also protects getTime()'s internal self-update).
  if (!_time_server_registered) {
    log_e("Time server not registered, call registerTimeServer() after Zigbee.begin() first");
    return false;
  }
  ezb_zcl_status_t ret = EZB_ZCL_STATUS_SUCCESS;
  time_t unix_ts = mktime(&time);
  if (unix_ts == (time_t)-1) {
    log_e("Invalid calendar time");
    return false;
  }
  _time_server_cfg.time = zb_utctime_from_unix(unix_ts);
  log_d("Setting ZCL UTCTime to %" PRIu32 " s since 2000-01-01 UTC", _time_server_cfg.time);
  ret = setClusterAttribute(
    EZB_ZCL_CLUSTER_ID_TIME, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_TIME_TIME_ID, &_time_server_cfg.time, false
  );
  if (ret != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set time: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeEP::setTimezone(int32_t gmt_offset) {
  ezb_zcl_status_t ret = EZB_ZCL_STATUS_SUCCESS;
  _time_gmt_offset = gmt_offset;
  log_d("Setting timezone to %" PRId32, _time_gmt_offset);
  ret = setClusterAttribute(
    EZB_ZCL_CLUSTER_ID_TIME, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_TIME_TIME_ZONE_ID, &_time_gmt_offset, false
  );
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
    _time_server_cfg.time_status |= EZB_ZCL_TIME_TIME_STATUS_SYNCHRONIZED;
    setClusterAttribute(
      EZB_ZCL_CLUSTER_ID_TIME, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_TIME_TIME_STATUS_ID, &_time_server_cfg.time_status, false
    );

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
  (void)hw_version;
  (void)max_data_size;
  if (!requireBeforeAddEndpoint("addOTAClient")) {
    return false;
  }
  if (ezb_af_endpoint_get_cluster_desc(_ep_desc, EZB_ZCL_CLUSTER_ID_OTA_UPGRADE, EZB_ZCL_CLUSTER_CLIENT) != nullptr) {
    log_w("OTA client cluster already added");
    return true;
  }

  _ota_client_cfg.upgrade_server_id = EZB_ZCL_OTA_UPGRADE_UPGRADE_SERVER_ID_DEFAULT_VALUE;
  _ota_client_cfg.file_offset = EZB_ZCL_OTA_UPGRADE_FILE_OFFSET_DEFAULT_VALUE;
  _ota_client_cfg.image_upgrade_status = EZB_ZCL_OTA_UPGRADE_IMAGE_UPGRADE_STATUS_NORMAL;
  _ota_client_cfg.manufacturer_id = manufacturer;
  _ota_client_cfg.image_type_id = image_type;
  _ota_current_file_version = file_version;
  _ota_downloaded_file_version = downloaded_file_ver;

  ezb_zcl_cluster_desc_t ota_cluster = ezb_zcl_ota_upgrade_create_cluster_desc(&_ota_client_cfg, EZB_ZCL_CLUSTER_CLIENT);
  if (ota_cluster == nullptr) {
    log_e("Failed to create OTA cluster");
    return false;
  }
  ezb_err_t ret = ezb_zcl_ota_upgrade_cluster_desc_add_attr(
    ota_cluster, EZB_ZCL_ATTR_OTA_UPGRADE_CURRENT_FILE_VERSION_ID, (void *)&_ota_current_file_version
  );
  if (ret == EZB_ERR_NONE) {
    ret = ezb_zcl_ota_upgrade_cluster_desc_add_attr(
      ota_cluster, EZB_ZCL_ATTR_OTA_UPGRADE_DOWNLOADED_FILE_VERSION_ID, (void *)&_ota_downloaded_file_version
    );
  }
  if (ret == EZB_ERR_NONE) {
    ret = ezb_af_endpoint_add_cluster_desc(_ep_desc, ota_cluster);
  }
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

bool ZigbeeEP::sendIASZoneEnrollResponse(
  uint16_t dst_short_addr, uint8_t dst_endpoint, uint8_t zone_id, esp_zb_zcl_ias_zone_enroll_response_code_t response_code
) {
  esp_zb_zcl_ias_zone_enroll_response_cmd_t cmd_resp;
  memset(&cmd_resp, 0, sizeof(cmd_resp));
  cmd_resp.zcl_basic_cmd.dst_addr_u.addr_short = dst_short_addr;
  cmd_resp.zcl_basic_cmd.dst_endpoint = dst_endpoint;
  cmd_resp.zcl_basic_cmd.src_endpoint = _endpoint;
  cmd_resp.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
  cmd_resp.enroll_rsp_code = (uint8_t)response_code;
  cmd_resp.zone_id = zone_id;

  if (!acquireCommandLock()) {
    return false;
  }
  esp_zb_zcl_ias_zone_enroll_cmd_resp(&cmd_resp);
  releaseCommandLock();
  log_v("IAS Zone Enroll Response sent to 0x%04x endpoint %u (zone id %u, code %u)", dst_short_addr, dst_endpoint, zone_id, (uint8_t)response_code);
  return true;
}

void ZigbeeEP::zbPrivilegeCommand(const esp_zb_zcl_privilege_command_message_t *message) {
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
