/* Zigbee Common Functions */
#include "ZigbeeCore.h"
#include "Arduino.h"

#if CONFIG_ZB_ENABLED

#include "esp_ota_ops.h"
#if CONFIG_ZB_DELTA_OTA  // Delta OTA, code is prepared for this feature but not enabled by default
#include "esp_delta_ota_ops.h"
#endif

//OTA Upgrade defines and variables
#define OTA_ELEMENT_HEADER_LEN 6 /* OTA element format header size include tag identifier and length field */

/**
 * @name Enumeration for the tag identifier denotes the type and format of the data within the element
 * @anchor esp_ota_element_tag_id_t
 */
typedef enum esp_ota_element_tag_id_e {
  UPGRADE_IMAGE = 0x0000, /*!< Upgrade image */
} esp_ota_element_tag_id_t;

static const esp_partition_t *s_ota_partition = NULL;
static esp_ota_handle_t s_ota_handle = 0;
static bool s_tagid_received = false;

// forward declaration of all implemented handlers
static esp_err_t zb_attribute_set_handler(const esp_zb_zcl_set_attr_value_message_t *message);
static esp_err_t zb_attribute_reporting_handler(const esp_zb_zcl_report_attr_message_t *message);
static esp_err_t zb_cmd_read_attr_resp_handler(const esp_zb_zcl_cmd_read_attr_resp_message_t *message);
static esp_err_t zb_configure_report_resp_handler(const esp_zb_zcl_cmd_config_report_resp_message_t *message);
static esp_err_t zb_cmd_ias_zone_status_change_handler(const esp_zb_zcl_ias_zone_status_change_notification_message_t *message);
static esp_err_t zb_cmd_ias_zone_enroll_response_handler(const esp_zb_zcl_ias_zone_enroll_response_message_t *message);
static esp_err_t zb_cmd_default_resp_handler(const esp_zb_zcl_cmd_default_resp_message_t *message);
static esp_err_t zb_window_covering_movement_resp_handler(const esp_zb_zcl_window_covering_movement_message_t *message);
static esp_err_t zb_ota_upgrade_status_handler(const esp_zb_zcl_ota_upgrade_value_message_t *message);
static esp_err_t zb_ota_upgrade_query_image_resp_handler(const esp_zb_zcl_ota_upgrade_query_image_resp_message_t *message);

// Zigbee action handlers
[[maybe_unused]]
static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message) {
  esp_err_t ret = ESP_OK;
  switch (callback_id) {
    case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:         ret = zb_attribute_set_handler((esp_zb_zcl_set_attr_value_message_t *)message); break;
    case ESP_ZB_CORE_REPORT_ATTR_CB_ID:            ret = zb_attribute_reporting_handler((esp_zb_zcl_report_attr_message_t *)message); break;
    case ESP_ZB_CORE_CMD_READ_ATTR_RESP_CB_ID:     ret = zb_cmd_read_attr_resp_handler((esp_zb_zcl_cmd_read_attr_resp_message_t *)message); break;
    case ESP_ZB_CORE_CMD_REPORT_CONFIG_RESP_CB_ID: ret = zb_configure_report_resp_handler((esp_zb_zcl_cmd_config_report_resp_message_t *)message); break;
    case ESP_ZB_CORE_CMD_IAS_ZONE_ZONE_STATUS_CHANGE_NOT_ID:
      ret = zb_cmd_ias_zone_status_change_handler((esp_zb_zcl_ias_zone_status_change_notification_message_t *)message);
      break;
    case ESP_ZB_CORE_IAS_ZONE_ENROLL_RESPONSE_VALUE_CB_ID:
      ret = zb_cmd_ias_zone_enroll_response_handler((esp_zb_zcl_ias_zone_enroll_response_message_t *)message);
      break;
    case ESP_ZB_CORE_WINDOW_COVERING_MOVEMENT_CB_ID:
      ret = zb_window_covering_movement_resp_handler((esp_zb_zcl_window_covering_movement_message_t *)message);
      break;
    case ESP_ZB_CORE_OTA_UPGRADE_VALUE_CB_ID: ret = zb_ota_upgrade_status_handler((esp_zb_zcl_ota_upgrade_value_message_t *)message); break;
    case ESP_ZB_CORE_OTA_UPGRADE_QUERY_IMAGE_RESP_CB_ID:
      ret = zb_ota_upgrade_query_image_resp_handler((esp_zb_zcl_ota_upgrade_query_image_resp_message_t *)message);
      break;
    case ESP_ZB_CORE_CMD_DEFAULT_RESP_CB_ID: ret = zb_cmd_default_resp_handler((esp_zb_zcl_cmd_default_resp_message_t *)message); break;
    default:                                 log_w("Receive unhandled Zigbee action(0x%x) callback", callback_id); break;
  }
  return ret;
}

static esp_err_t zb_attribute_set_handler(const esp_zb_zcl_set_attr_value_message_t *message) {
  if (!message) {
    log_e("Empty message");
    return ESP_FAIL;
  }
  if (message->info.status != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Received message: error status(%d)", message->info.status);
    return ESP_ERR_INVALID_ARG;
  }

  log_v(
    "Received message: endpoint(%d), cluster(0x%x), attribute(0x%x), data size(%d)", message->info.dst_endpoint, message->info.cluster, message->attribute.id,
    message->attribute.data.size
  );

  // List through all Zigbee EPs and call the callback function, with the message
  for (std::list<ZigbeeEP *>::iterator it = Zigbee.ep_objects.begin(); it != Zigbee.ep_objects.end(); ++it) {
    if (message->info.dst_endpoint == (*it)->getEndpoint()) {
      if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY) {
        (*it)->zbIdentify(message);  //method zbIdentify implemented in the common EP class
      } else {
        (*it)->zbAttributeSet(message);  //method zbAttributeSet must be implemented in specific EP class
      }
    }
  }
  return ESP_OK;
}

static esp_err_t zb_attribute_reporting_handler(const esp_zb_zcl_report_attr_message_t *message) {
  if (!message) {
    log_e("Empty message");
    return ESP_FAIL;
  }
  if (message->status != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Received message: error status(%d)", message->status);
    return ESP_ERR_INVALID_ARG;
  }
  log_v(
    "Received report from address(0x%x) src endpoint(%d) to dst endpoint(%d) cluster(0x%x)", message->src_address.u.short_addr, message->src_endpoint,
    message->dst_endpoint, message->cluster
  );
  // List through all Zigbee EPs and call the callback function, with the message
  for (std::list<ZigbeeEP *>::iterator it = Zigbee.ep_objects.begin(); it != Zigbee.ep_objects.end(); ++it) {
    if (message->dst_endpoint == (*it)->getEndpoint()) {
      (*it)->zbAttributeRead(
        message->cluster, &message->attribute, message->src_endpoint, message->src_address
      );  //method zbAttributeRead must be implemented in specific EP class
    }
  }
  return ESP_OK;
}

static esp_err_t zb_cmd_read_attr_resp_handler(const esp_zb_zcl_cmd_read_attr_resp_message_t *message) {
  if (!message) {
    log_e("Empty message");
    return ESP_FAIL;
  }
  if (message->info.status != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Received message: error status(%d)", message->info.status);
    return ESP_ERR_INVALID_ARG;
  }
  log_v(
    "Read attribute response: from address(0x%x) src endpoint(%d) to dst endpoint(%d) cluster(0x%x)", message->info.src_address.u.short_addr,
    message->info.src_endpoint, message->info.dst_endpoint, message->info.cluster
  );

  for (std::list<ZigbeeEP *>::iterator it = Zigbee.ep_objects.begin(); it != Zigbee.ep_objects.end(); ++it) {
    if (message->info.dst_endpoint == (*it)->getEndpoint()) {
      esp_zb_zcl_read_attr_resp_variable_t *variable = message->variables;
      while (variable) {
        log_v(
          "Read attribute response: status(%d), cluster(0x%x), attribute(0x%x), type(0x%x), value(%d)", variable->status, message->info.cluster,
          variable->attribute.id, variable->attribute.data.type, variable->attribute.data.value ? *(uint8_t *)variable->attribute.data.value : 0
        );
        if (variable->status == ESP_ZB_ZCL_STATUS_SUCCESS) {
          if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_BASIC) {
            (*it)->zbReadBasicCluster(&variable->attribute);  //method zbReadBasicCluster implemented in the common EP class
          } else if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_TIME) {
            (*it)->zbReadTimeCluster(&variable->attribute);  //method zbReadTimeCluster implemented in the common EP class
          } else {
            (*it)->zbAttributeRead(
              message->info.cluster, &variable->attribute, message->info.src_endpoint, message->info.src_address
            );  //method zbAttributeRead must be implemented in specific EP class
          }
        }
        variable = variable->next;
      }
    }
  }
  return ESP_OK;
}

static esp_err_t zb_configure_report_resp_handler(const esp_zb_zcl_cmd_config_report_resp_message_t *message) {
  if (!message) {
    log_e("Empty message");
    return ESP_FAIL;
  }
  if (message->info.status != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Received message: error status(%d)", message->info.status);
    return ESP_ERR_INVALID_ARG;
  }
  esp_zb_zcl_config_report_resp_variable_t *variable = message->variables;
  while (variable) {
    log_v(
      "Configure report response: status(%d), cluster(0x%x), direction(0x%x), attribute(0x%x)", variable->status, message->info.cluster, variable->direction,
      variable->attribute_id
    );
    variable = variable->next;
  }
  return ESP_OK;
}

static esp_err_t zb_cmd_ias_zone_status_change_handler(const esp_zb_zcl_ias_zone_status_change_notification_message_t *message) {
  if (!message) {
    log_e("Empty message");
    return ESP_FAIL;
  }
  if (message->info.status != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Received message: error status(%d)", message->info.status);
    return ESP_ERR_INVALID_ARG;
  }
  log_v(
    "IAS Zone Status Notification: from address(0x%x) src endpoint(%d) to dst endpoint(%d) cluster(0x%x)", message->info.src_address.u.short_addr,
    message->info.src_endpoint, message->info.dst_endpoint, message->info.cluster
  );

  for (std::list<ZigbeeEP *>::iterator it = Zigbee.ep_objects.begin(); it != Zigbee.ep_objects.end(); ++it) {
    if (message->info.dst_endpoint == (*it)->getEndpoint()) {
      (*it)->zbIASZoneStatusChangeNotification(message);
    }
  }
  return ESP_OK;
}

static esp_err_t zb_cmd_ias_zone_enroll_response_handler(const esp_zb_zcl_ias_zone_enroll_response_message_t *message) {
  if (!message) {
    log_e("Empty message");
    return ESP_FAIL;
  }
  if (message->info.status != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Received message: error status(%d)", message->info.status);
    return ESP_ERR_INVALID_ARG;
  }
  log_v("IAS Zone Enroll Response received");
  for (std::list<ZigbeeEP *>::iterator it = Zigbee.ep_objects.begin(); it != Zigbee.ep_objects.end(); ++it) {
    if (message->info.dst_endpoint == (*it)->getEndpoint()) {
      (*it)->zbIASZoneEnrollResponse(message);
    }
  }
  return ESP_OK;
}

static esp_err_t zb_window_covering_movement_resp_handler(const esp_zb_zcl_window_covering_movement_message_t *message) {
  if (!message) {
    log_e("Empty message");
  }
  if (message->info.status != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Received message: error status(%d)", message->info.status);
  }

  log_v(
    "Received message: endpoint(%d), cluster(0x%x), command(0x%x), payload(%d)", message->info.dst_endpoint, message->info.cluster, message->command,
    message->payload
  );

  // List through all Zigbee EPs and call the callback function, with the message
  for (std::list<ZigbeeEP *>::iterator it = Zigbee.ep_objects.begin(); it != Zigbee.ep_objects.end(); ++it) {
    if (message->info.dst_endpoint == (*it)->getEndpoint()) {
      (*it)->zbWindowCoveringMovementCmd(message);  //method zbWindowCoveringMovementCmd must be implemented in specific EP class
    }
  }
  return ESP_OK;
}

static esp_err_t esp_element_ota_data(uint32_t total_size, const void *payload, uint16_t payload_size, void **outbuf, uint16_t *outlen) {
  static uint16_t tagid = 0;
  void *data_buf = NULL;
  uint16_t data_len;

  if (!s_tagid_received) {
    uint32_t length = 0;
    if (!payload || payload_size <= OTA_ELEMENT_HEADER_LEN) {
      log_e("Invalid element format");
      return ESP_ERR_INVALID_ARG;
    }

    const uint8_t *payload_ptr = (const uint8_t *)payload;
    tagid = *(const uint16_t *)payload_ptr;
    length = *(const uint32_t *)(payload_ptr + sizeof(tagid));
    if ((length + OTA_ELEMENT_HEADER_LEN) != total_size) {
      log_e("Invalid element length [%ld/%ld]", length, total_size);
      return ESP_ERR_INVALID_ARG;
    }

    s_tagid_received = true;

    data_buf = (void *)(payload_ptr + OTA_ELEMENT_HEADER_LEN);
    data_len = payload_size - OTA_ELEMENT_HEADER_LEN;
  } else {
    data_buf = (void *)payload;
    data_len = payload_size;
  }

  switch (tagid) {
    case UPGRADE_IMAGE:
      *outbuf = data_buf;
      *outlen = data_len;
      break;
    default:
      log_e("Unsupported element tag identifier %d", tagid);
      return ESP_ERR_INVALID_ARG;
      break;
  }

  return ESP_OK;
}

static esp_err_t zb_ota_upgrade_status_handler(const esp_zb_zcl_ota_upgrade_value_message_t *message) {
  static uint32_t total_size = 0;
  static uint32_t offset = 0;
  [[maybe_unused]]
  static int64_t start_time = 0;
  esp_err_t ret = ESP_OK;

  if (message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS) {
    switch (message->upgrade_status) {
      case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_START:
        log_i("Zigbee - OTA upgrade start");
        start_time = esp_timer_get_time();
        s_ota_partition = esp_ota_get_next_update_partition(NULL);
        assert(s_ota_partition);
#if CONFIG_ZB_DELTA_OTA
        ret = esp_delta_ota_begin(s_ota_partition, 0, &s_ota_handle);
#else
        ret = esp_ota_begin(s_ota_partition, 0, &s_ota_handle);
#endif
        if (ret != ESP_OK) {
          log_e("Zigbee - Failed to begin OTA partition, status: %s", esp_err_to_name(ret));
          return ret;
        }
        break;
      case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_RECEIVE:
        total_size = message->ota_header.image_size;
        offset += message->payload_size;
        log_i("Zigbee - OTA Client receives data: progress [%ld/%ld]", offset, total_size);
        if (message->payload_size && message->payload) {
          uint16_t payload_size = 0;
          void *payload = NULL;
          ret = esp_element_ota_data(total_size, message->payload, message->payload_size, &payload, &payload_size);
          if (ret != ESP_OK) {
            log_e("Zigbee - Failed to element OTA data, status: %s", esp_err_to_name(ret));
            return ret;
          }
#if CONFIG_ZB_DELTA_OTA
          ret = esp_delta_ota_write(s_ota_handle, payload, payload_size);
#else
          ret = esp_ota_write(s_ota_handle, (const void *)payload, payload_size);
#endif
          if (ret != ESP_OK) {
            log_e("Zigbee - Failed to write OTA data to partition, status: %s", esp_err_to_name(ret));
            return ret;
          }
        }
        break;
      case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_APPLY: log_i("Zigbee - OTA upgrade apply"); break;
      case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_CHECK:
        ret = offset == total_size ? ESP_OK : ESP_FAIL;
        offset = 0;
        total_size = 0;
        s_tagid_received = false;
        log_i("Zigbee - OTA upgrade check status: %s", esp_err_to_name(ret));
        break;
      case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_FINISH:
        log_i("Zigbee - OTA Finish");
        log_i(
          "Zigbee - OTA Information: version: 0x%lx, manufacturer code: 0x%x, image type: 0x%x, total size: %ld bytes, cost time: %lld ms,",
          message->ota_header.file_version, message->ota_header.manufacturer_code, message->ota_header.image_type, message->ota_header.image_size,
          (esp_timer_get_time() - start_time) / 1000
        );
#if CONFIG_ZB_DELTA_OTA
        ret = esp_delta_ota_end(s_ota_handle);
#else
        ret = esp_ota_end(s_ota_handle);
#endif
        if (ret != ESP_OK) {
          log_e("Zigbee - Failed to end OTA partition, status: %s", esp_err_to_name(ret));
          return ret;
        }
        ret = esp_ota_set_boot_partition(s_ota_partition);
        if (ret != ESP_OK) {
          log_e("Zigbee - Failed to set OTA boot partition, status: %s", esp_err_to_name(ret));
          return ret;
        }
        log_w("Zigbee - Prepare to restart system");
        esp_restart();
        break;
      default: log_i("Zigbee - OTA status: %d", message->upgrade_status); break;
    }
  }
  return ret;
}

static esp_err_t zb_ota_upgrade_query_image_resp_handler(const esp_zb_zcl_ota_upgrade_query_image_resp_message_t *message) {
  if (message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_i("Zigbee - Queried OTA image from address: 0x%04hx, endpoint: %d", message->server_addr.u.short_addr, message->server_endpoint);
    log_i("Zigbee - Image version: 0x%lx, manufacturer code: 0x%x, image size: %ld", message->file_version, message->manufacturer_code, message->image_size);
    if (message->image_size == 0) {
      log_i("Zigbee - Rejecting OTA image upgrade, image size is 0");
      return ESP_FAIL;
    }
    if (message->file_version == 0) {
      log_i("Zigbee - Rejecting OTA image upgrade, file version is 0");
      return ESP_FAIL;
    }
    log_i("Zigbee - Approving OTA image upgrade");
  } else {
    log_i("Zigbee - OTA image upgrade response status: 0x%x", message->info.status);
  }
  return ESP_OK;
}

static esp_err_t zb_cmd_default_resp_handler(const esp_zb_zcl_cmd_default_resp_message_t *message) {
  if (!message) {
    log_e("Empty message");
    return ESP_FAIL;
  }
  if (message->info.status != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Received message: error status(%d)", message->info.status);
    return ESP_ERR_INVALID_ARG;
  }
  log_v(
    "Received default response: from address(0x%x), src_endpoint(%d) to dst_endpoint(%d), cluster(0x%x) with status 0x%x",
    message->info.src_address.u.short_addr, message->info.src_endpoint, message->info.dst_endpoint, message->info.cluster, message->status_code
  );
  return ESP_OK;
}

#endif  // CONFIG_ZB_ENABLED
