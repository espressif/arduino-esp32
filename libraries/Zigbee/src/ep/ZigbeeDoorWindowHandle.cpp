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

#include "ZigbeeDoorWindowHandle.h"
#if CONFIG_ZB_ENABLED

esp_zb_cluster_list_t *zigbee_door_window_handle_clusters_create(zigbee_door_window_handle_cfg_t *door_window_handle) {
  esp_zb_basic_cluster_cfg_t *basic_cfg = door_window_handle ? &(door_window_handle->basic_cfg) : NULL;
  esp_zb_identify_cluster_cfg_t *identify_cfg = door_window_handle ? &(door_window_handle->identify_cfg) : NULL;
  esp_zb_ias_zone_cluster_cfg_t *ias_zone_cfg = door_window_handle ? &(door_window_handle->ias_zone_cfg) : NULL;
  esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
  esp_zb_cluster_list_add_basic_cluster(cluster_list, esp_zb_basic_cluster_create(basic_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_identify_cluster(cluster_list, esp_zb_identify_cluster_create(identify_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_ias_zone_cluster(cluster_list, esp_zb_ias_zone_cluster_create(ias_zone_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  return cluster_list;
}

ZigbeeDoorWindowHandle::ZigbeeDoorWindowHandle(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = ESP_ZB_HA_IAS_ZONE_ID;
  _zone_status = 0;
  _zone_id = 0xff;
  _ias_cie_endpoint = 1;
  _enrolled = false;

  //Create custom door window handle configuration
  zigbee_door_window_handle_cfg_t door_window_handle_cfg = ZIGBEE_DEFAULT_DOOR_WINDOW_HANDLE_CONFIG();
  _cluster_list = zigbee_door_window_handle_clusters_create(&door_window_handle_cfg);

  _ep_config = {.endpoint = _endpoint, .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID, .app_device_id = ESP_ZB_HA_IAS_ZONE_ID, .app_device_version = 0};
}

void ZigbeeDoorWindowHandle::setIASClientEndpoint(uint8_t ep_number) {
  _ias_cie_endpoint = ep_number;
}

bool ZigbeeDoorWindowHandle::setClosed() {
  esp_zb_zcl_status_t ret = ESP_ZB_ZCL_STATUS_SUCCESS;
  log_v("Setting Door/Window handle to closed");
  uint8_t closed = 0;  // ALARM1 = 0, ALARM2 = 0
  esp_zb_lock_acquire(portMAX_DELAY);
  ret = esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_IAS_ZONE, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_IAS_ZONE_ZONESTATUS_ID, &closed, false
  );
  esp_zb_lock_release();
  if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set door/window handle to closed: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  _zone_status = closed;
  return report();
}

bool ZigbeeDoorWindowHandle::setOpen() {
  esp_zb_zcl_status_t ret = ESP_ZB_ZCL_STATUS_SUCCESS;
  log_v("Setting Door/Window handle to open");
  uint8_t open = ESP_ZB_ZCL_IAS_ZONE_ZONE_STATUS_ALARM1 | ESP_ZB_ZCL_IAS_ZONE_ZONE_STATUS_ALARM2;  // ALARM1 = 1, ALARM2 = 1
  esp_zb_lock_acquire(portMAX_DELAY);
  ret = esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_IAS_ZONE, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_IAS_ZONE_ZONESTATUS_ID, &open, false
  );
  esp_zb_lock_release();
  if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set door/window handle to open: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  _zone_status = open;
  return report();
}

bool ZigbeeDoorWindowHandle::setTilted() {
  esp_zb_zcl_status_t ret = ESP_ZB_ZCL_STATUS_SUCCESS;
  log_v("Setting Door/Window handle to tilted");
  uint8_t tilted = ESP_ZB_ZCL_IAS_ZONE_ZONE_STATUS_ALARM1;  // ALARM1 = 1, ALARM2 = 0
  esp_zb_lock_acquire(portMAX_DELAY);
  ret = esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_IAS_ZONE, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_IAS_ZONE_ZONESTATUS_ID, &tilted, false
  );
  esp_zb_lock_release();
  if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set door/window handle to tilted: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  _zone_status = tilted;
  return report();
}

bool ZigbeeDoorWindowHandle::report() {
  /* Send IAS Zone status changed notification command */

  esp_zb_zcl_ias_zone_status_change_notif_cmd_t status_change_notif_cmd;
  status_change_notif_cmd.address_mode = ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT;
  status_change_notif_cmd.zcl_basic_cmd.src_endpoint = _endpoint;
  status_change_notif_cmd.zcl_basic_cmd.dst_endpoint = _ias_cie_endpoint;  //default is 1
  memcpy(status_change_notif_cmd.zcl_basic_cmd.dst_addr_u.addr_long, _ias_cie_addr, sizeof(esp_zb_ieee_addr_t));

  status_change_notif_cmd.zone_status = _zone_status;
  status_change_notif_cmd.extend_status = 0;
  status_change_notif_cmd.zone_id = _zone_id;
  status_change_notif_cmd.delay = 0;

  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_ias_zone_status_change_notif_cmd_req(&status_change_notif_cmd);  //return transaction sequence number, ignore it
  esp_zb_lock_release();
  log_v("IAS Zone status changed notification sent");
  return true;
}

void ZigbeeDoorWindowHandle::zbIASZoneEnrollResponse(const esp_zb_zcl_ias_zone_enroll_response_message_t *message) {
  if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_IAS_ZONE) {
    log_v("IAS Zone Enroll Response: zone id(%d), status(%d)", message->zone_id, message->response_code);
    if (message->response_code == ESP_ZB_ZCL_IAS_ZONE_ENROLL_RESPONSE_CODE_SUCCESS) {
      log_v("IAS Zone Enroll Response: success");
      esp_zb_lock_acquire(portMAX_DELAY);
      memcpy(
        _ias_cie_addr,
        (*(esp_zb_ieee_addr_t *)
            esp_zb_zcl_get_attribute(_endpoint, ESP_ZB_ZCL_CLUSTER_ID_IAS_ZONE, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_IAS_ZONE_IAS_CIE_ADDRESS_ID)
              ->data_p),
        sizeof(esp_zb_ieee_addr_t)
      );
      esp_zb_lock_release();
      _zone_id = message->zone_id;
      _enrolled = true;
    }
  } else {
    log_w("Received message ignored. Cluster ID: %d not supported for On/Off Light", message->info.cluster);
  }
}

bool ZigbeeDoorWindowHandle::requestIASZoneEnroll() {
  esp_zb_zcl_ias_zone_enroll_request_cmd_t enroll_request;
  enroll_request.zcl_basic_cmd.src_endpoint = _endpoint;
  enroll_request.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
  enroll_request.zone_type = ESP_ZB_ZCL_IAS_ZONE_ZONETYPE_DOOR_WINDOW_HANDLE;
  enroll_request.manuf_code = 0;

  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_ias_zone_enroll_cmd_req(&enroll_request);  //return transaction sequence number, ignore it
  esp_zb_lock_release();
  log_v("IAS Zone enroll request sent");
  return true;
}

bool ZigbeeDoorWindowHandle::restoreIASZoneEnroll() {
  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_attr_t *ias_cie_attr =
    esp_zb_zcl_get_attribute(_endpoint, ESP_ZB_ZCL_CLUSTER_ID_IAS_ZONE, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_IAS_ZONE_IAS_CIE_ADDRESS_ID);
  esp_zb_zcl_attr_t *zone_id_attr =
    esp_zb_zcl_get_attribute(_endpoint, ESP_ZB_ZCL_CLUSTER_ID_IAS_ZONE, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_IAS_ZONE_ZONEID_ID);
  esp_zb_lock_release();

  if (ias_cie_attr == NULL || ias_cie_attr->data_p == NULL) {
    log_e("Failed to restore IAS Zone enroll: ias cie address attribute not found");
    return false;
  }
  if (zone_id_attr == NULL || zone_id_attr->data_p == NULL) {
    log_e("Failed to restore IAS Zone enroll: zone id attribute not found");
    return false;
  }

  memcpy(_ias_cie_addr, (esp_zb_ieee_addr_t *)ias_cie_attr->data_p, sizeof(esp_zb_ieee_addr_t));
  _zone_id = (*(uint8_t *)zone_id_attr->data_p);

  log_d(
    "Restored IAS Zone enroll: zone id(%d), ias cie address(%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X)", _zone_id, _ias_cie_addr[0], _ias_cie_addr[1],
    _ias_cie_addr[2], _ias_cie_addr[3], _ias_cie_addr[4], _ias_cie_addr[5], _ias_cie_addr[6], _ias_cie_addr[7]
  );

  if (_zone_id == 0xFF) {
    log_e("Failed to restore IAS Zone enroll: zone id not valid");
    return false;
  }
  _enrolled = true;
  return true;
}

#endif  // CONFIG_ZB_ENABLED
