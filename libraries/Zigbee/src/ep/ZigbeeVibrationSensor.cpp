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

#include "ZigbeeVibrationSensor.h"
#if CONFIG_ZB_ENABLED
#include "ezbee/zha.h"
#include "ezbee/zcl/cluster/ias_zone.h"


ZigbeeVibrationSensor::ZigbeeVibrationSensor(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = EZB_ZHA_IAS_ZONE_ID;
  _ias_zone_cfg = {
    .zone_state = EZB_ZCL_IAS_ZONE_ZONE_STATE_NOT_ENROLLED,
    .zone_type = EZB_ZCL_IAS_ZONE_ZONE_TYPE_VIBRATION_MOVEMENT_SENSOR,
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
    log_e("Failed to create vibration sensor endpoint descriptor");
    return;
  }
  ezb_af_endpoint_add_cluster_desc(_ep_desc, ezb_zcl_basic_create_cluster_desc(nullptr, EZB_ZCL_CLUSTER_SERVER));
  ezb_af_endpoint_add_cluster_desc(_ep_desc, ezb_zcl_identify_create_cluster_desc(nullptr, EZB_ZCL_CLUSTER_SERVER));
  ezb_af_endpoint_add_cluster_desc(_ep_desc, ezb_zcl_ias_zone_create_cluster_desc(&_ias_zone_cfg, EZB_ZCL_CLUSTER_SERVER));
}

void ZigbeeVibrationSensor::setIASClientEndpoint(uint8_t ep_number) {
  _ias_cie_endpoint = ep_number;
}

bool ZigbeeVibrationSensor::setVibration(bool sensed) {
  log_v("Setting Vibration sensor to %s", sensed ? "sensed" : "not sensed");
  uint16_t vibration = sensed ? EZB_ZCL_IAS_ZONE_ZONE_STATUS_ALARM1 : 0;
  ezb_zcl_status_t ret =
    setClusterAttribute(EZB_ZCL_CLUSTER_ID_IAS_ZONE, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_IAS_ZONE_ZONE_STATUS_ID, &vibration, false);
  if (ret != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set vibration status: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  _zone_status = vibration;
  return report();
}

bool ZigbeeVibrationSensor::report() {
  /* Send IAS Zone status changed notification command */
  if (!_enrolled) {
    log_e("Failed to report: IAS Zone not enrolled");
    return false;
  }
  // TODO(zb-v2): ESP-ZIGBEE-SDK v2.x removed the explicit IAS Zone Status Change Notification sender used in
  // v1 (esp_zb_zcl_ias_zone_status_change_notif_cmd_t + esp_zb_zcl_ias_zone_status_change_notif_cmd_req).
  // Per ezbee/zcl/cluster/ias_zone.h, the Zone Status Change Notification is now emitted automatically by
  // the stack whenever the ZoneStatus attribute (EZB_ZCL_ATTR_IAS_ZONE_ZONE_STATUS_ID) changes, which
  // setVibration() already does via setClusterAttribute(). A manual send to a specific CIE
  // address/endpoint/zone-id/delay (_ias_cie_addr, _ias_cie_endpoint, _zone_id) has no direct v2.x
  // equivalent and needs SDK guidance.
  log_v("IAS Zone status change notification is emitted automatically on ZoneStatus attribute change");
  return true;
}

void ZigbeeVibrationSensor::zbIASZoneEnrollResponse(const ezb_zcl_ias_zone_enroll_rsp_message_t *message) {
  if (message->info.cluster_id == EZB_ZCL_CLUSTER_ID_IAS_ZONE) {
    log_v("IAS Zone Enroll Response: zone id(%u), status(%u)", message->in.payload.zone_id, message->in.payload.enroll_rsp_code);
    if (message->in.payload.enroll_rsp_code == EZB_ZCL_IAS_ZONE_ENROLL_RESPONSE_CODE_SUCCESS) {
      log_v("IAS Zone Enroll Response: success");
      // v2.x: read the IAS CIE address attribute through the opaque attribute descriptor (replaces the v1
      // esp_zb_zcl_get_attribute()->data_p access). No lock here: this runs in the stack callback context.
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

bool ZigbeeVibrationSensor::requestIASZoneEnroll() {
  ezb_zcl_ias_zone_enroll_req_cmd_t enroll_request;
  memset(&enroll_request, 0, sizeof(enroll_request));
  // No explicit destination: send to bound devices (replaces v1 ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT).
  ezb_address_set_none(&enroll_request.cmd_ctrl.dst_addr);
  enroll_request.cmd_ctrl.src_ep = _endpoint;
  enroll_request.payload.zone_type = EZB_ZCL_IAS_ZONE_ZONE_TYPE_VIBRATION_MOVEMENT_SENSOR;
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

bool ZigbeeVibrationSensor::restoreIASZoneEnroll() {
  if (!getClusterAttribute(
      EZB_ZCL_CLUSTER_ID_IAS_ZONE, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_IAS_ZONE_IAS_CIE_ADDRESS_ID, _ias_cie_addr, sizeof(_ias_cie_addr)
    )) {
    log_e("Failed to restore IAS Zone enroll: ias cie address attribute not found");
    return false;
  }
  if (!getClusterAttribute(EZB_ZCL_CLUSTER_ID_IAS_ZONE, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_IAS_ZONE_ZONE_ID_ID, &_zone_id, sizeof(_zone_id))) {
    log_e("Failed to restore IAS Zone enroll: zone id attribute not found");
    return false;
  }

  log_d(
    "Restored IAS Zone enroll: zone id(%u), ias cie address(%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X)", _zone_id, _ias_cie_addr[0], _ias_cie_addr[1],
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
