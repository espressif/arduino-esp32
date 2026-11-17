// Copyright 2026 Espressif Systems (Shanghai) PTE LTD
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

/* Zigbee Common Functions */
#include "ZigbeeCore.h"
#include "Arduino.h"

#if CONFIG_ZB_ENABLED

#include "esp_ota_ops.h"
#if CONFIG_ZB_DELTA_OTA  // Delta OTA, code is prepared for this feature but not enabled by default
#include "esp_delta_ota_ops.h"
#endif
#include "ZigbeeOTAFileParser.h"

static const esp_partition_t *s_ota_partition = NULL;
static esp_ota_handle_t s_ota_handle = 0;
static zb_ota_file_parser_t s_ota_file_parser;
static bool s_ota_file_parser_active = false;
static bool s_ota_notify_active = false;
static bool s_ota_receive_failed = false;       // flash/parse failed; wait for stack ABORT
static uint32_t s_ota_approved_image_size = 0;  // Query image size; used if RECEIVING races START

static void zb_ota_notify_state(bool active) {
  if (s_ota_notify_active == active) {
    return;
  }
  s_ota_notify_active = active;
  for (std::list<ZigbeeEP *>::iterator it = Zigbee.ep_objects.begin(); it != Zigbee.ep_objects.end(); ++it) {
    (*it)->zbOTAState(active);
  }
}

static void zb_ota_notify_complete(bool success) {
  zb_ota_notify_state(false);
  for (std::list<ZigbeeEP *>::iterator it = Zigbee.ep_objects.begin(); it != Zigbee.ep_objects.end(); ++it) {
    (*it)->zbOTAComplete(success);
  }
}

static void zb_ota_reset_receive_state() {
  if (s_ota_handle) {
    esp_ota_abort(s_ota_handle);
    s_ota_handle = 0;
  }
  s_ota_partition = NULL;
  s_ota_file_parser_active = false;
  s_ota_receive_failed = false;
  s_ota_approved_image_size = 0;
  memset(&s_ota_file_parser, 0, sizeof(s_ota_file_parser));
}

// Close the flash session after a mid-transfer error. Keep approved size so a trailing
// Image Block (stack may already have queued one) does not try to re-prepare. Full cleanup
// and user notify happen in PROGRESS_ABORT (stack nests ABORT when out.result is ABORT).
static void zb_ota_fail_receive() {
  if (s_ota_handle) {
    esp_ota_abort(s_ota_handle);
    s_ota_handle = 0;
  }
  s_ota_partition = NULL;
  s_ota_file_parser_active = false;
  s_ota_receive_failed = true;
  memset(&s_ota_file_parser, 0, sizeof(s_ota_file_parser));
}

// Open the OTA partition and init the streaming parser. Called from START (and from
// RECEIVING if START was missed). Do not call from the Query approve callback — flash
// work there delays Image Block Request and ZHA times out.
//
// esp_ota_* runs on the ZCL task (same as esp-zigbee-sdk ota_client). Sequential writes
// avoid a full erase in START; keep RECEIVING work short (throttled logs, no Serial spam).
static esp_err_t zb_ota_prepare_receive(uint32_t image_size) {
  if (s_ota_receive_failed) {
    return ESP_ERR_INVALID_STATE;
  }
  if (s_ota_file_parser_active && s_ota_handle != 0) {
    return ESP_OK;
  }
  if (s_ota_handle != 0 || s_ota_file_parser_active) {
    zb_ota_reset_receive_state();
  }

  s_ota_partition = esp_ota_get_next_update_partition(NULL);
  if (s_ota_partition == NULL) {
    log_e("Zigbee - No OTA partition found");
    return ESP_ERR_NOT_FOUND;
  }

  zb_ota_file_parser_init(&s_ota_file_parser, image_size);
#if CONFIG_ZB_DELTA_OTA
  esp_err_t ret = esp_delta_ota_begin(s_ota_partition, 0, &s_ota_handle);
#else
  // Sequential writes: avoid a full-partition erase in START before the first Image Block
  // Request (ZHA aborts if that stalls ~30s). esp-zigbee-sdk ota_client uses size 0 instead.
  esp_err_t ret = esp_ota_begin(s_ota_partition, OTA_WITH_SEQUENTIAL_WRITES, &s_ota_handle);
#endif
  if (ret != ESP_OK) {
    log_e("Zigbee - Failed to begin OTA partition, status: %s", esp_err_to_name(ret));
    zb_ota_reset_receive_state();
    return ret;
  }
  s_ota_file_parser_active = true;
  zb_ota_notify_state(true);
  return ESP_OK;
}

// forward declaration of all implemented handlers
static esp_err_t zb_attribute_set_handler(const ezb_zcl_set_attr_value_message_t *message);
static esp_err_t zb_attribute_reporting_handler(const ezb_zcl_cmd_report_attr_message_t *message);
static esp_err_t zb_cmd_read_attr_resp_handler(const ezb_zcl_cmd_read_attr_rsp_message_t *message);
static esp_err_t zb_cmd_write_attr_resp_handler(const ezb_zcl_cmd_write_attr_rsp_message_t *message);
static esp_err_t zb_configure_report_resp_handler(const ezb_zcl_cmd_config_report_rsp_message_t *message);
static esp_err_t zb_cmd_ias_zone_status_change_handler(const ezb_zcl_ias_zone_status_change_notif_message_t *message);
static esp_err_t zb_cmd_ias_zone_enroll_request_handler(const ezb_zcl_ias_zone_enroll_req_message_t *message);
static esp_err_t zb_cmd_ias_zone_enroll_response_handler(const ezb_zcl_ias_zone_enroll_rsp_message_t *message);
static esp_err_t zb_cmd_default_resp_handler(const ezb_zcl_cmd_default_rsp_message_t *message);
static esp_err_t zb_cmd_identify_query_resp_handler(const ezb_zcl_identify_query_rsp_message_t *message);
static esp_err_t zb_window_covering_movement_resp_handler(const ezb_zcl_window_covering_movement_message_t *message);
static esp_err_t zb_door_lock_cmd_handler(ezb_zcl_door_lock_lock_door_message_t *message, bool lock);
template<typename T> static void zb_door_lock_user_cmd_handler(T *message, void (ZigbeeEP::*handler)(T *));
static esp_err_t zb_ota_upgrade_status_handler(ezb_zcl_ota_upgrade_client_progress_message_t *message);
static esp_err_t zb_ota_upgrade_query_image_resp_handler(ezb_zcl_ota_upgrade_query_next_image_rsp_message_t *message);
static esp_err_t zb_manuf_spec_command_handler(const ezb_zcl_manuf_spec_cmd_message_t *message);

// Zigbee ZCL core action handler.
// Status is communicated back to the stack via message->out.result (left at its default here except
// where a handler explicitly rejects, e.g. the OTA query-image response). The internal esp_err_t
// returns below are kept only for local control flow/logging.
static void zb_action_handler(ezb_zcl_core_action_callback_id_t callback_id, void *message) {
  switch (callback_id) {
    case EZB_ZCL_CORE_SET_ATTR_VALUE_CB_ID:    zb_attribute_set_handler((ezb_zcl_set_attr_value_message_t *)message); break;
    case EZB_ZCL_CORE_REPORT_ATTR_CB_ID:       zb_attribute_reporting_handler((ezb_zcl_cmd_report_attr_message_t *)message); break;
    case EZB_ZCL_CORE_READ_ATTR_RSP_CB_ID:     zb_cmd_read_attr_resp_handler((ezb_zcl_cmd_read_attr_rsp_message_t *)message); break;
    case EZB_ZCL_CORE_WRITE_ATTR_RSP_CB_ID:    zb_cmd_write_attr_resp_handler((ezb_zcl_cmd_write_attr_rsp_message_t *)message); break;
    case EZB_ZCL_CORE_CONFIG_REPORT_RSP_CB_ID: zb_configure_report_resp_handler((ezb_zcl_cmd_config_report_rsp_message_t *)message); break;
    case EZB_ZCL_CORE_IAS_ZONE_STATUS_CHANGE_NOTIF_CB_ID:
      zb_cmd_ias_zone_status_change_handler((ezb_zcl_ias_zone_status_change_notif_message_t *)message);
      break;
    case EZB_ZCL_CORE_IAS_ZONE_ENROLL_CB_ID:          zb_cmd_ias_zone_enroll_request_handler((ezb_zcl_ias_zone_enroll_req_message_t *)message); break;
    case EZB_ZCL_CORE_IAS_ZONE_ENROLL_RSP_CB_ID:      zb_cmd_ias_zone_enroll_response_handler((ezb_zcl_ias_zone_enroll_rsp_message_t *)message); break;
    case EZB_ZCL_CORE_WINDOW_COVERING_MOVEMENT_CB_ID: zb_window_covering_movement_resp_handler((ezb_zcl_window_covering_movement_message_t *)message); break;
    case EZB_ZCL_CORE_DOOR_LOCK_LOCK_DOOR_CB_ID:      zb_door_lock_cmd_handler((ezb_zcl_door_lock_lock_door_message_t *)message, true); break;
    case EZB_ZCL_CORE_DOOR_LOCK_UNLOCK_DOOR_CB_ID:    zb_door_lock_cmd_handler((ezb_zcl_door_lock_lock_door_message_t *)message, false); break;
    case EZB_ZCL_CORE_DOOR_LOCK_SET_PIN_CODE_CB_ID:
      zb_door_lock_user_cmd_handler((ezb_zcl_door_lock_set_pin_code_message_t *)message, &ZigbeeEP::zbDoorLockSetPinCode);
      break;
    case EZB_ZCL_CORE_DOOR_LOCK_GET_PIN_CODE_CB_ID:
      zb_door_lock_user_cmd_handler((ezb_zcl_door_lock_get_pin_code_message_t *)message, &ZigbeeEP::zbDoorLockGetPinCode);
      break;
    case EZB_ZCL_CORE_DOOR_LOCK_CLEAR_PIN_CODE_CB_ID:
      zb_door_lock_user_cmd_handler((ezb_zcl_door_lock_clear_pin_code_message_t *)message, &ZigbeeEP::zbDoorLockClearPinCode);
      break;
    case EZB_ZCL_CORE_DOOR_LOCK_CLEAR_ALL_PIN_CODES_CB_ID:
      zb_door_lock_user_cmd_handler((ezb_zcl_door_lock_clear_all_pin_codes_message_t *)message, &ZigbeeEP::zbDoorLockClearAllPinCodes);
      break;
    case EZB_ZCL_CORE_DOOR_LOCK_SET_USER_STATUS_CB_ID:
      zb_door_lock_user_cmd_handler((ezb_zcl_door_lock_set_user_status_message_t *)message, &ZigbeeEP::zbDoorLockSetUserStatus);
      break;
    case EZB_ZCL_CORE_DOOR_LOCK_GET_USER_STATUS_CB_ID:
      zb_door_lock_user_cmd_handler((ezb_zcl_door_lock_get_user_status_message_t *)message, &ZigbeeEP::zbDoorLockGetUserStatus);
      break;
    case EZB_ZCL_CORE_OTA_UPGRADE_CLIENT_PROGRESS_CB_ID: zb_ota_upgrade_status_handler((ezb_zcl_ota_upgrade_client_progress_message_t *)message); break;
    case EZB_ZCL_CORE_OTA_UPGRADE_QUERY_NEXT_IMAGE_RSP_CB_ID:
      zb_ota_upgrade_query_image_resp_handler((ezb_zcl_ota_upgrade_query_next_image_rsp_message_t *)message);
      break;
    case EZB_ZCL_CORE_DEFAULT_RSP_CB_ID:        zb_cmd_default_resp_handler((ezb_zcl_cmd_default_rsp_message_t *)message); break;
    case EZB_ZCL_CORE_IDENTIFY_QUERY_RSP_CB_ID: zb_cmd_identify_query_resp_handler((ezb_zcl_identify_query_rsp_message_t *)message); break;
    case EZB_ZCL_CORE_MANUF_SPEC_CMD_CB_ID:     zb_manuf_spec_command_handler((ezb_zcl_manuf_spec_cmd_message_t *)message); break;
    default:                                    log_w("Receive unhandled Zigbee action(0x%x) callback", (unsigned int)callback_id); break;
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
  // A single report can carry multiple attributes (in.variables); rebuild one ezb_zcl_attribute_t each.
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
      // Rebuild a nested ezb_zcl_attribute_t from the flat read-attr-rsp variable fields.
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
      "Configure report response: status(%d), cluster(0x%x), direction(0x%x), attribute(0x%x)", variable->status, message->info.cluster_id, variable->direction,
      variable->attr_id
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
  log_v(
    "IAS Zone Status Notification: from address(0x%x) src endpoint(%u) to dst endpoint(%u) cluster(0x%x)",
    message->in.header ? message->in.header->src_addr.u.short_addr : 0, message->in.header ? message->in.header->src_ep : 0, message->info.dst_ep,
    message->info.cluster_id
  );

  for (std::list<ZigbeeEP *>::iterator it = Zigbee.ep_objects.begin(); it != Zigbee.ep_objects.end(); ++it) {
    if (message->info.dst_ep == (*it)->getEndpoint()) {
      (*it)->zbIASZoneStatusChangeNotification(message);
    }
  }
  return ESP_OK;
}

static esp_err_t zb_cmd_ias_zone_enroll_request_handler(const ezb_zcl_ias_zone_enroll_req_message_t *message) {
  if (!message) {
    log_e("Empty message");
    return ESP_FAIL;
  }
  if (message->info.status != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Received message: error status(%d)", message->info.status);
    return ESP_ERR_INVALID_ARG;
  }
  log_v(
    "IAS Zone Enroll Request: from address(0x%x) src endpoint(%u) to dst endpoint(%u) cluster(0x%x) zone type(0x%x) manufacturer code(0x%x)",
    message->in.header ? message->in.header->src_addr.u.short_addr : 0, message->in.header ? message->in.header->src_ep : 0, message->info.dst_ep,
    message->info.cluster_id, message->in.payload.zone_type, message->in.payload.manuf_code
  );

  for (std::list<ZigbeeEP *>::iterator it = Zigbee.ep_objects.begin(); it != Zigbee.ep_objects.end(); ++it) {
    if (message->info.dst_ep == (*it)->getEndpoint()) {
      (*it)->zbIASZoneEnrollRequest(message);
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

  // Movement command ID is in the ZCL header (in.header->cmd_id); payload is a lift/tilt union.
  log_v(
    "Received message: endpoint(%u), cluster(0x%x), command(0x%x), payload(%u)", message->info.dst_ep, message->info.cluster_id,
    message->in.header ? message->in.header->cmd_id : 0, message->in.payload.lift_value
  );

  // List through all Zigbee EPs and call the callback function, with the message
  for (std::list<ZigbeeEP *>::iterator it = Zigbee.ep_objects.begin(); it != Zigbee.ep_objects.end(); ++it) {
    if (message->info.dst_ep == (*it)->getEndpoint()) {
      (*it)->zbWindowCoveringMovementCmd(message);  //method zbWindowCoveringMovementCmd must be implemented in specific EP class
    }
  }
  return ESP_OK;
}

static esp_err_t zb_door_lock_cmd_handler(ezb_zcl_door_lock_lock_door_message_t *message, bool lock) {
  if (!message) {
    log_e("Empty message");
    return ESP_FAIL;
  }
  log_v("Received door %s command: endpoint(%u), cluster(0x%x)", lock ? "lock" : "unlock", message->info.dst_ep, message->info.cluster_id);

  // The command is answered with a failure unless an endpoint accepts it
  message->out.result = EZB_ZCL_STATUS_FAIL;
  for (std::list<ZigbeeEP *>::iterator it = Zigbee.ep_objects.begin(); it != Zigbee.ep_objects.end(); ++it) {
    if (message->info.dst_ep == (*it)->getEndpoint()) {
      (*it)->zbDoorLockCmd(message, lock);  //method zbDoorLockCmd must be implemented in specific EP class
    }
  }
  return ESP_OK;
}

// PIN code / user status commands: dispatched to the endpoint, which fills in the response status.
// Without an endpoint that handles the command, the status in the response stays "failure".
template<typename T> static void zb_door_lock_user_cmd_handler(T *message, void (ZigbeeEP::*handler)(T *)) {
  if (!message) {
    log_e("Empty message");
    return;
  }
  message->out.result = EZB_ZCL_STATUS_FAIL;
  for (std::list<ZigbeeEP *>::iterator it = Zigbee.ep_objects.begin(); it != Zigbee.ep_objects.end(); ++it) {
    if (message->info.dst_ep == (*it)->getEndpoint()) {
      ((*it)->*handler)(message);
    }
  }
}

// OTA client progress: start / receiving / check / apply / finish / abort.
// Parses the full Zigbee OTA file (header + sub-element) like esp-zigbee-sdk ota_client.
static esp_err_t zb_ota_upgrade_status_handler(ezb_zcl_ota_upgrade_client_progress_message_t *message) {
  [[maybe_unused]]
  static int64_t start_time = 0;
  esp_err_t ret = ESP_OK;

  switch (message->in.progress) {
    case EZB_ZCL_OTA_UPGRADE_PROGRESS_START:
      log_i(
        "Zigbee - OTA upgrade start (manuf 0x%04x type 0x%04x ver 0x%08" PRIx32 " size %" PRIu32 ")", message->in.start.manuf_code,
        message->in.start.image_type, message->in.start.file_version, message->in.start.image_size
      );
      start_time = esp_timer_get_time();
      s_ota_receive_failed = false;
      ret = zb_ota_prepare_receive(message->in.start.image_size);
      break;

    case EZB_ZCL_OTA_UPGRADE_PROGRESS_RECEIVING:
    {
      // Stack may still deliver a block after a prior ABORT out.result (next request was
      // queued before the app callback). Refuse without re-opening flash.
      if (s_ota_receive_failed) {
        ret = ESP_ERR_INVALID_STATE;
        break;
      }
      if (!s_ota_file_parser_active || s_ota_handle == 0) {
        if (s_ota_approved_image_size == 0) {
          log_e("Zigbee - OTA RECEIVING before START and no approved image size");
          ret = ESP_ERR_INVALID_STATE;
          break;
        }
        log_w("Zigbee - OTA RECEIVING before START; preparing with approved size %" PRIu32, s_ota_approved_image_size);
        ret = zb_ota_prepare_receive(s_ota_approved_image_size);
        if (ret != ESP_OK) {
          break;
        }
      }
      if (message->in.receiving.block == nullptr || message->in.receiving.block_size == 0) {
        log_e("Zigbee - OTA RECEIVING with empty block");
        ret = ESP_ERR_INVALID_ARG;
        break;
      }
      {
        uint32_t received = (uint32_t)(message->in.receiving.file_offset + message->in.receiving.block_size);
        // Avoid logging every 48-byte block (~15k lines) — Serial can stall the ZCL task.
        if (received == message->in.receiving.block_size || (received % 4096u) < message->in.receiving.block_size
            || received >= s_ota_file_parser.total_image_size) {
          log_i("Zigbee - OTA Client receives data: progress [%" PRIu32 "/%" PRIu32 "]", received, s_ota_file_parser.total_image_size);
        }
      }
      zb_ota_file_parser_setup(&s_ota_file_parser, message->in.receiving.block_size, message->in.receiving.block);
      do {
        esp_err_t parse_ret = zb_ota_file_parser_process(&s_ota_file_parser);
        if (parse_ret != ESP_OK && parse_ret != ESP_ERR_NOT_FINISHED) {
          ret = parse_ret;
          break;
        }
        if (zb_ota_file_parser_is_element_value(&s_ota_file_parser)) {
          if (s_ota_file_parser.element.type != ZB_OTA_ELEMENT_UPGRADE_IMAGE) {
            log_e("Zigbee - Unsupported OTA element type 0x%04x", s_ota_file_parser.element.type);
            ret = ESP_ERR_INVALID_ARG;
            break;
          }
          if (s_ota_file_parser.element.total > s_ota_partition->size + ZB_OTA_ELEMENT_HEADER_LEN) {
            log_e("Zigbee - OTA image exceeds partition size");
            ret = ESP_ERR_INVALID_SIZE;
            break;
          }
#if CONFIG_ZB_DELTA_OTA
          ret = esp_delta_ota_write(s_ota_handle, s_ota_file_parser.element.val, s_ota_file_parser.element.length);
#else
          ret = esp_ota_write(s_ota_handle, s_ota_file_parser.element.val, s_ota_file_parser.element.length);
#endif
          if (ret != ESP_OK) {
            log_e("Zigbee - Failed to write OTA data, status: %s", esp_err_to_name(ret));
            break;
          }
        }
        ret = parse_ret;
      } while (ret == ESP_ERR_NOT_FINISHED);
      if (ret == ESP_ERR_NOT_FINISHED) {
        ret = ESP_OK;
      }
      if (ret != ESP_OK) {
        zb_ota_fail_receive();
      }
      break;
    }

    case EZB_ZCL_OTA_UPGRADE_PROGRESS_CHECK:
      if (s_ota_receive_failed || !s_ota_file_parser_active) {
        ret = ESP_FAIL;
      } else {
        ret = zb_ota_file_parser_check(&s_ota_file_parser);
      }
      log_i("Zigbee - OTA upgrade check status: %s", esp_err_to_name(ret));
      if (ret != ESP_OK) {
        zb_ota_fail_receive();
      }
      break;

    case EZB_ZCL_OTA_UPGRADE_PROGRESS_APPLY:
      log_i("Zigbee - OTA upgrade apply");
      if (s_ota_receive_failed || s_ota_handle == 0 || s_ota_partition == NULL) {
        log_e("Zigbee - OTA apply with no active receive session");
        ret = ESP_ERR_INVALID_STATE;
        zb_ota_fail_receive();
        break;
      }
#if CONFIG_ZB_DELTA_OTA
      ret = esp_delta_ota_end(s_ota_handle);
#else
      ret = esp_ota_end(s_ota_handle);
#endif
      if (ret != ESP_OK) {
        log_e("Zigbee - Failed to end OTA partition, status: %s", esp_err_to_name(ret));
        zb_ota_fail_receive();
        break;
      }
      // esp_ota_end() closed the session; do not abort on later FINISH/reset.
      s_ota_handle = 0;
      ret = esp_ota_set_boot_partition(s_ota_partition);
      if (ret != ESP_OK) {
        log_e("Zigbee - Failed to set OTA boot partition, status: %s", esp_err_to_name(ret));
        zb_ota_fail_receive();
      }
      break;

    case EZB_ZCL_OTA_UPGRADE_PROGRESS_FINISH:
      log_i(
        "Zigbee - OTA Finish (total %" PRIu32 " bytes, %" PRId32 " ms)", s_ota_file_parser.total_offset, (int32_t)((esp_timer_get_time() - start_time) / 1000)
      );
      zb_ota_reset_receive_state();
      zb_ota_notify_complete(true);
      log_w("Zigbee - Prepare to restart system");
      esp_restart();
      break;

    case EZB_ZCL_OTA_UPGRADE_PROGRESS_ABORT:
      log_w("Zigbee - OTA upgrade aborted");
      zb_ota_reset_receive_state();
      zb_ota_notify_complete(false);
      break;

    default: log_i("Zigbee - OTA progress: %d", message->in.progress); break;
  }

  // ABORT makes the stack nest PROGRESS_ABORT (resets download status). CHECK/RECEIVING
  // returns are otherwise ignored for the already-queued next request / Upgrade End.
  message->out.result = (ret == ESP_OK) ? EZB_ZCL_STATUS_SUCCESS : EZB_ZCL_STATUS_ABORT;
  return ret;
}

static esp_err_t zb_ota_upgrade_query_image_resp_handler(ezb_zcl_ota_upgrade_query_next_image_rsp_message_t *message) {
  // ZCL_MESSAGE_SCHEDULE presets out.result to NO_PROCESS (= approve). Only write FAIL
  // when rejecting. Do not set SUCCESS explicitly and do not touch flash/attributes here —
  // the stack then runs PROGRESS_START and builds Image Block Request in the same path.
  if (message->in.image.status == EZB_ZCL_STATUS_SUCCESS) {
    const ezb_zcl_ota_upgrade_new_image_t *image = &message->in.image;
    log_i("Zigbee - Queried OTA image from endpoint: %u", image->ep_id);
    log_i("Zigbee - Image version: 0x%08" PRIx32 ", manufacturer code: 0x%04x, image size: %" PRIu32, image->file_version, image->manuf_code, image->size);
    if (image->size == 0) {
      log_w("Zigbee - Rejecting OTA image upgrade, image size is 0");
      message->out.result = EZB_ZCL_STATUS_FAIL;
      return ESP_FAIL;
    }
    if (image->file_version == 0) {
      log_w("Zigbee - Rejecting OTA image upgrade, file version is 0");
      message->out.result = EZB_ZCL_STATUS_FAIL;
      return ESP_FAIL;
    }
    s_ota_approved_image_size = image->size;
    // App accept only — stack still skips if version equals Current or DownloadedFileVersion.
    log_i("Zigbee - Accepting OTA image for download");
  } else {
    log_i("Zigbee - No OTA image available (status 0x%02x)", message->in.image.status);
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
  log_v(
    "Received default response: from address(0x%x), src_endpoint(%u) to dst_endpoint(%u), cluster(0x%x) with status 0x%x",
    message->in.header ? message->in.header->src_addr.u.short_addr : 0, message->in.header ? message->in.header->src_ep : 0, message->info.dst_ep,
    message->info.cluster_id, message->in.status_code
  );

  // Call global callback if set
  Zigbee.callDefaultResponseCallback(
    (zb_cmd_type_t)message->in.rsp_to_cmd, (ezb_zcl_status_t)message->in.status_code, message->info.dst_ep, message->info.cluster_id
  );

  // List through all Zigbee EPs and call the callback function, with the message
  for (std::list<ZigbeeEP *>::iterator it = Zigbee.ep_objects.begin(); it != Zigbee.ep_objects.end(); ++it) {
    if (message->info.dst_ep == (*it)->getEndpoint()) {
      (*it)->zbDefaultResponse(message);  //method zbDefaultResponse is implemented in the common EP class
    }
  }
  return ESP_OK;
}

static esp_err_t zb_cmd_identify_query_resp_handler(const ezb_zcl_identify_query_rsp_message_t *message) {
  if (!message) {
    log_e("Empty message");
    return ESP_FAIL;
  }
  if (message->info.status != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Received Identify Query Response: error status(%d)", message->info.status);
    return ESP_ERR_INVALID_ARG;
  }
  log_v(
    "Identify Query Response: from address(0x%x) src_endpoint(%u) to dst_endpoint(%u), timeout %u s",
    message->in.header ? message->in.header->src_addr.u.short_addr : 0, message->in.header ? message->in.header->src_ep : 0, message->info.dst_ep,
    message->in.timeout
  );

  for (std::list<ZigbeeEP *>::iterator it = Zigbee.ep_objects.begin(); it != Zigbee.ep_objects.end(); ++it) {
    if (message->info.dst_ep == (*it)->getEndpoint()) {
      (*it)->zbIdentifyQueryResponse(message);
    }
  }
  return ESP_OK;
}

// Dispatch manufacturer-specific commands to both EP hooks.
static esp_err_t zb_manuf_spec_command_handler(const ezb_zcl_manuf_spec_cmd_message_t *message) {
  if (!message) {
    log_e("Empty message");
    return ESP_FAIL;
  }
  if (message->info.status != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Received message: error status(%d)", message->info.status);
    return ESP_ERR_INVALID_ARG;
  }
  log_v(
    "Manufacturer-specific command: from address(0x%x) src endpoint(%u) to dst endpoint(%u) cluster(0x%x) command(0x%x)",
    message->in.header ? message->in.header->src_addr.u.short_addr : 0, message->in.header ? message->in.header->src_ep : 0, message->info.dst_ep,
    message->info.cluster_id, message->in.header ? message->in.header->cmd_id : 0
  );

  for (std::list<ZigbeeEP *>::iterator it = Zigbee.ep_objects.begin(); it != Zigbee.ep_objects.end(); ++it) {
    if (message->info.dst_ep == (*it)->getEndpoint()) {
      (*it)->zbCustomClusterCommand(message);
      (*it)->zbPrivilegeCommand(message);
    }
  }
  return ESP_OK;
}

void zigbee_register_zcl_handlers(void) {
  ezb_zcl_core_action_handler_register(zb_action_handler);
}

#endif  // CONFIG_ZB_ENABLED
