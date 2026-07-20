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

/* Common Class for Zigbee End point */

#pragma once

#include "ZigbeeCore.h"
#if CONFIG_ZB_ENABLED

#include <Arduino.h>
#include <ColorFormat.h>

/* Useful defines */
#define ZB_CMD_TIMEOUT             10000     // 10 seconds
#define OTA_UPGRADE_QUERY_INTERVAL (1 * 60)  // 1 hour = 60 minutes

#define ZB_ARRAY_LENGHT(arr) (sizeof(arr) / sizeof(arr[0]))

#define RGB_TO_XYZ(r, g, b, X, Y, Z)                               \
  {                                                                \
    X = (float)(0.412453 * (r) + 0.357580 * (g) + 0.180423 * (b)); \
    Y = (float)(0.212671 * (r) + 0.715160 * (g) + 0.072169 * (b)); \
    Z = (float)(0.019334 * (r) + 0.119193 * (g) + 0.950227 * (b)); \
  }

// NOTE(zb-v2): v2.x exposes packing through EZB_PACKED_* (see core_types.h) rather than a single
// ESP_ZB_PACKED_STRUCT macro, so the ZCL string helper uses the portable packed attribute directly.
typedef struct zbstring_s {
  uint8_t len;
  char data[];
} __attribute__((packed)) zbstring_t;

typedef struct zb_device_params_s {
  uint8_t ieee_addr[8];  // was esp_zb_ieee_addr_t; kept as a raw EUI-64 byte array (ezb_extaddr_t is a union)
  uint8_t endpoint;
  uint16_t short_addr;
} zb_device_params_t;

typedef enum {
  ZB_POWER_SOURCE_UNKNOWN = 0x00,
  ZB_POWER_SOURCE_MAINS = 0x01,
  ZB_POWER_SOURCE_BATTERY = 0x03,
} zb_power_source_t;

// Global function for converting ZCL status to name.
// NOTE(zb-v2): name kept (used across the library); parameter is now the v2.x ezb_zcl_status_t (uint8_t).
const char *esp_zb_zcl_status_to_name(ezb_zcl_status_t status);

/* Zigbee End Device Class */
class ZigbeeEP {
public:
  // constants and limits
  static constexpr size_t ZB_MAX_NAME_LENGTH = 32;

  // constructors and destructor
  ZigbeeEP(uint8_t endpoint = 10);
  ~ZigbeeEP() {}

  // Set ep config and endpoint descriptor.
  // NOTE(zb-v2): The v1 ZCL "cluster list" (esp_zb_cluster_list_t) no longer exists. In v2.x a derived
  // class creates an endpoint descriptor with ezb_af_create_endpoint_desc(&ep_config), attaches each
  // cluster descriptor with ezb_af_endpoint_add_cluster_desc(), and passes the result here. ZigbeeCore
  // then attaches _ep_desc to the device descriptor via ezb_af_device_add_endpoint_desc().
  // v2.x lifecycle: Zigbee.role(role), configure EPs, Zigbee.addEndpoint(), Zigbee.begin().
  bool isEpReady() const {
    return _ep_desc != nullptr;
  }


  // Set application version and hardware version
  void setVersion(uint8_t version);
  void setHardwareVersion(uint8_t version);

  uint8_t getEndpoint() {
    return _endpoint;
  }

  void printBoundDevices();
  void printBoundDevices(Print &print);

  std::list<zb_device_params_t *> getBoundDevices() const {
    return _bound_devices;
  }

  bool bound() {
    return _is_bound;
  }
  void allowMultipleBinding(bool bind) {
    _allow_multiple_binding = bind;
  }
  void setManualBinding(bool bind) {
    _use_manual_binding = bind;
  }

  // Set Manufacturer name and model
  bool setManufacturerAndModel(const char *name, const char *model);

  // Methods to read manufacturer and model name from selected endpoint and short address.
  // NOTE(zb-v2): IEEE address parameters are plain const uint8_t[8] EUI-64 pointers (the v1
  // esp_zb_ieee_addr_t typedef is no longer used here).
  char *readManufacturer(uint8_t endpoint, uint16_t short_addr, const uint8_t *ieee_addr);
  char *readModel(uint8_t endpoint, uint16_t short_addr, const uint8_t *ieee_addr);

  // Set Power source and battery percentage for battery powered devices
  bool setPowerSource(zb_power_source_t power_source, uint8_t percentage = 0xff, uint8_t voltage = 0xff);  // voltage in 100mV
  bool setBatteryPercentage(uint8_t percentage);                                                           // 0-100 %
  bool setBatteryVoltage(uint8_t voltage);                                                                 // voltage in 100mV (example value 35 for 3.5V)
  bool reportBatteryPercentage();                                                                          // battery voltage is not reportable attribute

  // Set time
  bool addTimeCluster(tm time = {}, int32_t gmt_offset = 0);  // gmt offset in seconds
  // Activate the Time cluster server so the Time attribute (0x0000) can be served/written.
  // MUST be called after Zigbee.begin() (the per-endpoint time context exists only once the stack runs).
  bool registerTimeServer();
  bool setTime(tm time);
  bool setTimezone(int32_t gmt_offset);

  // Get time from Coordinator or specific endpoint (blocking until response)
  struct tm getTime(uint8_t endpoint = 1, int32_t short_addr = 0x0000, const uint8_t *ieee_addr = nullptr);
  int32_t getTimezone(uint8_t endpoint = 1, int32_t short_addr = 0x0000, const uint8_t *ieee_addr = nullptr);  // gmt offset in seconds

  bool epAllowMultipleBinding() {
    return _allow_multiple_binding;
  }
  bool epUseManualBinding() {
    return _use_manual_binding;
  }

  // OTA methods
  /**
   * @brief Add OTA client to the Zigbee endpoint.
   *
   * @param file_version The current file version of the OTA client.
   * @param downloaded_file_ver The version of the downloaded file.
   * @param hw_version The hardware version of the device.
   * @param manufacturer The manufacturer code (default: 0x1001).
   * @param image_type The image type code (default: 0x1011).
   * @param max_data_size The maximum data size for OTA transfer (default and recommended: 223).
   * @return true if the OTA client was added successfully, false otherwise.
   */
  bool addOTAClient(
    uint32_t file_version, uint32_t downloaded_file_ver, uint16_t hw_version, uint16_t manufacturer = 0x1001, uint16_t image_type = 0x1011,
    uint8_t max_data_size = 223
  );
  /**
   * @brief Request OTA update from the server, first request is within a minute and the next requests are sent every hour automatically.
  */
  void requestOTAUpdate();

  // Register a privilege command to intercept standard cluster commands before the stack processes them
  void addPrivilegeCommand(uint16_t cluster_id, uint16_t command_id);

  // Send an IAS Zone Enroll Response (CIE/coordinator side) to a device that issued an enroll request.
  // Typically called from within a zbIASZoneEnrollRequest() override using the request message src address/endpoint.
  bool sendIASZoneEnrollResponse(
    uint16_t dst_short_addr, uint8_t dst_endpoint, uint8_t zone_id,
    esp_zb_zcl_ias_zone_enroll_response_code_t response_code = ESP_ZB_ZCL_IAS_ZONE_ENROLL_RESPONSE_CODE_SUCCESS
  );

  // findEndpoint may be implemented by EPs to find and bind devices
  virtual void findEndpoint(ezb_zdo_match_desc_req_t *cmd_req) {};

  // list of all handlers function calls, to be override by EPs implementation
  virtual void zbAttributeSet(const ezb_zcl_set_attr_value_message_t *message) {};
  virtual void zbAttributeRead(uint16_t cluster_id, const ezb_zcl_attribute_t *attribute, uint8_t src_endpoint, ezb_address_t src_address) {};
  virtual void
    zbWriteAttributeResponse(uint16_t cluster_id, uint16_t attribute_id, ezb_zcl_status_t status, uint8_t src_endpoint, ezb_address_t src_address) {};
  virtual void zbReadBasicCluster(const ezb_zcl_attribute_t *attribute);  //already implemented
  virtual void zbIdentify(const ezb_zcl_set_attr_value_message_t *message);
  virtual void zbOTAState(bool otaActive);
  virtual void zbWindowCoveringMovementCmd(const ezb_zcl_window_covering_movement_message_t *message) {};
  virtual void zbReadTimeCluster(const ezb_zcl_attribute_t *attribute);  //already implemented
  virtual void zbIASZoneStatusChangeNotification(const ezb_zcl_ias_zone_status_change_notif_message_t *message) {};
  virtual void zbIASZoneEnrollRequest(const esp_zb_zcl_ias_zone_enroll_request_message_t *message) {};
  virtual void zbIASZoneEnrollResponse(const ezb_zcl_ias_zone_enroll_rsp_message_t *message) {};
  virtual void zbDefaultResponse(const ezb_zcl_cmd_default_rsp_message_t *message);  //already implemented
  // NOTE(zb-v2): v1 "privilege command" and "custom cluster command" interception have no direct v2.x
  // equivalent. The closest mechanism is the manufacturer-specific command callback
  // (EZB_ZCL_CORE_MANUF_SPEC_CMD_CB_ID -> ezb_zcl_manuf_spec_cmd_message_t) and/or the raw frame handler.
  // These two hooks are mapped to the manuf-spec message and need a redesign in ZigbeeHandlers.
  virtual void zbPrivilegeCommand(const ezb_zcl_manuf_spec_cmd_message_t *message);
  virtual void zbCustomClusterCommand(const ezb_zcl_manuf_spec_cmd_message_t *message);

  virtual void addBoundDevice(zb_device_params_t *device) {
    _bound_devices.push_back(device);
    _is_bound = true;
  }

  virtual void removeBoundDevice(uint8_t endpoint, const uint8_t *ieee_addr);
  virtual void removeBoundDevice(zb_device_params_t *device);

  virtual void clearBoundDevices() {
    _bound_devices.clear();
    _is_bound = false;
  }

  void onIdentify(void (*callback)(uint16_t)) {
    _on_identify = callback;
  }

  void onOTAStateChange(void (*callback)(bool state)) {
    _on_ota_state_change = callback;
  }

  void onDefaultResponse(void (*callback)(zb_cmd_type_t resp_to_cmd, ezb_zcl_status_t status)) {
    _on_default_response = callback;
  }

  void onPrivilegeCommand(void (*callback)(const ezb_zcl_manuf_spec_cmd_message_t *message)) {
    _on_privilege_command = callback;
  }

  void onCustomClusterCommand(void (*callback)(const ezb_zcl_manuf_spec_cmd_message_t *message)) {
    _on_custom_cluster_command = callback;
  }

  // Convert ZCL status to name

private:
  char *_read_manufacturer;
  char *_read_model;
  void (*_on_identify)(uint16_t time);
  void (*_on_ota_state_change)(bool state);
  void (*_on_default_response)(zb_cmd_type_t resp_to_cmd, ezb_zcl_status_t status);
  void (*_on_privilege_command)(const ezb_zcl_manuf_spec_cmd_message_t *message);
  void (*_on_custom_cluster_command)(const ezb_zcl_manuf_spec_cmd_message_t *message);
  time_t _read_time;
  int32_t _read_timezone;

protected:
  uint8_t _endpoint;
  // NOTE(zb-v2): HA standard device IDs move to ezbee/zha.h (e.g. ezb_zha_standard_devices_t). Kept as a
  // plain uint16_t app_device_id here to avoid coupling to the (not-yet-migrated) ZHA device-id enum.
  uint16_t _device_id;
  ezb_af_ep_config_t _ep_config;   // was esp_zb_endpoint_config_t
  ezb_af_ep_desc_t _ep_desc;       // was esp_zb_cluster_list_t *_cluster_list (opaque endpoint descriptor)
  bool _is_bound;
  bool _allow_multiple_binding;
  bool _use_manual_binding;
  std::list<zb_device_params_t *> _bound_devices;
  SemaphoreHandle_t lock;
  zb_power_source_t _power_source;
  uint8_t _time_status;

  // Thread-safe outgoing ZCL helpers (not related to stack callbacks).
  ezb_zcl_status_t setClusterAttribute(uint16_t cluster_id, uint8_t cluster_role, uint16_t attr_id, void *value, bool check = false);
  bool getClusterAttribute(uint16_t cluster_id, uint8_t cluster_role, uint16_t attr_id, void *value, uint16_t value_size);
  bool reportClusterAttribute(ezb_zcl_report_attr_cmd_t *report_attr_cmd);
  bool readClusterAttribute(ezb_zcl_read_attr_cmd_t *read_req);
  // NOTE(zb-v2): v2.x reporting is handle-based (ezb_zcl_reporting_info_t obtained via
  // ezb_zcl_reporting_info_find(), then ezb_zcl_reporting_info_update()); there is no caller-populated
  // reporting-info struct anymore. This helper now takes the opaque reporting handle.
  bool setClusterReporting(ezb_zcl_reporting_info_t reporting_info);
  bool configureClusterReporting(ezb_zcl_config_report_cmd_t *report_cmd);

  // Acquire/release pair for outgoing ZCL/ZDO commands (caller invokes SDK between them).
  bool acquireCommandLock();
  void releaseCommandLock();

  bool requireBeforeAddEndpoint(const char *api) const;

  // Descriptor-time helpers (before Zigbee.addEndpoint()): read/update attributes on _ep_desc.
  typedef ezb_err_t (*ep_cluster_desc_add_attr_fn)(ezb_zcl_cluster_desc_t cluster_desc, uint16_t attr_id, const void *value);

  ezb_zcl_cluster_desc_t getEpClusterDesc(uint16_t cluster_id, uint8_t cluster_role = EZB_ZCL_CLUSTER_SERVER) const;
  ezb_zcl_attr_desc_t getEpClusterAttrDesc(uint16_t cluster_id, uint8_t cluster_role, uint16_t attr_id) const;
  bool setEpClusterAttrValue(uint16_t cluster_id, uint8_t cluster_role, uint16_t attr_id, const void *value) const;
  bool addOrSetEpClusterAttr(
    uint16_t cluster_id, uint8_t cluster_role, uint16_t attr_id, const void *value, ep_cluster_desc_add_attr_fn add_attr
  ) const;
  bool configureEpClusterAttr(
    const char *api, uint16_t cluster_id, uint8_t cluster_role, uint16_t attr_id, void *value, ep_cluster_desc_add_attr_fn add_attr
  );

  char _zb_manufacturer[ZB_MAX_NAME_LENGTH + 2];
  char _zb_model[ZB_MAX_NAME_LENGTH + 2];
  uint8_t _app_version;
  uint8_t _hw_version;

  ezb_zcl_power_config_cluster_server_config_t _power_config_cfg;
  uint8_t _battery_percentage;  // BatteryPercentageRemaining (0-200), not in power_config cluster config
  uint8_t _battery_voltage;     // BatteryVoltage in 100 mV, not in power_config cluster config

  ezb_zcl_time_cluster_server_config_t _time_server_cfg;
  int32_t _time_gmt_offset;       // TimeZone, not in time server cluster config
  bool _time_server = false;      // addTimeCluster() was called (a Time server cluster exists)
  bool _time_server_registered = false;  // registerTimeServer() succeeded (interface active)

  ezb_zcl_ota_upgrade_cluster_client_config_t _ota_client_cfg;
  uint32_t _ota_current_file_version;     // CurrentFileVersion, not in OTA client cluster config
  uint32_t _ota_downloaded_file_version;  // DownloadedFileVersion, not in OTA client cluster config

  // Friend class declaration to allow access to protected members
  friend class ZigbeeCore;
};

#endif  // CONFIG_ZB_ENABLED
