/* Common Class for Zigbee End Point */

#include "ZigbeeEP.h"

#if SOC_IEEE802154_SUPPORTED && CONFIG_ZB_ENABLED

#include "esp_zigbee_cluster.h"
#include "zcl/esp_zigbee_zcl_power_config.h"

bool ZigbeeEP::_is_bound = false;
bool ZigbeeEP::_allow_multiple_binding = false;

//TODO: is_bound and allow_multiple_binding to make not static

/* Zigbee End Device Class */
ZigbeeEP::ZigbeeEP(uint8_t endpoint) {
  _endpoint = endpoint;
  log_v("Endpoint: %d", _endpoint);
  _ep_config.endpoint = 0;
  _cluster_list = nullptr;
  _on_identify = nullptr;
  if (!lock) {
    lock = xSemaphoreCreateBinary();
    if (lock == NULL) {
      log_e("Semaphore creation failed");
    }
  }
}

ZigbeeEP::~ZigbeeEP() {}

void ZigbeeEP::setVersion(uint8_t version) {
  _ep_config.app_device_version = version;
}

void ZigbeeEP::setManufacturerAndModel(const char *name, const char *model) {
  // Convert manufacturer to ZCL string
  size_t length = strlen(name);
  if (length > 32) {
    log_e("Manufacturer name is too long");
    return;
  }
  // Allocate a new array of size length + 2 (1 for the length, 1 for null terminator)
  char *zb_name = new char[length + 2];
  // Store the length as the first element
  zb_name[0] = static_cast<char>(length);  // Cast size_t to char
  // Use memcpy to copy the characters to the result array
  memcpy(zb_name + 1, name, length);
  // Null-terminate the array
  zb_name[length + 1] = '\0';

  // Convert model to ZCL string
  length = strlen(model);
  if (length > 32) {
    log_e("Model name is too long");
    delete[] zb_name;
    return;
  }
  char *zb_model = new char[length + 2];
  zb_model[0] = static_cast<char>(length);
  memcpy(zb_model + 1, model, length);
  zb_model[length + 1] = '\0';

  // Get the basic cluster and update the manufacturer and model attributes
  esp_zb_attribute_list_t *basic_cluster = esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_BASIC, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, (void *)zb_name);
  esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, (void *)zb_model);
}

void ZigbeeEP::setPowerSource(zb_power_source_t power_source, uint8_t battery_percentage) {
  esp_zb_attribute_list_t *basic_cluster = esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_BASIC, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_update_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_POWER_SOURCE_ID, (void *)&power_source);

  if (power_source == ZB_POWER_SOURCE_BATTERY) {
    // Add power config cluster and battery percentage attribute
    battery_percentage = battery_percentage * 2;
    esp_zb_attribute_list_t *power_config_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG);
    esp_zb_power_config_cluster_add_attr(power_config_cluster, ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID, (void *)&battery_percentage);
    esp_zb_cluster_list_add_power_config_cluster(_cluster_list, power_config_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  }
  _power_source = power_source;
}

void ZigbeeEP::setBatteryPercentage(uint8_t percentage) {
  // 100% = 200 in decimal, 0% = 0
  // Convert percentage to 0-200 range
  if (percentage > 100) {
    percentage = 100;
  }
  percentage = percentage * 2;
  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID, &percentage,
    false
  );
  esp_zb_lock_release();
  log_v("Battery percentage updated");
}

void ZigbeeEP::reportBatteryPercentage() {
  /* Send report attributes command */
  esp_zb_zcl_report_attr_cmd_t report_attr_cmd;
  report_attr_cmd.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
  report_attr_cmd.attributeID = ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID;
  report_attr_cmd.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI;
  report_attr_cmd.clusterID = ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG;
  report_attr_cmd.zcl_basic_cmd.src_endpoint = _endpoint;

  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_report_attr_cmd_req(&report_attr_cmd);
  esp_zb_lock_release();
  log_v("Battery percentage reported");
}

char *ZigbeeEP::readManufacturer(uint8_t endpoint, uint16_t short_addr, esp_zb_ieee_addr_t ieee_addr) {
  /* Read peer Manufacture Name & Model Identifier */
  esp_zb_zcl_read_attr_cmd_t read_req;

  if (short_addr != 0) {
    read_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
    read_req.zcl_basic_cmd.dst_addr_u.addr_short = short_addr;
  } else {
    read_req.address_mode = ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT;
    memcpy(read_req.zcl_basic_cmd.dst_addr_u.addr_long, ieee_addr, sizeof(esp_zb_ieee_addr_t));
  }

  read_req.zcl_basic_cmd.src_endpoint = _endpoint;
  read_req.zcl_basic_cmd.dst_endpoint = endpoint;
  read_req.clusterID = ESP_ZB_ZCL_CLUSTER_ID_BASIC;

  uint16_t attributes[] = {
    ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID,
  };
  read_req.attr_number = ZB_ARRAY_LENTH(attributes);
  read_req.attr_field = attributes;

  // clear read manufacturer
  _read_manufacturer = nullptr;

  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_read_attr_cmd_req(&read_req);
  esp_zb_lock_release();

  //Wait for response or timeout
  if (xSemaphoreTake(lock, ZB_CMD_TIMEOUT) != pdTRUE) {
    log_e("Error while reading manufacturer");
  }
  return _read_manufacturer;
}

char *ZigbeeEP::readModel(uint8_t endpoint, uint16_t short_addr, esp_zb_ieee_addr_t ieee_addr) {
  /* Read peer Manufacture Name & Model Identifier */
  esp_zb_zcl_read_attr_cmd_t read_req;

  if (short_addr != 0) {
    read_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
    read_req.zcl_basic_cmd.dst_addr_u.addr_short = short_addr;
  } else {
    read_req.address_mode = ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT;
    memcpy(read_req.zcl_basic_cmd.dst_addr_u.addr_long, ieee_addr, sizeof(esp_zb_ieee_addr_t));
  }

  read_req.zcl_basic_cmd.src_endpoint = _endpoint;
  read_req.zcl_basic_cmd.dst_endpoint = endpoint;
  read_req.clusterID = ESP_ZB_ZCL_CLUSTER_ID_BASIC;

  uint16_t attributes[] = {
    ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID,
  };
  read_req.attr_number = ZB_ARRAY_LENTH(attributes);
  read_req.attr_field = attributes;

  // clear read model
  _read_model = nullptr;

  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_read_attr_cmd_req(&read_req);
  esp_zb_lock_release();

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
      "Device on endpoint %d, short address: 0x%x, ieee address: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x", device->endpoint, device->short_addr,
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
      "Device on endpoint %d, short address: 0x%x, ieee address: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\r\n", device->endpoint, device->short_addr,
      device->ieee_addr[7], device->ieee_addr[6], device->ieee_addr[5], device->ieee_addr[4], device->ieee_addr[3], device->ieee_addr[2], device->ieee_addr[1],
      device->ieee_addr[0]
    );
  }
}

void ZigbeeEP::zbReadBasicCluster(const esp_zb_zcl_attribute_t *attribute) {
  /* Basic cluster attributes */
  if (attribute->id == ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID && attribute->data.type == ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING && attribute->data.value) {
    zbstring_t *zbstr = (zbstring_t *)attribute->data.value;
    char *string = (char *)malloc(zbstr->len + 1);
    memcpy(string, zbstr->data, zbstr->len);
    string[zbstr->len] = '\0';
    log_i("Peer Manufacturer is \"%s\"", string);
    _read_manufacturer = string;
    xSemaphoreGive(lock);
  }
  if (attribute->id == ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID && attribute->data.type == ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING && attribute->data.value) {
    zbstring_t *zbstr = (zbstring_t *)attribute->data.value;
    char *string = (char *)malloc(zbstr->len + 1);
    memcpy(string, zbstr->data, zbstr->len);
    string[zbstr->len] = '\0';
    log_i("Peer Model is \"%s\"", string);
    _read_model = string;
    xSemaphoreGive(lock);
  }
}

void ZigbeeEP::zbIdentify(const esp_zb_zcl_set_attr_value_message_t *message) {
  if (message->attribute.id == ESP_ZB_ZCL_CMD_IDENTIFY_IDENTIFY_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
    if (_on_identify != NULL) {
      _on_identify(*(uint16_t *)message->attribute.data.value);
    }
  } else {
    log_w("Other identify commands are not implemented yet.");
  }
}

#endif  //SOC_IEEE802154_SUPPORTED && CONFIG_ZB_ENABLED
