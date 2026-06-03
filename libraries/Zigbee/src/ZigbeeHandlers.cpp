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

#include "Arduino.h"

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
static esp_err_t zb_attribute_set_handler(const ezb_zcl_set_attr_value_message_t *message);
static esp_err_t zb_attribute_reporting_handler(const ezb_zcl_cmd_report_attr_message_t *message);
static esp_err_t zb_cmd_read_attr_resp_handler(const ezb_zcl_cmd_read_attr_rsp_message_t *message);
static esp_err_t zb_cmd_write_attr_resp_handler(const ezb_zcl_cmd_write_attr_rsp_message_t *message);
static esp_err_t zb_configure_report_resp_handler(const ezb_zcl_cmd_config_report_rsp_message_t *message);
static esp_err_t zb_cmd_ias_zone_status_change_handler(const ezb_zcl_ias_zone_status_change_notif_message_t *message);
static esp_err_t zb_cmd_ias_zone_enroll_response_handler(const ezb_zcl_ias_zone_enroll_rsp_message_t *message);
static esp_err_t zb_cmd_default_resp_handler(const ezb_zcl_cmd_default_rsp_message_t *message);
static esp_err_t zb_window_covering_movement_resp_handler(const ezb_zcl_window_covering_movement_message_t *message);
static esp_err_t zb_ota_upgrade_status_handler(const ezb_zcl_ota_upgrade_client_progress_message_t *message);
static esp_err_t zb_ota_upgrade_query_image_resp_handler(ezb_zcl_ota_upgrade_query_next_image_rsp_message_t *message);
static esp_err_t zb_manuf_spec_command_handler(const ezb_zcl_manuf_spec_cmd_message_t *message);

// Zigbee ZCL core action handler (v2.x).
// NOTE(zb-v2): The callback signature changed: it now returns void and receives a non-const message.
// Status is communicated back to the stack via message->out.result (left at its default here except
// where a handler explicitly rejects, e.g. the OTA query-image response). The internal esp_err_t
// returns below are kept only for local control flow/logging.
static void zb_action_handler(ezb_zcl_core_action_callback_id_t callback_id, void *message) {
  switch (callback_id) {
    case EZB_ZCL_CORE_SET_ATTR_VALUE_CB_ID:   zb_attribute_set_handler((ezb_zcl_set_attr_value_message_t *)message); break;
    case EZB_ZCL_CORE_REPORT_ATTR_CB_ID:      zb_attribute_reporting_handler((ezb_zcl_cmd_report_attr_message_t *)message); break;
    case EZB_ZCL_CORE_READ_ATTR_RSP_CB_ID:    zb_cmd_read_attr_resp_handler((ezb_zcl_cmd_read_attr_rsp_message_t *)message); break;
    case EZB_ZCL_CORE_WRITE_ATTR_RSP_CB_ID:   zb_cmd_write_attr_resp_handler((ezb_zcl_cmd_write_attr_rsp_message_t *)message); break;
    case EZB_ZCL_CORE_CONFIG_REPORT_RSP_CB_ID: zb_configure_report_resp_handler((ezb_zcl_cmd_config_report_rsp_message_t *)message); break;
    case EZB_ZCL_CORE_IAS_ZONE_STATUS_CHANGE_NOTIF_CB_ID:
      zb_cmd_ias_zone_status_change_handler((ezb_zcl_ias_zone_status_change_notif_message_t *)message);
      break;
    case EZB_ZCL_CORE_IAS_ZONE_ENROLL_RSP_CB_ID:
      zb_cmd_ias_zone_enroll_response_handler((ezb_zcl_ias_zone_enroll_rsp_message_t *)message);
      break;
    case EZB_ZCL_CORE_WINDOW_COVERING_MOVEMENT_CB_ID:
      zb_window_covering_movement_resp_handler((ezb_zcl_window_covering_movement_message_t *)message);
      break;
    case EZB_ZCL_CORE_OTA_UPGRADE_CLIENT_PROGRESS_CB_ID:
      zb_ota_upgrade_status_handler((ezb_zcl_ota_upgrade_client_progress_message_t *)message);
      break;
    case EZB_ZCL_CORE_OTA_UPGRADE_QUERY_NEXT_IMAGE_RSP_CB_ID:
      zb_ota_upgrade_query_image_resp_handler((ezb_zcl_ota_upgrade_query_next_image_rsp_message_t *)message);
      break;
    case EZB_ZCL_CORE_DEFAULT_RSP_CB_ID:    zb_cmd_default_resp_handler((ezb_zcl_cmd_default_rsp_message_t *)message); break;
    // NOTE(zb-v2): v1 had separate PRIVILEGE_COMMAND_REQ and CUSTOM_CLUSTER_REQ callbacks; in v2.x both
    // collapse into the single manufacturer-specific command callback.
    case EZB_ZCL_CORE_MANUF_SPEC_CMD_CB_ID: zb_manuf_spec_command_handler((ezb_zcl_manuf_spec_cmd_message_t *)message); break;
    default:                                log_w("Receive unhandled Zigbee action(0x%x) callback", (unsigned int)callback_id); break;
  }
}

static esp_err_t zb_attribute_set_handler(const ezb_zcl_set_attr_value_message_t *message) {
  if (!message) {
    log_e("Empty message");
    return ESP_FAIL;
  }
  if (message->info.status != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Received message: error status(%d)", message->info.status);
    return ESP_ERR_INVALID_ARG;
  }

  log_v(
    "Received message: endpoint(%u), cluster(0x%x), attribute(0x%x), data size(%u)", message->info.dst_ep, message->info.cluster_id, message->in.attribute.id,
    message->in.attribute.data.size
  );

  // List through all Zigbee EPs and call the callback function, with the message
  for (std::list<ZigbeeEP *>::iterator it = Zigbee.ep_objects.begin(); it != Zigbee.ep_objects.end(); ++it) {
    if (message->info.dst_ep == (*it)->getEndpoint()) {
      if (message->info.cluster_id == EZB_ZCL_CLUSTER_ID_IDENTIFY) {
        (*it)->zbIdentify(message);  //method zbIdentify implemented in the common EP class
      } else {
        (*it)->zbAttributeSet(message);  //method zbAttributeSet must be implemented in specific EP class
      }
    }
  }
  return ESP_OK;
}

static esp_err_t zb_attribute_reporting_handler(const ezb_zcl_cmd_report_attr_message_t *message) {
  if (!message) {
    log_e("Empty message");
    return ESP_FAIL;
  }
  if (message->info.status != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Received message: error status(%d)", message->info.status);
    return ESP_ERR_INVALID_ARG;
  }
  const ezb_zcl_cmd_hdr_t *hdr = message->in.header;
  uint8_t src_endpoint = hdr ? hdr->src_ep : 0;
  ezb_address_t src_address = hdr ? hdr->src_addr : EZB_ADDRESS_NONE();
  log_v(
    "Received report from address(0x%x) src endpoint(%u) to dst endpoint(%u) cluster(0x%x)", src_address.u.short_addr, src_endpoint, message->info.dst_ep,
    message->info.cluster_id
  );
  // List through all Zigbee EPs and call the callback function, with the message.
  // NOTE(zb-v2): a single report message can carry multiple attributes (in.variables linked list);
  // we rebuild an ezb_zcl_attribute_t per variable to keep the zbAttributeRead() EP interface unchanged.
  for (std::list<ZigbeeEP *>::iterator it = Zigbee.ep_objects.begin(); it != Zigbee.ep_objects.end(); ++it) {
    if (message->info.dst_ep == (*it)->getEndpoint()) {
      for (ezb_zcl_report_attr_variable_t *variable = message->in.variables; variable != nullptr; variable = variable->next) {
        ezb_zcl_attribute_t attribute = {};
        attribute.id = variable->attr_id;
        attribute.data.type = variable->attr_type;
        attribute.data.size = ezb_zcl_get_attr_value_size((ezb_zcl_attr_type_t)variable->attr_type, variable->attr_value);
        attribute.data.value = variable->attr_value;
        (*it)->zbAttributeRead(
          message->info.cluster_id, &attribute, src_endpoint, src_address
        );  //method zbAttributeRead must be implemented in specific EP class
      }
    }
  }
  return ESP_OK;
}

static esp_err_t zb_cmd_read_attr_resp_handler(const ezb_zcl_cmd_read_attr_rsp_message_t *message) {
  if (!message) {
    log_e("Empty message");
    return ESP_FAIL;
  }
  if (message->info.status != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Received message: error status(%d)", message->info.status);
    return ESP_ERR_INVALID_ARG;
  }
  const ezb_zcl_cmd_hdr_t *hdr = message->in.header;
  uint8_t src_endpoint = hdr ? hdr->src_ep : 0;
  ezb_address_t src_address = hdr ? hdr->src_addr : EZB_ADDRESS_NONE();
  log_v(
    "Read attribute response: from address(0x%x) src endpoint(%u) to dst endpoint(%u) cluster(0x%x)", src_address.u.short_addr, src_endpoint,
    message->info.dst_ep, message->info.cluster_id
  );

  for (std::list<ZigbeeEP *>::iterator it = Zigbee.ep_objects.begin(); it != Zigbee.ep_objects.end(); ++it) {
    if (message->info.dst_ep == (*it)->getEndpoint()) {
      // NOTE(zb-v2): read-attr-rsp variables now expose flat fields (attr_id/status/attr_type/attr_value)
      // instead of a nested ezb_zcl_attribute_t; rebuild one to keep the EP interface unchanged.
      ezb_zcl_read_attr_rsp_variable_t *variable = message->in.variables;
      while (variable) {
        ezb_zcl_attribute_t attribute = {};
        attribute.id = variable->attr_id;
        attribute.data.type = variable->attr_type;
        attribute.data.size = ezb_zcl_get_attr_value_size((ezb_zcl_attr_type_t)variable->attr_type, variable->attr_value);
        attribute.data.value = variable->attr_value;
        log_v(
          "Read attribute response: status(%d), cluster(0x%x), attribute(0x%x), type(0x%x), value(%d)", variable->status, message->info.cluster_id,
          attribute.id, attribute.data.type, attribute.data.value ? *(uint8_t *)attribute.data.value : 0
        );
        if (variable->status == EZB_ZCL_STATUS_SUCCESS) {
          if (message->info.cluster_id == EZB_ZCL_CLUSTER_ID_BASIC) {
            (*it)->zbReadBasicCluster(&attribute);  //method zbReadBasicCluster implemented in the common EP class
          } else if (message->info.cluster_id == EZB_ZCL_CLUSTER_ID_TIME) {
            (*it)->zbReadTimeCluster(&attribute);  //method zbReadTimeCluster implemented in the common EP class
          } else {
            (*it)->zbAttributeRead(
              message->info.cluster_id, &attribute, src_endpoint, src_address
            );  //method zbAttributeRead must be implemented in specific EP class
          }
        }
        variable = variable->next;
      }
    }
  }
  return ESP_OK;
}

static esp_err_t zb_cmd_write_attr_resp_handler(const ezb_zcl_cmd_write_attr_rsp_message_t *message) {
  if (!message) {
    log_e("Empty message");
    return ESP_FAIL;
  }
  if (message->info.status != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Received message: error status(%d)", message->info.status);
    return ESP_ERR_INVALID_ARG;
  }
  const ezb_zcl_cmd_hdr_t *hdr = message->in.header;
  uint8_t src_endpoint = hdr ? hdr->src_ep : 0;
  ezb_address_t src_address = hdr ? hdr->src_addr : EZB_ADDRESS_NONE();
  log_v(
    "Write attribute response: from address(0x%x) src endpoint(%u) to dst endpoint(%u) cluster(0x%x)", src_address.u.short_addr, src_endpoint,
    message->info.dst_ep, message->info.cluster_id
  );
  for (std::list<ZigbeeEP *>::iterator it = Zigbee.ep_objects.begin(); it != Zigbee.ep_objects.end(); ++it) {
    if (message->info.dst_ep == (*it)->getEndpoint()) {
      ezb_zcl_write_attr_rsp_variable_t *variable = message->in.variables;
      while (variable) {
        log_v("Write attribute response: status(%d), cluster(0x%x), attribute(0x%x)", variable->status, message->info.cluster_id, variable->attr_id);
        if (variable->status == EZB_ZCL_STATUS_SUCCESS) {
          (*it)->zbWriteAttributeResponse(message->info.cluster_id, variable->attr_id, variable->status, src_endpoint, src_address);
        }
        variable = variable->next;
      }
    }
  }
  return ESP_OK;
}

static esp_err_t zb_configure_report_resp_handler(const ezb_zcl_cmd_config_report_rsp_message_t *message) {
  if (!message) {
    log_e("Empty message");
    return ESP_FAIL;
  }
  if (message->info.status != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Received message: error status(%d)", message->info.status);
    return ESP_ERR_INVALID_ARG;
  }
  ezb_zcl_config_report_rsp_variable_t *variable = message->in.variables;
  while (variable) {
    log_v(
      "Configure report response: status(%d), cluster(0x%x), direction(0x%x), attribute(0x%x)", variable->status, message->info.cluster_id,
      variable->direction, variable->attr_id
    );
    variable = variable->next;
  }
  return ESP_OK;
}

static esp_err_t zb_cmd_ias_zone_status_change_handler(const ezb_zcl_ias_zone_status_change_notif_message_t *message) {
  if (!message) {
    log_e("Empty message");
    return ESP_FAIL;
  }
  if (message->info.status != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Received message: error status(%d)", message->info.status);
    return ESP_ERR_INVALID_ARG;
  }
  const ezb_zcl_cmd_hdr_t *hdr = message->in.header;
  log_v(
    "IAS Zone Status Notification: from address(0x%x) src endpoint(%u) to dst endpoint(%u) cluster(0x%x)", hdr ? hdr->src_addr.u.short_addr : 0,
    hdr ? hdr->src_ep : 0, message->info.dst_ep, message->info.cluster_id
  );

  for (std::list<ZigbeeEP *>::iterator it = Zigbee.ep_objects.begin(); it != Zigbee.ep_objects.end(); ++it) {
    if (message->info.dst_ep == (*it)->getEndpoint()) {
      (*it)->zbIASZoneStatusChangeNotification(message);
    }
  }
  return ESP_OK;
}

static esp_err_t zb_cmd_ias_zone_enroll_response_handler(const ezb_zcl_ias_zone_enroll_rsp_message_t *message) {
  if (!message) {
    log_e("Empty message");
    return ESP_FAIL;
  }
  if (message->info.status != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Received message: error status(%d)", message->info.status);
    return ESP_ERR_INVALID_ARG;
  }
  log_v("IAS Zone Enroll Response received");
  for (std::list<ZigbeeEP *>::iterator it = Zigbee.ep_objects.begin(); it != Zigbee.ep_objects.end(); ++it) {
    if (message->info.dst_ep == (*it)->getEndpoint()) {
      (*it)->zbIASZoneEnrollResponse(message);
    }
  }
  return ESP_OK;
}

static esp_err_t zb_window_covering_movement_resp_handler(const ezb_zcl_window_covering_movement_message_t *message) {
  if (!message) {
    log_e("Empty message");
    return ESP_FAIL;
  }
  if (message->info.status != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Received message: error status(%d)", message->info.status);
  }

  // NOTE(zb-v2): the movement command ID now lives in the ZCL header (in.header->cmd_id); the payload
  // is a union (lift/tilt value or percentage) decoded by the EP based on the command ID.
  const ezb_zcl_cmd_hdr_t *hdr = message->in.header;
  log_v(
    "Received message: endpoint(%u), cluster(0x%x), command(0x%x), payload(%u)", message->info.dst_ep, message->info.cluster_id, hdr ? hdr->cmd_id : 0,
    message->in.payload.lift_value
  );

  // List through all Zigbee EPs and call the callback function, with the message
  for (std::list<ZigbeeEP *>::iterator it = Zigbee.ep_objects.begin(); it != Zigbee.ep_objects.end(); ++it) {
    if (message->info.dst_ep == (*it)->getEndpoint()) {
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
      log_e("Invalid element length [%" PRIu32 "/%" PRIu32 "]", length, total_size);
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
      log_e("Unsupported element tag identifier %u", tagid);
      return ESP_ERR_INVALID_ARG;
      break;
  }

  return ESP_OK;
}

// NOTE(zb-v2): The OTA client model was redesigned. The v1 single STATUS callback (with ota_header +
// raw payload) is replaced by a progress enum (start/receiving/check/apply/finish/abort) carrying a
// union of per-phase data. The flash-write flow below is ported faithfully; the OTA sub-element header
// stripping (esp_element_ota_data) is retained since the OTA file sub-element format is spec-defined and
// independent of the SDK version.
static esp_err_t zb_ota_upgrade_status_handler(const ezb_zcl_ota_upgrade_client_progress_message_t *message) {
  static uint32_t total_size = 0;
  static uint32_t offset = 0;
  [[maybe_unused]]
  static int64_t start_time = 0;
  esp_err_t ret = ESP_OK;

  if (message->info.status != EZB_ZCL_STATUS_SUCCESS) {
    return ESP_OK;
  }

  switch (message->in.progress) {
    case EZB_ZCL_OTA_UPGRADE_PROGRESS_START:
      log_i("Zigbee - OTA upgrade start");
      total_size = message->in.start.image_size;
      offset = 0;
      for (std::list<ZigbeeEP *>::iterator it = Zigbee.ep_objects.begin(); it != Zigbee.ep_objects.end(); ++it) {
        (*it)->zbOTAState(true);  // Notify that OTA is active
      }
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
        for (std::list<ZigbeeEP *>::iterator it = Zigbee.ep_objects.begin(); it != Zigbee.ep_objects.end(); ++it) {
          (*it)->zbOTAState(false);  // Notify that OTA is no longer active
        }
        return ret;
      }
      break;
    case EZB_ZCL_OTA_UPGRADE_PROGRESS_RECEIVING: {
      uint8_t block_size = message->in.receiving.block_size;
      uint8_t *block = message->in.receiving.block;
      offset += block_size;
      log_i("Zigbee - OTA Client receives data: progress [%" PRIu32 "/%" PRIu32 "]", offset, total_size);
      if (block_size && block) {
        uint16_t payload_size = 0;
        void *payload = NULL;
        ret = esp_element_ota_data(total_size, block, block_size, &payload, &payload_size);
        if (ret != ESP_OK) {
          log_e("Zigbee - Failed to element OTA data, status: %s", esp_err_to_name(ret));
          for (std::list<ZigbeeEP *>::iterator it = Zigbee.ep_objects.begin(); it != Zigbee.ep_objects.end(); ++it) {
            (*it)->zbOTAState(false);  // Notify that OTA is no longer active
          }
          return ret;
        }
#if CONFIG_ZB_DELTA_OTA
        ret = esp_delta_ota_write(s_ota_handle, payload, payload_size);
#else
        ret = esp_ota_write(s_ota_handle, (const void *)payload, payload_size);
#endif
        if (ret != ESP_OK) {
          log_e("Zigbee - Failed to write OTA data to partition, status: %s", esp_err_to_name(ret));
          for (std::list<ZigbeeEP *>::iterator it = Zigbee.ep_objects.begin(); it != Zigbee.ep_objects.end(); ++it) {
            (*it)->zbOTAState(false);  // Notify that OTA is no longer active
          }
          return ret;
        }
      }
      break;
    }
    case EZB_ZCL_OTA_UPGRADE_PROGRESS_APPLY: log_i("Zigbee - OTA upgrade apply"); break;
    case EZB_ZCL_OTA_UPGRADE_PROGRESS_CHECK:
      ret = offset == total_size ? ESP_OK : ESP_FAIL;
      offset = 0;
      total_size = 0;
      s_tagid_received = false;
      log_i("Zigbee - OTA upgrade check status: %s", esp_err_to_name(ret));
      break;
    case EZB_ZCL_OTA_UPGRADE_PROGRESS_FINISH:
      log_i("Zigbee - OTA Finish");
      log_i(
        "Zigbee - OTA Information: total size: %" PRIu32 " bytes, cost time: %" PRId32 " ms,", offset,
        (int32_t)((esp_timer_get_time() - start_time) / 1000)
      );
      for (std::list<ZigbeeEP *>::iterator it = Zigbee.ep_objects.begin(); it != Zigbee.ep_objects.end(); ++it) {
        (*it)->zbOTAState(false);  // Notify that OTA is no longer active
      }
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
    case EZB_ZCL_OTA_UPGRADE_PROGRESS_ABORT:
      log_w("Zigbee - OTA upgrade aborted");
      offset = 0;
      total_size = 0;
      s_tagid_received = false;
      for (std::list<ZigbeeEP *>::iterator it = Zigbee.ep_objects.begin(); it != Zigbee.ep_objects.end(); ++it) {
        (*it)->zbOTAState(false);  // Notify that OTA is no longer active
      }
      break;
    default: log_i("Zigbee - OTA progress: %d", message->in.progress); break;
  }
  return ret;
}

static esp_err_t zb_ota_upgrade_query_image_resp_handler(ezb_zcl_ota_upgrade_query_next_image_rsp_message_t *message) {
  if (message->info.status == EZB_ZCL_STATUS_SUCCESS) {
    const ezb_zcl_ota_upgrade_new_image_t *image = &message->in.image;
    log_i("Zigbee - Queried OTA image from endpoint: %u", image->ep_id);
    log_i(
      "Zigbee - Image version: 0x%08" PRIx32 ", manufacturer code: 0x%04x, image size: %" PRIu32, image->file_version, image->manuf_code, image->size
    );
    if (image->size == 0) {
      log_i("Zigbee - Rejecting OTA image upgrade, image size is 0");
      message->out.result = EZB_ZCL_STATUS_FAIL;
      return ESP_FAIL;
    }
    if (image->file_version == 0) {
      log_i("Zigbee - Rejecting OTA image upgrade, file version is 0");
      message->out.result = EZB_ZCL_STATUS_FAIL;
      return ESP_FAIL;
    }
    log_i("Zigbee - Approving OTA image upgrade");
  } else {
    log_i("Zigbee - OTA image upgrade response status: 0x%x", message->info.status);
  }
  return ESP_OK;
}

static esp_err_t zb_cmd_default_resp_handler(const ezb_zcl_cmd_default_rsp_message_t *message) {
  if (!message) {
    log_e("Empty message");
    return ESP_FAIL;
  }
  if (message->info.status != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Received message: error status(%d)", message->info.status);
    return ESP_ERR_INVALID_ARG;
  }
  const ezb_zcl_cmd_hdr_t *hdr = message->in.header;
  log_v(
    "Received default response: from address(0x%x), src_endpoint(%u) to dst_endpoint(%u), cluster(0x%x) with status 0x%x", hdr ? hdr->src_addr.u.short_addr : 0,
    hdr ? hdr->src_ep : 0, message->info.dst_ep, message->info.cluster_id, message->in.status_code
  );

  // Call global callback if set
  Zigbee.callDefaultResponseCallback((zb_cmd_type_t)message->in.rsp_to_cmd, (ezb_zcl_status_t)message->in.status_code, message->info.dst_ep, message->info.cluster_id);

  // List through all Zigbee EPs and call the callback function, with the message
  for (std::list<ZigbeeEP *>::iterator it = Zigbee.ep_objects.begin(); it != Zigbee.ep_objects.end(); ++it) {
    if (message->info.dst_ep == (*it)->getEndpoint()) {
      (*it)->zbDefaultResponse(message);  //method zbDefaultResponse is implemented in the common EP class
    }
  }
  return ESP_OK;
}

// NOTE(zb-v2): v1's separate privilege-command and custom-cluster-command callbacks are merged into the
// single manufacturer-specific command callback. We dispatch to both EP hooks so that endpoints relying
// on either zbCustomClusterCommand() or zbPrivilegeCommand() continue to receive the command.
static esp_err_t zb_manuf_spec_command_handler(const ezb_zcl_manuf_spec_cmd_message_t *message) {
  if (!message) {
    log_e("Empty message");
    return ESP_FAIL;
  }
  if (message->info.status != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Received message: error status(%d)", message->info.status);
    return ESP_ERR_INVALID_ARG;
  }
  const ezb_zcl_cmd_hdr_t *hdr = message->in.header;
  log_v(
    "Manufacturer-specific command: from address(0x%x) src endpoint(%u) to dst endpoint(%u) cluster(0x%x) command(0x%x)", hdr ? hdr->src_addr.u.short_addr : 0,
    hdr ? hdr->src_ep : 0, message->info.dst_ep, message->info.cluster_id, hdr ? hdr->cmd_id : 0
  );

  for (std::list<ZigbeeEP *>::iterator it = Zigbee.ep_objects.begin(); it != Zigbee.ep_objects.end(); ++it) {
    if (message->info.dst_ep == (*it)->getEndpoint()) {
      (*it)->zbCustomClusterCommand(message);
      (*it)->zbPrivilegeCommand(message);
    }
  }
  return ESP_OK;
}

#endif  // CONFIG_ZB_ENABLED
