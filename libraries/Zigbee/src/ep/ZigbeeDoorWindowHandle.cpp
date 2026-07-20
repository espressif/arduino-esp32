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
#include "ezbee/zha.h"
#include "ezbee/zcl/cluster/ias_zone.h"


ZigbeeDoorWindowHandle::ZigbeeDoorWindowHandle(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = EZB_ZHA_IAS_ZONE_ID;
  _ias_zone_cfg = {
    .zone_state = EZB_ZCL_IAS_ZONE_ZONE_STATE_NOT_ENROLLED,
    .zone_type = EZB_ZCL_IAS_ZONE_ZONE_TYPE_DOOR_WINDOW_HANDLE,
    .zone_status = EZB_ZCL_IAS_ZONE_ZONE_STATUS_DEFAULT_VALUE,
    .ias_cie_address = 0,
    .zone_id = EZB_ZCL_IAS_ZONE_ZONE_ID_DEFAULT_VALUE,
  };
  _zone_status = 0;
  _zone_id = 0xff;
  _ias_cie_endpoint = 1;
  _enrolled = false;
  memset(_ias_cie_addr, 0, sizeof(_ias_cie_addr));

  _ep_config = {.ep_id = _endpoint, .app_profile_id = EZB_AF_HA_PROFILE_ID, .app_device_id = EZB_ZHA_IAS_ZONE_ID, .app_device_version = 0};
  _ep_desc = ezb_af_create_endpoint_desc(&_ep_config);
  if (_ep_desc == nullptr) {
    log_e("Failed to create door/window handle endpoint descriptor");
    return;
  }
  ezb_af_endpoint_add_cluster_desc(_ep_desc, ezb_zcl_basic_create_cluster_desc(nullptr, EZB_ZCL_CLUSTER_SERVER));
  ezb_af_endpoint_add_cluster_desc(_ep_desc, ezb_zcl_identify_create_cluster_desc(nullptr, EZB_ZCL_CLUSTER_SERVER));
  ezb_af_endpoint_add_cluster_desc(_ep_desc, ezb_zcl_ias_zone_create_cluster_desc(&_ias_zone_cfg, EZB_ZCL_CLUSTER_SERVER));
}

void ZigbeeDoorWindowHandle::setIASClientEndpoint(uint8_t ep_number) {
  _ias_cie_endpoint = ep_number;
}

bool ZigbeeDoorWindowHandle::setClosed() {
  log_v("Setting Door/Window handle to closed");
  // Keep RESTORE_NOTIFY set so the server emits the restore notification when alarm bits clear.
  uint16_t closed = EZB_ZCL_IAS_ZONE_ZONE_STATUS_RESTORE_NOTIFY;  // ALARM1 = 0, ALARM2 = 0
  ezb_zcl_status_t ret =
    setClusterAttribute(EZB_ZCL_CLUSTER_ID_IAS_ZONE, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_IAS_ZONE_ZONE_STATUS_ID, &closed, false);
  if (ret != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set door/window handle to closed: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  _zone_status = closed;
  return report();
}

bool ZigbeeDoorWindowHandle::setOpen() {
  log_v("Setting Door/Window handle to open");
  uint16_t open =
    EZB_ZCL_IAS_ZONE_ZONE_STATUS_ALARM1 | EZB_ZCL_IAS_ZONE_ZONE_STATUS_ALARM2 | EZB_ZCL_IAS_ZONE_ZONE_STATUS_RESTORE_NOTIFY;
  ezb_zcl_status_t ret =
    setClusterAttribute(EZB_ZCL_CLUSTER_ID_IAS_ZONE, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_IAS_ZONE_ZONE_STATUS_ID, &open, false);
  if (ret != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set door/window handle to open: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  _zone_status = open;
  return report();
}

bool ZigbeeDoorWindowHandle::setTilted() {
  log_v("Setting Door/Window handle to tilted");
  uint16_t tilted = EZB_ZCL_IAS_ZONE_ZONE_STATUS_ALARM1 | EZB_ZCL_IAS_ZONE_ZONE_STATUS_RESTORE_NOTIFY;  // ALARM1 = 1, ALARM2 = 0
  ezb_zcl_status_t ret =
    setClusterAttribute(EZB_ZCL_CLUSTER_ID_IAS_ZONE, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_IAS_ZONE_ZONE_STATUS_ID, &tilted, false);
  if (ret != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set door/window handle to tilted: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  _zone_status = tilted;
  return report();
}

bool ZigbeeDoorWindowHandle::report() {
  /* Send IAS Zone status changed notification command */
  if (!_enrolled) {
    log_e("Failed to report: IAS Zone not enrolled");
    return false;
  }
  // v2.x has no public notification sender; the server emits it automatically on ZoneStatus change.
  log_v("IAS Zone status change notification is emitted automatically on ZoneStatus attribute change");
  return true;
}

void ZigbeeDoorWindowHandle::zbIASZoneEnrollResponse(const ezb_zcl_ias_zone_enroll_rsp_message_t *message) {
  if (message->info.cluster_id == EZB_ZCL_CLUSTER_ID_IAS_ZONE) {
    log_v("IAS Zone Enroll Response: zone id(%u), status(%u)", message->in.payload.zone_id, message->in.payload.enroll_rsp_code);
    if (message->in.payload.enroll_rsp_code == EZB_ZCL_IAS_ZONE_ENROLL_RESPONSE_CODE_SUCCESS) {
      log_v("IAS Zone Enroll Response: success");
      ezb_zcl_attr_desc_t ias_cie_attr =
        ezb_zcl_get_attr_desc(_endpoint, EZB_ZCL_CLUSTER_ID_IAS_ZONE, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_IAS_ZONE_IAS_CIE_ADDRESS_ID, EZB_ZCL_STD_MANUF_CODE);
      if (ias_cie_attr == nullptr || ezb_zcl_attr_desc_get_value(ias_cie_attr, _ias_cie_addr) != EZB_ERR_NONE) {
        return;
      }
      _zone_id = message->in.payload.zone_id;
      _enrolled = true;
    }
  } else {
    log_w("Received message ignored. Cluster ID: %u not supported for IAS Zone", message->info.cluster_id);
  }
}

bool ZigbeeDoorWindowHandle::requestIASZoneEnroll() {
  ezb_zcl_ias_zone_enroll_req_cmd_t enroll_request;
  memset(&enroll_request, 0, sizeof(enroll_request));
  // No explicit destination: send to bound devices.
  ezb_address_set_none(&enroll_request.cmd_ctrl.dst_addr);
  enroll_request.cmd_ctrl.src_ep = _endpoint;
  enroll_request.payload.zone_type = EZB_ZCL_IAS_ZONE_ZONE_TYPE_DOOR_WINDOW_HANDLE;
  enroll_request.payload.manuf_code = 0;

  if (!acquireCommandLock()) {
    return false;
  }
  ezb_err_t ret = ezb_zcl_ias_zone_enroll_cmd_req(&enroll_request);
  releaseCommandLock();
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to send IAS Zone enroll request: 0x%x", ret);
    return false;
  }
  log_v("IAS Zone enroll request sent");
  return true;
}

bool ZigbeeDoorWindowHandle::restoreIASZoneEnroll() {
  // NOTE: Workaround, not a proper fix. v2.x does not persist the IAS Zone attributes across reboot
  // (unlike SDK in v1.x), so we just re-assert ZoneState = ENROLLED to resume notifications to the
  // persisted CIE binding. Revert to reading back the persisted attributes if the SDK matches old behaviour.
  uint8_t zone_state = EZB_ZCL_IAS_ZONE_ZONE_STATE_ENROLLED;
  ezb_zcl_status_t ret =
    setClusterAttribute(EZB_ZCL_CLUSTER_ID_IAS_ZONE, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_IAS_ZONE_ZONE_STATE_ID, &zone_state, false);
  if (ret != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to restore IAS Zone enroll: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  _enrolled = true;
  log_d("Restored IAS Zone enrollment (ZoneState set to ENROLLED)");
  return true;
}

#endif  // CONFIG_ZB_ENABLED
