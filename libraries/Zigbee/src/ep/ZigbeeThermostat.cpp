#include "ZigbeeThermostat.h"
#if CONFIG_ZB_ENABLED

static float zb_s16_to_temperature(int16_t value) {
  return 1.0 * value / 100;
}

// Initialize the static instance of the class
ZigbeeThermostat *ZigbeeThermostat::_instance = nullptr;

ZigbeeThermostat::ZigbeeThermostat(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = ESP_ZB_HA_THERMOSTAT_DEVICE_ID;
  _instance = this;   // Set the static pointer to this instance
  _device = nullptr;  // Initialize sensor pointer to null

  //use custom config to avoid narrowing error -> must be fixed in zigbee-sdk
  esp_zb_thermostat_cfg_t thermostat_cfg = ZB_DEFAULT_THERMOSTAT_CONFIG();

  //use custom cluster creating to accept reportings from temperature sensor
  _cluster_list = esp_zb_zcl_cluster_list_create();
  esp_zb_attribute_list_t *basic_cluster = esp_zb_basic_cluster_create(&(thermostat_cfg.basic_cfg));
  esp_zb_cluster_list_add_basic_cluster(_cluster_list, basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_identify_cluster(_cluster_list, esp_zb_identify_cluster_create(&(thermostat_cfg.identify_cfg)), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_identify_cluster(_cluster_list, esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY), ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE);
  esp_zb_cluster_list_add_thermostat_cluster(_cluster_list, esp_zb_thermostat_cluster_create(&(thermostat_cfg.thermostat_cfg)), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  /* Add temperature measurement cluster for attribute reporting */
  esp_zb_cluster_list_add_temperature_meas_cluster(_cluster_list, esp_zb_temperature_meas_cluster_create(NULL), ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE);

  _ep_config = {.endpoint = _endpoint, .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID, .app_device_id = ESP_ZB_HA_THERMOSTAT_DEVICE_ID, .app_device_version = 0};
}

void ZigbeeThermostat::bindCb(esp_zb_zdp_status_t zdo_status, void *user_ctx) {
  ZigbeeThermostat *instance = static_cast<ZigbeeThermostat *>(user_ctx);
  if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
    log_i("Bound successfully!");
    if (instance->_device) {
      zb_device_params_t *sensor = (zb_device_params_t *)instance->_device;
      log_i("The sensor originating from address(0x%x) on endpoint(%d)", sensor->short_addr, sensor->endpoint);
      log_d("Sensor bound to thermostat on EP %d", instance->_endpoint);
      instance->_bound_devices.push_back(sensor);
    }
    instance->_is_bound = true;
  } else {
    instance->_device = nullptr;
  }
}

void ZigbeeThermostat::bindCbWrapper(esp_zb_zdp_status_t zdo_status, void *user_ctx) {
  ZigbeeThermostat *instance = static_cast<ZigbeeThermostat *>(user_ctx);
  if (instance) {
    log_d("bindCbWrapper on EP %d", instance->_endpoint);
    instance->bindCb(zdo_status, user_ctx);
  }
}

void ZigbeeThermostat::findCbWrapper(esp_zb_zdp_status_t zdo_status, uint16_t addr, uint8_t endpoint, void *user_ctx) {
  ZigbeeThermostat *instance = static_cast<ZigbeeThermostat *>(user_ctx);
  if (instance) {
    log_d("findCbWrapper on EP %d", instance->_endpoint);
    instance->findCb(zdo_status, addr, endpoint, user_ctx);
  }
}

void ZigbeeThermostat::findCb(esp_zb_zdp_status_t zdo_status, uint16_t addr, uint8_t endpoint, void *user_ctx) {
  ZigbeeThermostat *instance = static_cast<ZigbeeThermostat *>(user_ctx);
  if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
    log_i("Found temperature sensor");
    esp_zb_zdo_bind_req_param_t bind_req = {0};
    /* Store the information of the remote device */
    zb_device_params_t *sensor = (zb_device_params_t *)malloc(sizeof(zb_device_params_t));
    sensor->endpoint = endpoint;
    sensor->short_addr = addr;
    esp_zb_ieee_address_by_short(sensor->short_addr, sensor->ieee_addr);
    log_d("Temperature sensor found: short address(0x%x), endpoint(%d)", sensor->short_addr, sensor->endpoint);

    /* 1. Send binding request to the sensor */
    bind_req.req_dst_addr = addr;
    memcpy(bind_req.src_address, sensor->ieee_addr, sizeof(esp_zb_ieee_addr_t));
    bind_req.src_endp = endpoint;
    bind_req.cluster_id = ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT;
    bind_req.dst_addr_mode = ESP_ZB_ZDO_BIND_DST_ADDR_MODE_64_BIT_EXTENDED;
    esp_zb_get_long_address(bind_req.dst_address_u.addr_long);
    bind_req.dst_endp = instance->_endpoint;

    log_i("Request temperature sensor to bind us");
    esp_zb_zdo_device_bind_req(&bind_req, ZigbeeThermostat::bindCbWrapper, NULL);

    /* 2. Send binding request to self */
    bind_req.req_dst_addr = esp_zb_get_short_address();

    /* populate the src information of the binding */
    esp_zb_get_long_address(bind_req.src_address);
    bind_req.src_endp = instance->_endpoint;
    bind_req.cluster_id = ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT;
    bind_req.dst_addr_mode = ESP_ZB_ZDO_BIND_DST_ADDR_MODE_64_BIT_EXTENDED;
    memcpy(bind_req.dst_address_u.addr_long, sensor->ieee_addr, sizeof(esp_zb_ieee_addr_t));
    bind_req.dst_endp = endpoint;
    log_i("Try to bind Temperature Measurement");
    //save sensor params in the class
    instance->_device = sensor;

    log_d("Find callback on EP %d", instance->_endpoint);
    esp_zb_zdo_device_bind_req(&bind_req, ZigbeeThermostat::bindCbWrapper, this);
  } else {
    log_d("No temperature sensor endpoint found");
  }
}

void ZigbeeThermostat::findEndpoint(esp_zb_zdo_match_desc_req_param_t *param) {
  uint16_t cluster_list[] = {ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT};
  param->profile_id = ESP_ZB_AF_HA_PROFILE_ID;
  param->num_in_clusters = 1;
  param->num_out_clusters = 0;
  param->cluster_list = cluster_list;
  esp_zb_zdo_match_cluster(param, ZigbeeThermostat::findCbWrapper, this);
}

void ZigbeeThermostat::zbAttributeRead(uint16_t cluster_id, const esp_zb_zcl_attribute_t *attribute, uint8_t src_endpoint, esp_zb_zcl_addr_t src_address) {
  static uint8_t read_config = 0;
  if (cluster_id == ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT) {
    if (attribute->id == ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID && attribute->data.type == ESP_ZB_ZCL_ATTR_TYPE_S16) {
      int16_t value = attribute->data.value ? *(int16_t *)attribute->data.value : 0;
      if (_on_temp_receive) {
        _on_temp_receive(zb_s16_to_temperature(value));
      }
      if (_on_temp_receive_with_source) {
        _on_temp_receive_with_source(zb_s16_to_temperature(value), src_endpoint, src_address);
      }
    }
    if (attribute->id == ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_MIN_VALUE_ID && attribute->data.type == ESP_ZB_ZCL_ATTR_TYPE_S16) {
      int16_t min_value = attribute->data.value ? *(int16_t *)attribute->data.value : 0;
      _min_temp = zb_s16_to_temperature(min_value);
      read_config++;
      log_d("Received min temperature: %.2f°C from endpoint %d", _min_temp, src_endpoint);
    }
    if (attribute->id == ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_MAX_VALUE_ID && attribute->data.type == ESP_ZB_ZCL_ATTR_TYPE_S16) {
      int16_t max_value = attribute->data.value ? *(int16_t *)attribute->data.value : 0;
      _max_temp = zb_s16_to_temperature(max_value);
      read_config++;
      log_d("Received max temperature: %.2f°C from endpoint %d", _max_temp, src_endpoint);
    }
    if (attribute->id == ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_TOLERANCE_ID && attribute->data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
      uint16_t tolerance = attribute->data.value ? *(uint16_t *)attribute->data.value : 0;
      _tolerance = 1.0 * tolerance / 100;
      read_config++;
      log_d("Received tolerance: %.2f°C from endpoint %d", _tolerance, src_endpoint);
    }
    if (read_config == 3) {
      log_d("All config attributes processed");
      read_config = 0;
      xSemaphoreGive(lock);
    }
  }
}

void ZigbeeThermostat::getTemperature() {
  /* Send "read attributes" command to all bound sensors */
  esp_zb_zcl_read_attr_cmd_t read_req = {0};
  read_req.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
  read_req.zcl_basic_cmd.src_endpoint = _endpoint;
  read_req.clusterID = ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT;

  uint16_t attributes[] = {ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID};
  read_req.attr_number = ZB_ARRAY_LENGHT(attributes);
  read_req.attr_field = attributes;

  log_i("Sending 'read temperature' command");
  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_read_attr_cmd_req(&read_req);
  esp_zb_lock_release();
}

void ZigbeeThermostat::getTemperature(uint16_t group_addr) {
  /* Send "read attributes" command to the group */
  esp_zb_zcl_read_attr_cmd_t read_req = {0};
  read_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_GROUP_ENDP_NOT_PRESENT;
  read_req.zcl_basic_cmd.src_endpoint = _endpoint;
  read_req.zcl_basic_cmd.dst_addr_u.addr_short = group_addr;
  read_req.clusterID = ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT;

  uint16_t attributes[] = {ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID};
  read_req.attr_number = ZB_ARRAY_LENGHT(attributes);
  read_req.attr_field = attributes;

  log_i("Sending 'read temperature' command to group address 0x%x", group_addr);
  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_read_attr_cmd_req(&read_req);
  esp_zb_lock_release();
}

void ZigbeeThermostat::getTemperature(uint8_t endpoint, uint16_t short_addr) {
  /* Send "read attributes" command to specific endpoint */
  esp_zb_zcl_read_attr_cmd_t read_req = {0};
  read_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
  read_req.zcl_basic_cmd.src_endpoint = _endpoint;
  read_req.zcl_basic_cmd.dst_endpoint = endpoint;
  read_req.zcl_basic_cmd.dst_addr_u.addr_short = short_addr;
  read_req.clusterID = ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT;

  uint16_t attributes[] = {ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID};
  read_req.attr_number = ZB_ARRAY_LENGHT(attributes);
  read_req.attr_field = attributes;

  log_i("Sending 'read temperature' command to endpoint %d, address 0x%x", endpoint, short_addr);
  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_read_attr_cmd_req(&read_req);
  esp_zb_lock_release();
}

void ZigbeeThermostat::getTemperature(uint8_t endpoint, esp_zb_ieee_addr_t ieee_addr) {
  /* Send "read attributes" command to specific endpoint */
  esp_zb_zcl_read_attr_cmd_t read_req = {0};
  read_req.address_mode = ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT;
  read_req.zcl_basic_cmd.src_endpoint = _endpoint;
  read_req.zcl_basic_cmd.dst_endpoint = endpoint;
  read_req.clusterID = ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT;
  memcpy(read_req.zcl_basic_cmd.dst_addr_u.addr_long, ieee_addr, sizeof(esp_zb_ieee_addr_t));

  uint16_t attributes[] = {ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID};
  read_req.attr_number = ZB_ARRAY_LENGHT(attributes);
  read_req.attr_field = attributes;

  log_i(
    "Sending 'read temperature' command to endpoint %d, ieee address %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x", endpoint, ieee_addr[7], ieee_addr[6],
    ieee_addr[5], ieee_addr[4], ieee_addr[3], ieee_addr[2], ieee_addr[1], ieee_addr[0]
  );
  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_read_attr_cmd_req(&read_req);
  esp_zb_lock_release();
}

void ZigbeeThermostat::getSensorSettings() {
  /* Send "read attributes" command to all bound sensors */
  esp_zb_zcl_read_attr_cmd_t read_req = {0};
  read_req.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
  read_req.zcl_basic_cmd.src_endpoint = _endpoint;
  read_req.clusterID = ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT;

  uint16_t attributes[] = {
    ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_MIN_VALUE_ID, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_MAX_VALUE_ID, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_TOLERANCE_ID
  };
  read_req.attr_number = ZB_ARRAY_LENGHT(attributes);
  read_req.attr_field = attributes;

  log_i("Sending 'read sensor settings' command");
  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_read_attr_cmd_req(&read_req);
  esp_zb_lock_release();

  //Take semaphore to wait for response of all attributes
  if (xSemaphoreTake(lock, ZB_CMD_TIMEOUT) != pdTRUE) {
    log_e("Error while reading attributes");
    return;
  } else {
    //Call the callback function when all attributes are read
    _on_config_receive(_min_temp, _max_temp, _tolerance);
  }
}

void ZigbeeThermostat::getSensorSettings(uint16_t group_addr) {
  /* Send "read attributes" command to the group */
  esp_zb_zcl_read_attr_cmd_t read_req = {0};
  read_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_GROUP_ENDP_NOT_PRESENT;
  read_req.zcl_basic_cmd.src_endpoint = _endpoint;
  read_req.zcl_basic_cmd.dst_addr_u.addr_short = group_addr;
  read_req.clusterID = ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT;

  uint16_t attributes[] = {
    ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_MIN_VALUE_ID, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_MAX_VALUE_ID, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_TOLERANCE_ID
  };
  read_req.attr_number = ZB_ARRAY_LENGHT(attributes);
  read_req.attr_field = attributes;

  log_i("Sending 'read sensor settings' command to group address 0x%x", group_addr);
  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_read_attr_cmd_req(&read_req);
  esp_zb_lock_release();

  //Take semaphore to wait for response of all attributes
  if (xSemaphoreTake(lock, ZB_CMD_TIMEOUT) != pdTRUE) {
    log_e("Error while reading attributes");
    return;
  } else {
    //Call the callback function when all attributes are read
    _on_config_receive(_min_temp, _max_temp, _tolerance);
  }
}

void ZigbeeThermostat::getSensorSettings(uint8_t endpoint, uint16_t short_addr) {
  /* Send "read attributes" command to specific endpoint */
  esp_zb_zcl_read_attr_cmd_t read_req = {0};
  read_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
  read_req.zcl_basic_cmd.src_endpoint = _endpoint;
  read_req.zcl_basic_cmd.dst_endpoint = endpoint;
  read_req.zcl_basic_cmd.dst_addr_u.addr_short = short_addr;
  read_req.clusterID = ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT;

  uint16_t attributes[] = {
    ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_MIN_VALUE_ID, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_MAX_VALUE_ID, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_TOLERANCE_ID
  };
  read_req.attr_number = ZB_ARRAY_LENGHT(attributes);
  read_req.attr_field = attributes;

  log_i("Sending 'read sensor settings' command to endpoint %d, address 0x%x", endpoint, short_addr);
  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_read_attr_cmd_req(&read_req);
  esp_zb_lock_release();

  //Take semaphore to wait for response of all attributes
  if (xSemaphoreTake(lock, ZB_CMD_TIMEOUT) != pdTRUE) {
    log_e("Error while reading attributes");
    return;
  } else {
    //Call the callback function when all attributes are read
    _on_config_receive(_min_temp, _max_temp, _tolerance);
  }
}

void ZigbeeThermostat::getSensorSettings(uint8_t endpoint, esp_zb_ieee_addr_t ieee_addr) {
  /* Send "read attributes" command to specific endpoint */
  esp_zb_zcl_read_attr_cmd_t read_req = {0};
  read_req.address_mode = ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT;
  read_req.zcl_basic_cmd.src_endpoint = _endpoint;
  read_req.zcl_basic_cmd.dst_endpoint = endpoint;
  read_req.clusterID = ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT;
  memcpy(read_req.zcl_basic_cmd.dst_addr_u.addr_long, ieee_addr, sizeof(esp_zb_ieee_addr_t));

  uint16_t attributes[] = {
    ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_MIN_VALUE_ID, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_MAX_VALUE_ID, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_TOLERANCE_ID
  };
  read_req.attr_number = ZB_ARRAY_LENGHT(attributes);
  read_req.attr_field = attributes;

  log_i(
    "Sending 'read sensor settings' command to endpoint %d, ieee address %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x", endpoint, ieee_addr[7], ieee_addr[6],
    ieee_addr[5], ieee_addr[4], ieee_addr[3], ieee_addr[2], ieee_addr[1], ieee_addr[0]
  );
  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_read_attr_cmd_req(&read_req);
  esp_zb_lock_release();

  //Take semaphore to wait for response of all attributes
  if (xSemaphoreTake(lock, ZB_CMD_TIMEOUT) != pdTRUE) {
    log_e("Error while reading attributes");
    return;
  } else {
    //Call the callback function when all attributes are read
    _on_config_receive(_min_temp, _max_temp, _tolerance);
  }
}

void ZigbeeThermostat::setTemperatureReporting(uint16_t min_interval, uint16_t max_interval, float delta) {
  /* Send "configure report attribute" command to all bound sensors */
  esp_zb_zcl_config_report_cmd_t report_cmd = {0};
  report_cmd.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
  report_cmd.zcl_basic_cmd.src_endpoint = _endpoint;
  report_cmd.clusterID = ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT;

  int16_t report_change = (int16_t)delta * 100;
  esp_zb_zcl_config_report_record_t records[] = {
    {
      .direction = ESP_ZB_ZCL_REPORT_DIRECTION_SEND,
      .attributeID = ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID,
      .attrType = ESP_ZB_ZCL_ATTR_TYPE_S16,
      .min_interval = min_interval,
      .max_interval = max_interval,
      .reportable_change = (void *)&report_change,
    },
  };
  report_cmd.record_number = ZB_ARRAY_LENGHT(records);
  report_cmd.record_field = records;

  log_i("Sending 'configure reporting' command");
  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_config_report_cmd_req(&report_cmd);
  esp_zb_lock_release();
}

void ZigbeeThermostat::setTemperatureReporting(uint16_t group_addr, uint16_t min_interval, uint16_t max_interval, float delta) {
  /* Send "configure report attribute" command to the group */
  esp_zb_zcl_config_report_cmd_t report_cmd = {0};
  report_cmd.address_mode = ESP_ZB_APS_ADDR_MODE_16_GROUP_ENDP_NOT_PRESENT;
  report_cmd.zcl_basic_cmd.src_endpoint = _endpoint;
  report_cmd.zcl_basic_cmd.dst_addr_u.addr_short = group_addr;
  report_cmd.clusterID = ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT;

  int16_t report_change = (int16_t)delta * 100;
  esp_zb_zcl_config_report_record_t records[] = {
    {
      .direction = ESP_ZB_ZCL_REPORT_DIRECTION_SEND,
      .attributeID = ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID,
      .attrType = ESP_ZB_ZCL_ATTR_TYPE_S16,
      .min_interval = min_interval,
      .max_interval = max_interval,
      .reportable_change = (void *)&report_change,
    },
  };
  report_cmd.record_number = ZB_ARRAY_LENGHT(records);
  report_cmd.record_field = records;

  log_i("Sending 'configure reporting' command to group address 0x%x", group_addr);
  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_config_report_cmd_req(&report_cmd);
  esp_zb_lock_release();
}

void ZigbeeThermostat::setTemperatureReporting(uint8_t endpoint, uint16_t short_addr, uint16_t min_interval, uint16_t max_interval, float delta) {
  /* Send "configure report attribute" command to specific endpoint */
  esp_zb_zcl_config_report_cmd_t report_cmd = {0};
  report_cmd.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
  report_cmd.zcl_basic_cmd.src_endpoint = _endpoint;
  report_cmd.zcl_basic_cmd.dst_endpoint = endpoint;
  report_cmd.zcl_basic_cmd.dst_addr_u.addr_short = short_addr;
  report_cmd.clusterID = ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT;

  int16_t report_change = (int16_t)delta * 100;
  esp_zb_zcl_config_report_record_t records[] = {
    {
      .direction = ESP_ZB_ZCL_REPORT_DIRECTION_SEND,
      .attributeID = ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID,
      .attrType = ESP_ZB_ZCL_ATTR_TYPE_S16,
      .min_interval = min_interval,
      .max_interval = max_interval,
      .reportable_change = (void *)&report_change,
    },
  };
  report_cmd.record_number = ZB_ARRAY_LENGHT(records);
  report_cmd.record_field = records;

  log_i("Sending 'configure reporting' command to endpoint %d, address 0x%x", endpoint, short_addr);
  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_config_report_cmd_req(&report_cmd);
  esp_zb_lock_release();
}

void ZigbeeThermostat::setTemperatureReporting(uint8_t endpoint, esp_zb_ieee_addr_t ieee_addr, uint16_t min_interval, uint16_t max_interval, float delta) {
  /* Send "configure report attribute" command to specific endpoint */
  esp_zb_zcl_config_report_cmd_t report_cmd = {0};
  report_cmd.address_mode = ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT;
  report_cmd.zcl_basic_cmd.src_endpoint = _endpoint;
  report_cmd.zcl_basic_cmd.dst_endpoint = endpoint;
  report_cmd.clusterID = ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT;
  memcpy(report_cmd.zcl_basic_cmd.dst_addr_u.addr_long, ieee_addr, sizeof(esp_zb_ieee_addr_t));

  int16_t report_change = (int16_t)delta * 100;
  esp_zb_zcl_config_report_record_t records[] = {
    {
      .direction = ESP_ZB_ZCL_REPORT_DIRECTION_SEND,
      .attributeID = ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID,
      .attrType = ESP_ZB_ZCL_ATTR_TYPE_S16,
      .min_interval = min_interval,
      .max_interval = max_interval,
      .reportable_change = (void *)&report_change,
    },
  };
  report_cmd.record_number = ZB_ARRAY_LENGHT(records);
  report_cmd.record_field = records;

  log_i(
    "Sending 'configure reporting' command to endpoint %d, ieee address %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x", endpoint, ieee_addr[7], ieee_addr[6],
    ieee_addr[5], ieee_addr[4], ieee_addr[3], ieee_addr[2], ieee_addr[1], ieee_addr[0]
  );
  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_config_report_cmd_req(&report_cmd);
  esp_zb_lock_release();
}

#endif  // CONFIG_ZB_ENABLED
