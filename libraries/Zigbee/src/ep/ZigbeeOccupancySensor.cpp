#include "ZigbeeOccupancySensor.h"
#if CONFIG_ZB_ENABLED

esp_zb_cluster_list_t *zigbee_occupancy_sensor_clusters_create(zigbee_occupancy_sensor_cfg_t *occupancy_sensor) {
  esp_zb_basic_cluster_cfg_t *basic_cfg = occupancy_sensor ? &(occupancy_sensor->basic_cfg) : NULL;
  esp_zb_identify_cluster_cfg_t *identify_cfg = occupancy_sensor ? &(occupancy_sensor->identify_cfg) : NULL;
  esp_zb_occupancy_sensing_cluster_cfg_t *occupancy_meas_cfg = occupancy_sensor ? &(occupancy_sensor->occupancy_meas_cfg) : NULL;
  esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
  esp_zb_cluster_list_add_basic_cluster(cluster_list, esp_zb_basic_cluster_create(basic_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_identify_cluster(cluster_list, esp_zb_identify_cluster_create(identify_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_occupancy_sensing_cluster(cluster_list, esp_zb_occupancy_sensing_cluster_create(occupancy_meas_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  return cluster_list;
}

ZigbeeOccupancySensor::ZigbeeOccupancySensor(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID;

  //Create custom occupancy sensor configuration
  zigbee_occupancy_sensor_cfg_t occupancy_sensor_cfg = ZIGBEE_DEFAULT_OCCUPANCY_SENSOR_CONFIG();
  _cluster_list = zigbee_occupancy_sensor_clusters_create(&occupancy_sensor_cfg);

  _ep_config = {.endpoint = _endpoint, .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID, .app_device_id = ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID, .app_device_version = 0};
}

bool ZigbeeOccupancySensor::setSensorType(ZigbeeOccupancySensorType sensor_type, ZigbeeOccupancySensorTypeBitmap sensor_type_bitmap) {
  uint8_t sensor_type_bitmap_value = sensor_type_bitmap;
  if(sensor_type_bitmap == ZIGBEE_OCCUPANCY_SENSOR_BITMAP_DEFAULT) {
    sensor_type_bitmap_value = (1 << sensor_type); // Default to single sensor type if bitmap is default
  }
  esp_zb_attribute_list_t *occupancy_sens_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_err_t ret = esp_zb_cluster_update_attr(occupancy_sens_cluster, ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_OCCUPANCY_SENSOR_TYPE_ID, (void *)&sensor_type);
  if (ret != ESP_OK) {
    log_e("Failed to set sensor type: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  ret = esp_zb_cluster_update_attr(occupancy_sens_cluster, ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_OCCUPANCY_SENSOR_TYPE_BITMAP_ID, (void *)&sensor_type_bitmap_value);
  if (ret != ESP_OK) {
    log_e("Failed to set sensor type bitmap: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }

  // Handle PIR attributes if PIR bit is set (bit 0)
  if (sensor_type_bitmap & ZIGBEE_OCCUPANCY_SENSOR_BITMAP_PIR) {
    ret = esp_zb_occupancy_sensing_cluster_add_attr(occupancy_sens_cluster, ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_PIR_OCC_TO_UNOCC_DELAY_ID, ESP_ZB_ZCL_OCCUPANCY_SENSING_PIR_OCC_TO_UNOCC_DELAY_DEFAULT_VALUE);
    if (ret != ESP_OK) {
      log_e("Failed to set PIR occupied to unoccupied delay: 0x%x: %s", ret, esp_err_to_name(ret));
      return false;
    }
    ret = esp_zb_occupancy_sensing_cluster_add_attr(occupancy_sens_cluster, ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_PIR_UNOCC_TO_OCC_DELAY_ID, ESP_ZB_ZCL_OCCUPANCY_SENSING_PIR_UNOCC_TO_OCC_DELAY_DEFAULT_VALUE);
    if (ret != ESP_OK) {
      log_e("Failed to set PIR unoccupied to occupied delay: 0x%x: %s", ret, esp_err_to_name(ret));
      return false;
    }
    uint8_t pir_threshold = ESP_ZB_ZCL_OCCUPANCY_SENSING_PIR_UNOCC_TO_OCC_THRESHOLD_DEFAULT_VALUE;
    ret = esp_zb_occupancy_sensing_cluster_add_attr(occupancy_sens_cluster, ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_PIR_UNOCC_TO_OCC_THRESHOLD_ID, &pir_threshold);
    if (ret != ESP_OK) {
      log_e("Failed to set PIR unoccupied to occupied threshold: 0x%x: %s", ret, esp_err_to_name(ret));
      return false;
    }
  }

  // Handle Ultrasonic attributes if Ultrasonic bit is set (bit 1)
  if (sensor_type_bitmap & ZIGBEE_OCCUPANCY_SENSOR_BITMAP_ULTRASONIC) {
    ret = esp_zb_occupancy_sensing_cluster_add_attr(occupancy_sens_cluster, ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_ULTRASONIC_OCCUPIED_TO_UNOCCUPIED_DELAY_ID, ESP_ZB_ZCL_OCCUPANCY_SENSING_ULTRASONIC_OCCUPIED_TO_UNOCCUPIED_DELAY_DEFAULT_VALUE);
    if (ret != ESP_OK) {
      log_e("Failed to set ultrasonic occupied to unoccupied delay: 0x%x: %s", ret, esp_err_to_name(ret));
      return false;
    }
    ret = esp_zb_occupancy_sensing_cluster_add_attr(occupancy_sens_cluster, ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_ULTRASONIC_UNOCCUPIED_TO_OCCUPIED_DELAY_ID, ESP_ZB_ZCL_OCCUPANCY_SENSING_ULTRASONIC_UNOCCUPIED_TO_OCCUPIED_DELAY_DEFAULT_VALUE);
    if (ret != ESP_OK) {
      log_e("Failed to set ultrasonic unoccupied to occupied delay: 0x%x: %s", ret, esp_err_to_name(ret));
      return false;
    }
    uint8_t ultrasonic_threshold = ESP_ZB_ZCL_OCCUPANCY_SENSING_ULTRASONIC_UNOCCUPIED_TO_OCCUPIED_THRESHOLD_DEFAULT_VALUE;
    ret = esp_zb_occupancy_sensing_cluster_add_attr(occupancy_sens_cluster, ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_ULTRASONIC_UNOCCUPIED_TO_OCCUPIED_THRESHOLD_ID, &ultrasonic_threshold);
    if (ret != ESP_OK) {
      log_e("Failed to set ultrasonic unoccupied to occupied threshold: 0x%x: %s", ret, esp_err_to_name(ret));
      return false;
    }
  }

  // Handle Physical Contact attributes if Physical Contact bit is set (bit 2)
  if (sensor_type_bitmap & ZIGBEE_OCCUPANCY_SENSOR_BITMAP_PHYSICAL_CONTACT_AND_PIR) {
    ret = esp_zb_occupancy_sensing_cluster_add_attr(occupancy_sens_cluster, ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_PHYSICAL_CONTACT_OCCUPIED_TO_UNOCCUPIED_DELAY_ID, ESP_ZB_ZCL_OCCUPANCY_SENSING_PHYSICAL_CONTACT_OCCUPIED_TO_UNOCCUPIED_DELAY_DEFAULT_VALUE);
    if (ret != ESP_OK) {
      log_e("Failed to set physical contact occupied to unoccupied delay: 0x%x: %s", ret, esp_err_to_name(ret));
      return false;
    }
    ret = esp_zb_occupancy_sensing_cluster_add_attr(occupancy_sens_cluster, ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_PHYSICAL_CONTACT_UNOCCUPIED_TO_OCCUPIED_DELAY_ID, ESP_ZB_ZCL_OCCUPANCY_SENSING_PHYSICAL_CONTACT_UNOCCUPIED_TO_OCCUPIED_DELAY_DEFAULT_VALUE);
    if (ret != ESP_OK) {
      log_e("Failed to set physical contact unoccupied to occupied delay: 0x%x: %s", ret, esp_err_to_name(ret));
      return false;
    }
    uint8_t physical_contact_threshold = ESP_ZB_ZCL_OCCUPANCY_SENSING_PHYSICAL_CONTACT_UNOCCUPIED_TO_OCCUPIED_THRESHOLD_MIN_VALUE;
    ret = esp_zb_occupancy_sensing_cluster_add_attr(occupancy_sens_cluster, ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_PHYSICAL_CONTACT_UNOCCUPIED_TO_OCCUPIED_THRESHOLD_ID, &physical_contact_threshold);
    if (ret != ESP_OK) {
      log_e("Failed to set physical contact unoccupied to occupied threshold: 0x%x: %s", ret, esp_err_to_name(ret));
      return false;
    }
  }
  return true;
}

bool ZigbeeOccupancySensor::setOccupiedToUnoccupiedDelay(ZigbeeOccupancySensorType sensor_type, uint16_t delay) {
  esp_zb_attribute_list_t *occupancy_sens_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  
  esp_err_t ret;
  switch (sensor_type) {
    case ZIGBEE_OCCUPANCY_SENSOR_TYPE_PIR:
      ret = esp_zb_cluster_update_attr(occupancy_sens_cluster, ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_PIR_OCC_TO_UNOCC_DELAY_ID, (void *)&delay);
      break;
    case ZIGBEE_OCCUPANCY_SENSOR_TYPE_ULTRASONIC:
      ret = esp_zb_cluster_update_attr(occupancy_sens_cluster, ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_ULTRASONIC_OCCUPIED_TO_UNOCCUPIED_DELAY_ID, (void *)&delay);
      break;
    case ZIGBEE_OCCUPANCY_SENSOR_TYPE_PHYSICAL_CONTACT:
      ret = esp_zb_cluster_update_attr(occupancy_sens_cluster, ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_PHYSICAL_CONTACT_OCCUPIED_TO_UNOCCUPIED_DELAY_ID, (void *)&delay);
      break;
    default:
      log_e("Invalid sensor type for delay setting: 0x%x", sensor_type);
      return false;
  }
  
  if (ret != ESP_OK) {
    log_e("Failed to set occupied to unoccupied delay: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeOccupancySensor::setUnoccupiedToOccupiedDelay(ZigbeeOccupancySensorType sensor_type, uint16_t delay) {
  esp_zb_attribute_list_t *occupancy_sens_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  
  esp_err_t ret;
  switch (sensor_type) {
    case ZIGBEE_OCCUPANCY_SENSOR_TYPE_PIR:
      ret = esp_zb_cluster_update_attr(occupancy_sens_cluster, ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_PIR_UNOCC_TO_OCC_DELAY_ID, (void *)&delay);
      break;
    case ZIGBEE_OCCUPANCY_SENSOR_TYPE_ULTRASONIC:
      ret = esp_zb_cluster_update_attr(occupancy_sens_cluster, ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_ULTRASONIC_UNOCCUPIED_TO_OCCUPIED_DELAY_ID, (void *)&delay);
      break;
    case ZIGBEE_OCCUPANCY_SENSOR_TYPE_PHYSICAL_CONTACT:
      ret = esp_zb_cluster_update_attr(occupancy_sens_cluster, ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_PHYSICAL_CONTACT_UNOCCUPIED_TO_OCCUPIED_DELAY_ID, (void *)&delay);
      break;
    default:
      log_e("Invalid sensor type for delay setting: 0x%x", sensor_type);
      return false;
  }
  
  if (ret != ESP_OK) {
    log_e("Failed to set unoccupied to occupied delay: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeOccupancySensor::setUnoccupiedToOccupiedThreshold(ZigbeeOccupancySensorType sensor_type, uint8_t threshold) {
  esp_zb_attribute_list_t *occupancy_sens_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  
  esp_err_t ret;
  switch (sensor_type) {
    case ZIGBEE_OCCUPANCY_SENSOR_TYPE_PIR:
      ret = esp_zb_cluster_update_attr(occupancy_sens_cluster, ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_PIR_UNOCC_TO_OCC_THRESHOLD_ID, (void *)&threshold);
      break;
    case ZIGBEE_OCCUPANCY_SENSOR_TYPE_ULTRASONIC:
      ret = esp_zb_cluster_update_attr(occupancy_sens_cluster, ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_ULTRASONIC_UNOCCUPIED_TO_OCCUPIED_THRESHOLD_ID, (void *)&threshold);
      break;
    case ZIGBEE_OCCUPANCY_SENSOR_TYPE_PHYSICAL_CONTACT:
      ret = esp_zb_cluster_update_attr(occupancy_sens_cluster, ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_PHYSICAL_CONTACT_UNOCCUPIED_TO_OCCUPIED_THRESHOLD_ID, (void *)&threshold);
      break;
    default:
      log_e("Invalid sensor type for threshold setting: 0x%x", sensor_type);
      return false;
  }
  
  if (ret != ESP_OK) {
    log_e("Failed to set unoccupied to occupied threshold: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeOccupancySensor::setOccupancy(bool occupied) {
  esp_zb_zcl_status_t ret = ESP_ZB_ZCL_STATUS_SUCCESS;
  log_v("Updating occupancy sensor value...");
  /* Update occupancy sensor value */
  log_d("Setting occupancy to %d", occupied);
  esp_zb_lock_acquire(portMAX_DELAY);
  ret = esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_OCCUPANCY_ID, &occupied, false
  );
  esp_zb_lock_release();
  if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set occupancy: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeOccupancySensor::report() {
  /* Send report attributes command */
  esp_zb_zcl_report_attr_cmd_t report_attr_cmd;
  report_attr_cmd.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
  report_attr_cmd.attributeID = ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_OCCUPANCY_ID;
  report_attr_cmd.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI;
  report_attr_cmd.clusterID = ESP_ZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING;
  report_attr_cmd.zcl_basic_cmd.src_endpoint = _endpoint;
  report_attr_cmd.manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC;

  esp_zb_lock_acquire(portMAX_DELAY);
  esp_err_t ret = esp_zb_zcl_report_attr_cmd_req(&report_attr_cmd);
  esp_zb_lock_release();
  if (ret != ESP_OK) {
    log_e("Failed to send occupancy report: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  log_v("Occupancy report sent");
  return true;
}

//set attribute method -> method overridden in child class
void ZigbeeOccupancySensor::zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) {
  if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING) {
    //PIR
    if (message->attribute.id == ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_PIR_OCC_TO_UNOCC_DELAY_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
      uint16_t pir_occ_to_unocc_delay = *(uint16_t *)message->attribute.data.value;
      occupancyConfigChanged(ZIGBEE_OCCUPANCY_SENSOR_TYPE_PIR);
    } else if (message->attribute.id == ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_PIR_UNOCC_TO_OCC_DELAY_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
      uint16_t pir_unocc_to_occ_delay = *(uint16_t *)message->attribute.data.value;
      occupancyConfigChanged(ZIGBEE_OCCUPANCY_SENSOR_TYPE_PIR);
    } else if (message->attribute.id == ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_PIR_UNOCC_TO_OCC_THRESHOLD_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U8) {
      uint8_t pir_unocc_to_occ_threshold = *(uint8_t *)message->attribute.data.value;
      occupancyConfigChanged(ZIGBEE_OCCUPANCY_SENSOR_TYPE_PIR);
    } 
    //Ultrasonic
    else if (message->attribute.id == ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_ULTRASONIC_OCCUPIED_TO_UNOCCUPIED_DELAY_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
      uint16_t ultrasonic_occ_to_unocc_delay = *(uint16_t *)message->attribute.data.value;
      occupancyConfigChanged(ZIGBEE_OCCUPANCY_SENSOR_TYPE_ULTRASONIC);
    } else if (message->attribute.id == ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_ULTRASONIC_UNOCCUPIED_TO_OCCUPIED_DELAY_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
      uint16_t ultrasonic_unocc_to_occ_delay = *(uint16_t *)message->attribute.data.value;
      occupancyConfigChanged(ZIGBEE_OCCUPANCY_SENSOR_TYPE_ULTRASONIC);
    } else if (message->attribute.id == ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_ULTRASONIC_UNOCCUPIED_TO_OCCUPIED_THRESHOLD_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U8) {
      uint8_t ultrasonic_unocc_to_occ_threshold = *(uint8_t *)message->attribute.data.value;
      occupancyConfigChanged(ZIGBEE_OCCUPANCY_SENSOR_TYPE_ULTRASONIC);
    } 
    //Physical Contact  
    else if (message->attribute.id == ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_PHYSICAL_CONTACT_OCCUPIED_TO_UNOCCUPIED_DELAY_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
      uint16_t physical_contact_occ_to_unocc_delay = *(uint16_t *)message->attribute.data.value;
      occupancyConfigChanged(ZIGBEE_OCCUPANCY_SENSOR_TYPE_PHYSICAL_CONTACT);
    } else if (message->attribute.id == ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_PHYSICAL_CONTACT_UNOCCUPIED_TO_OCCUPIED_DELAY_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
      uint16_t physical_contact_unocc_to_occ_delay = *(uint16_t *)message->attribute.data.value;
      occupancyConfigChanged(ZIGBEE_OCCUPANCY_SENSOR_TYPE_PHYSICAL_CONTACT);
    } else if (message->attribute.id == ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_PHYSICAL_CONTACT_UNOCCUPIED_TO_OCCUPIED_THRESHOLD_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U8) {
      uint8_t physical_contact_unocc_to_occ_threshold = *(uint8_t *)message->attribute.data.value;
      occupancyConfigChanged(ZIGBEE_OCCUPANCY_SENSOR_TYPE_PHYSICAL_CONTACT);
    } else {
      log_w("Received message ignored. Attribute ID: %d not supported for Occupancy Sensor endpoint", message->attribute.id);
    }
  } else {
    log_w("Received message ignored. Cluster ID: %d not supported for Occupancy Sensor endpoint", message->info.cluster);
  }
}

void ZigbeeOccupancySensor::occupancyConfigChanged(ZigbeeOccupancySensorType sensor_type) {
  if (_on_occupancy_config_change) {
    _on_occupancy_config_change(sensor_type); //sensor type, delay, delay, threshold
  } else {
    log_w("No callback function set for occupancy config change");
  }
}

#endif  // CONFIG_ZB_ENABLED
