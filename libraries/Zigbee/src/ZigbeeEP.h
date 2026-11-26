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

/* Common Class for Zigbee End point */

#pragma once

#include "ZigbeeCore.h"
#if CONFIG_ZB_ENABLED

#include <Arduino.h>
#include <ColorFormat.h>
#include "esp_timer.h"

/* Useful defines */
#define ZB_CMD_TIMEOUT               10000      // 10 seconds
#define OTA_UPGRADE_QUERY_INTERVAL_S (60 * 60)  // Automatic re-query period in seconds (1 hour)

#define ZB_ARRAY_LENGHT(arr) (sizeof(arr) / sizeof(arr[0]))

#define RGB_TO_XYZ(r, g, b, X, Y, Z)                               \
  {                                                                \
    X = (float)(0.412453 * (r) + 0.357580 * (g) + 0.180423 * (b)); \
    Y = (float)(0.212671 * (r) + 0.715160 * (g) + 0.072169 * (b)); \
    Z = (float)(0.019334 * (r) + 0.119193 * (g) + 0.950227 * (b)); \
  }

/**
 * @brief Packed Zigbee string (length byte followed by character data).
 */
typedef struct zbstring_s {
  uint8_t len;
  char data[];
} __attribute__((packed)) zbstring_t;

/**
 * @brief Bound-device identity used by endpoint binding lists.
 */
typedef struct zb_device_params_s {
  uint8_t ieee_addr[8]; /**< EUI-64 IEEE address. */
  uint8_t endpoint;     /**< Remote endpoint number. */
  uint16_t short_addr;  /**< Network short address. */
} zb_device_params_t;

/**
 * @brief Power source reported by the Basic cluster.
 */
typedef enum {
  ZB_POWER_SOURCE_UNKNOWN = 0x00, /**< Power source is unknown. */
  ZB_POWER_SOURCE_MAINS = 0x01,   /**< Mains-powered. */
  ZB_POWER_SOURCE_BATTERY = 0x03, /**< Battery-powered. */
} zb_power_source_t;

/**
 * @brief Convert a ZCL status code to a readable name.
 * @param status ZCL status value.
 * @return Status name string.
 */
const char *esp_zb_zcl_status_to_name(ezb_zcl_status_t status);

/**
 * @brief Base class for all Zigbee endpoints.
 *
 * Provides manufacturer/model information, binding, power reporting, time
 * synchronization, OTA client support, and device discovery. Derived endpoint
 * classes add device-specific clusters and APIs.
 *
 * Lifecycle: `Zigbee.role()`, configure the endpoint, `Zigbee.addEndpoint()`,
 * then `Zigbee.begin()`.
 */
class ZigbeeEP {
public:
  // constants and limits
  static constexpr size_t ZB_MAX_NAME_LENGTH = 32;

  /**
   * @brief Construct a Zigbee endpoint.
   * @param endpoint Endpoint number (1-254). Default: 10.
   */
  ZigbeeEP(uint8_t endpoint = 10);
  ~ZigbeeEP() {}

  /**
   * @brief Check whether the endpoint descriptor has been created.
   * @return true if the derived class has created `_ep_desc`.
   */
  bool isEpReady() const {
    return _ep_desc != nullptr;
  }

  /**
   * @brief Set the application version for the endpoint.
   *
   * Stored in the endpoint config and added to the Basic cluster Application Version
   * attribute. Must be called before `Zigbee.begin()`.
   *
   * @param version Application version (0-255).
   */
  void setVersion(uint8_t version);
  /**
   * @brief Set the hardware version for the endpoint.
   *
   * Added to the Basic cluster Hardware Version attribute. Must be called before
   * `Zigbee.begin()`.
   *
   * @param version Hardware version (0-255).
   */
  void setHardwareVersion(uint8_t version);

  /**
   * @brief Get the endpoint number assigned to this device.
   * @return Endpoint number (1-254).
   */
  uint8_t getEndpoint() {
    return _endpoint;
  }

  /**
   * @brief Print bound devices to Serial.
   */
  void printBoundDevices();
  /**
   * @brief Print bound devices to a Print object.
   * @param print Destination (defaults to Serial in the Serial overload).
   */
  void printBoundDevices(Print &print);

  /**
   * @brief Get the list of devices bound to this endpoint.
   * @return List of bound device parameters.
   */
  std::list<zb_device_params_t *> getBoundDevices() const {
    return _bound_devices;
  }

  /**
   * @brief Check whether the endpoint has any bound devices.
   * @return true if the endpoint has bound devices.
   */
  bool bound() {
    return _is_bound;
  }
  /**
   * @brief Allow or disallow binding multiple devices to this endpoint.
   * @param bind true to allow multiple bindings, false for a single binding only.
   */
  void allowMultipleBinding(bool bind) {
    _allow_multiple_binding = bind;
  }
  /**
   * @brief Enable or disable manual binding mode.
   *
   * Manual binding is intended for ZHA or Zigbee2MQTT, where devices are bound
   * from the hub rather than automatically.
   *
   * @param bind true for manual binding, false for automatic binding (default).
   */
  void setManualBinding(bool bind) {
    _use_manual_binding = bind;
  }

  /**
   * @brief Set the manufacturer name and model identifier.
   * @param name Manufacturer name (max 32 characters).
   * @param model Model identifier (max 32 characters).
   * @return true if set successfully.
   */
  bool setManufacturerAndModel(const char *name, const char *model);

  /**
   * @brief Set the Basic cluster DeviceEnabled attribute (0x0012).
   *
   * When false, the stack ignores ZCL commands on this endpoint (the device stays
   * on the network). Call once before `Zigbee.addEndpoint()` to add the attribute
   * to the endpoint descriptor. After that, set/get work before and after
   * `Zigbee.begin()`. If the attribute was never added, a call after `begin()`
   * returns false.
   *
   * @param enabled true to accept ZCL commands (default), false to ignore them.
   * @return true if set successfully.
   */
  bool setDeviceEnabled(bool enabled);
  /**
   * @brief Get the current DeviceEnabled value.
   * @return true if the endpoint accepts ZCL commands, or true (ZCL default) if
   *         the attribute is not present.
   */
  bool getDeviceEnabled();

  /**
   * @brief Read the manufacturer name from a remote device.
   * @param endpoint Target endpoint number.
   * @param short_addr Target device short address.
   * @param ieee_addr Target device IEEE address.
   * @return Pointer to the manufacturer string, or `NULL` if the read failed.
   */
  char *readManufacturer(uint8_t endpoint, uint16_t short_addr, const uint8_t *ieee_addr);
  /**
   * @brief Read the model identifier from a remote device.
   * @param endpoint Target endpoint number.
   * @param short_addr Target device short address.
   * @param ieee_addr Target device IEEE address.
   * @return Pointer to the model string, or `NULL` if the read failed.
   */
  char *readModel(uint8_t endpoint, uint16_t short_addr, const uint8_t *ieee_addr);

  /**
   * @brief Set the power source type and optional battery status.
   * @param power_source Power source (`ZB_POWER_SOURCE_MAINS`, `ZB_POWER_SOURCE_BATTERY`, etc.).
   * @param percentage Battery charge percentage (0-100). Only used for a battery
   *                   power source. Values above 100, including the default 0xff,
   *                   are clamped to 100 and then stored as percentage * 2 (ZCL 0-200).
   * @param voltage Battery voltage in 100 mV units (e.g. 35 for 3.5 V). Only used for
   *                a battery power source. The default 0xff is stored as given.
   * @return true if set successfully.
   */
  bool setPowerSource(zb_power_source_t power_source, uint8_t percentage = 0xff, uint8_t voltage = 0xff);
  /**
   * @brief Set the current battery percentage.
   * @param percentage Battery percentage (0-100).
   * @return true if set successfully.
   */
  bool setBatteryPercentage(uint8_t percentage);
  /**
   * @brief Set the battery voltage.
   * @param voltage Battery voltage in 100 mV units (e.g. 35 for 3.5 V).
   * @return true if set successfully.
   */
  bool setBatteryVoltage(uint8_t voltage);
  /**
   * @brief Report the current battery percentage to the network.
   * @return true if reported successfully.
   */
  bool reportBatteryPercentage();

  /**
   * @brief Persist a local attribute in the Zigbee NVS dataset.
   *
   * Call after the attribute exists (typically after `role()` plus cluster setup,
   * before or after `begin()`). The next local write is stored and restored on reboot.
   *
   * @param cluster_id Cluster containing the attribute.
   * @param attr_id Attribute to persist.
   * @param persistent true to persist, false to stop persisting.
   * @param cluster_role Cluster role (default: server).
   * @return true if configured successfully.
   */
  bool setAttributePersistent(uint16_t cluster_id, uint16_t attr_id, bool persistent = true, uint8_t cluster_role = EZB_ZCL_CLUSTER_SERVER);
  /**
   * @brief Read a cluster attribute into a caller-provided buffer.
   * @param cluster_id Cluster containing the attribute.
   * @param attr_id Attribute to read.
   * @param value Output buffer.
   * @param value_size Size of the output buffer in bytes.
   * @param cluster_role Cluster role (default: server).
   * @return true if the attribute was read successfully.
   */
  bool getAttribute(uint16_t cluster_id, uint16_t attr_id, void *value, uint16_t value_size, uint8_t cluster_role = EZB_ZCL_CLUSTER_SERVER);

  /**
   * @brief Enable or disable Default Response on automatic attribute reports.
   *
   * Call after `setReporting()` (reporting info must already exist; typically after
   * `Zigbee.begin()`). Leave enabled when the sketch waits for DefaultRsp to confirm
   * delivery. Set `enabled` to false only for fire-and-forget reports.
   *
   * @param cluster_id Cluster of the reportable attribute.
   * @param attr_id Reportable attribute to configure.
   * @param enabled true to request DefaultRsp on reports, false to disable it.
   * @param cluster_role Cluster role (default: server).
   * @return true if set successfully.
   */
  bool setReportingDefaultResponse(uint16_t cluster_id, uint16_t attr_id, bool enabled, uint8_t cluster_role = EZB_ZCL_CLUSTER_SERVER);

  /**
   * @brief Add a time synchronization cluster to the endpoint.
   *
   * For a server cluster (this device has the time and GMT offset), fill `time` and
   * `gmt_offset`. For a client cluster (read time from the network), keep the defaults.
   *
   * @param time Current time structure (default: empty).
   * @param gmt_offset GMT offset in seconds (default: 0).
   * @return true if added successfully.
   */
  bool addTimeCluster(tm time = {}, int32_t gmt_offset = 0);
  /**
   * @brief Activate the Time cluster server so the Time attribute can be served or written.
   *
   * Must be called after `Zigbee.begin()` (the per-endpoint time context exists only
   * once the stack runs).
   *
   * @return true if registered successfully.
   */
  bool registerTimeServer();
  /**
   * @brief Set the current time for the endpoint.
   * @param time Time structure to set.
   * @return true if set successfully.
   */
  bool setTime(tm time);
  /**
   * @brief Set the timezone offset for the endpoint.
   * @param gmt_offset GMT offset in seconds.
   * @return true if set successfully.
   */
  bool setTimezone(int32_t gmt_offset);

  /**
   * @brief Get the current network time (blocking until a response).
   * @param endpoint Target endpoint (default: 1).
   * @param short_addr Target device short address (default: 0x0000).
   * @param ieee_addr Target device IEEE address (default: `nullptr`).
   * @return Network time structure.
   */
  struct tm getTime(uint8_t endpoint = 1, int32_t short_addr = 0x0000, const uint8_t *ieee_addr = nullptr);
  /**
   * @brief Get the timezone offset (blocking until a response).
   * @param endpoint Target endpoint (default: 1).
   * @param short_addr Target device short address (default: 0x0000).
   * @param ieee_addr Target device IEEE address (default: `nullptr`).
   * @return GMT offset in seconds.
   */
  int32_t getTimezone(uint8_t endpoint = 1, int32_t short_addr = 0x0000, const uint8_t *ieee_addr = nullptr);

  /**
   * @brief Check whether multiple device binding is allowed for this endpoint.
   * @return true if multiple bindings are allowed.
   */
  bool epAllowMultipleBinding() {
    return _allow_multiple_binding;
  }
  /**
   * @brief Check whether manual binding mode is enabled for this endpoint.
   * @return true if manual binding is enabled.
   */
  bool epUseManualBinding() {
    return _use_manual_binding;
  }

  /**
   * @brief Add an OTA client to the endpoint for firmware updates.
   *
   * Call before `Zigbee.addEndpoint()`. After `Zigbee.begin()`, `requestOTAUpdate()`
   * applies `hw_version` and `max_data_size` via the stack APIs. Receives a full
   * Zigbee OTA image (not Delta OTA). Use a dual-OTA partition scheme
   * (for example `Zigbee 4MB with spiffs`).
   *
   * @param file_version Current firmware version (`CurrentFileVersion`).
   * @param downloaded_file_ver `DownloadedFileVersion` (`0xFFFFFFFF` = none stored).
   *        Must not equal the image version offered by the OTA server or the stack
   *        will skip the download after Query approve.
   * @param hw_version Hardware version (passed in Query Next Image when non-zero).
   * @param manufacturer Manufacturer code (default: 0x1001).
   * @param image_type Image type code (default: 0x1011).
   * @param max_data_size Maximum Image Block size (default: 48, the stack default;
   *                      use 223 with Espressif `ota_server`).
   * @return true if the OTA client was added successfully.
   */
  bool addOTAClient(
    uint32_t file_version, uint32_t downloaded_file_ver, uint16_t hw_version, uint16_t manufacturer = 0x1001, uint16_t image_type = 0x1011,
    uint8_t max_data_size = 48
  );
  /**
   * @brief Discover an OTA Upgrade server and send Query Next Image.
   *
   * Call after the device is connected. Further queries are sent every hour
   * automatically. A coordinator (for example Home Assistant ZHA) can also start
   * an upgrade with Image Notify without this call.
   */
  void requestOTAUpdate();

  /**
   * @brief Register a privilege command to intercept standard cluster commands.
   *
   * Intercepts the command before the stack processes it.
   *
   * @param cluster_id Cluster of the command.
   * @param command_id Command identifier to intercept.
   */
  void addPrivilegeCommand(uint16_t cluster_id, uint16_t command_id);

  /**
   * @brief Send an IAS Zone Enroll Response (CIE/coordinator side).
   *
   * Typically called from a `zbIASZoneEnrollRequest()` override using the request
   * message source address and endpoint.
   *
   * @param dst_short_addr Destination short address.
   * @param dst_endpoint Destination endpoint.
   * @param zone_id Zone identifier to assign.
   * @param response_code Enroll response code (default: success).
   * @return true if the response was sent.
   */
  bool sendIASZoneEnrollResponse(
    uint16_t dst_short_addr, uint8_t dst_endpoint, uint8_t zone_id,
    ezb_zcl_ias_zone_enroll_rsp_code_t response_code = EZB_ZCL_IAS_ZONE_ENROLL_RESPONSE_CODE_SUCCESS
  );

  /**
   * @brief Find and bind matching devices. Override in derived endpoints.
   * @param cmd_req Match descriptor request filled by the stack.
   */
  virtual void findEndpoint(ezb_zdo_match_desc_req_t *cmd_req) {};

  /**
   * @brief Handle a ZCL attribute write. Override in derived endpoints.
   * @param message Attribute set message from the stack.
   */
  virtual void zbAttributeSet(const ezb_zcl_set_attr_value_message_t *message) {};
  /**
   * @brief Handle a ZCL attribute read response. Override in derived endpoints.
   * @param cluster_id Cluster of the attribute.
   * @param attribute Attribute value.
   * @param src_endpoint Source endpoint.
   * @param src_address Source address.
   */
  virtual void zbAttributeRead(uint16_t cluster_id, const ezb_zcl_attribute_t *attribute, uint8_t src_endpoint, ezb_address_t src_address) {};
  /**
   * @brief Handle a write-attribute response. Override in derived endpoints.
   * @param cluster_id Cluster of the attribute.
   * @param attribute_id Attribute identifier.
   * @param status ZCL status of the write.
   * @param src_endpoint Source endpoint.
   * @param src_address Source address.
   */
  virtual void zbWriteAttributeResponse(uint16_t cluster_id, uint16_t attribute_id, ezb_zcl_status_t status, uint8_t src_endpoint, ezb_address_t src_address) {
  };
  /**
   * @brief Handle a Basic cluster attribute read (implemented in the base class).
   * @param attribute Attribute value.
   */
  virtual void zbReadBasicCluster(const ezb_zcl_attribute_t *attribute);
  /**
   * @brief Handle an Identify cluster command.
   * @param message Identify attribute-set message.
   */
  virtual void zbIdentify(const ezb_zcl_set_attr_value_message_t *message);
  /**
   * @brief Handle an Identify Query Response.
   * @param message Query response from a device that is identifying.
   */
  virtual void zbIdentifyQueryResponse(const ezb_zcl_identify_query_rsp_message_t *message);
  /**
   * @brief Handle OTA transfer state changes.
   * @param otaActive true while download/apply is active.
   */
  virtual void zbOTAState(bool otaActive);
  /**
   * @brief Handle OTA transfer completion.
   * @param success true on successful FINISH (before reboot), false on abort/failure.
   */
  virtual void zbOTAComplete(bool success);
  /**
   * @brief Handle a Window Covering movement command. Override in derived endpoints.
   * @param message Movement command from the stack.
   */
  virtual void zbWindowCoveringMovementCmd(const ezb_zcl_window_covering_movement_message_t *message) {};
  /**
   * @brief Handle a Door Lock / Unlock command. Override in derived endpoints.
   * @param message Lock command message.
   * @param lock true for Lock Door, false for Unlock Door.
   */
  virtual void zbDoorLockCmd(ezb_zcl_door_lock_lock_door_message_t *message, bool lock) {};
  /**
   * @brief Handle Set PIN Code. Override in derived endpoints.
   * @param message Set PIN Code command.
   */
  virtual void zbDoorLockSetPinCode(ezb_zcl_door_lock_set_pin_code_message_t *message) {};
  /**
   * @brief Handle Get PIN Code. Override in derived endpoints.
   * @param message Get PIN Code command.
   */
  virtual void zbDoorLockGetPinCode(ezb_zcl_door_lock_get_pin_code_message_t *message) {};
  /**
   * @brief Handle Clear PIN Code. Override in derived endpoints.
   * @param message Clear PIN Code command.
   */
  virtual void zbDoorLockClearPinCode(ezb_zcl_door_lock_clear_pin_code_message_t *message) {};
  /**
   * @brief Handle Clear All PIN Codes. Override in derived endpoints.
   * @param message Clear All PIN Codes command.
   */
  virtual void zbDoorLockClearAllPinCodes(ezb_zcl_door_lock_clear_all_pin_codes_message_t *message) {};
  /**
   * @brief Handle Set User Status. Override in derived endpoints.
   * @param message Set User Status command.
   */
  virtual void zbDoorLockSetUserStatus(ezb_zcl_door_lock_set_user_status_message_t *message) {};
  /**
   * @brief Handle Get User Status. Override in derived endpoints.
   * @param message Get User Status command.
   */
  virtual void zbDoorLockGetUserStatus(ezb_zcl_door_lock_get_user_status_message_t *message) {};
  /**
   * @brief Handle a Time cluster attribute read (implemented in the base class).
   * @param attribute Attribute value.
   */
  virtual void zbReadTimeCluster(const ezb_zcl_attribute_t *attribute);
  /**
   * @brief Handle an IAS Zone status-change notification. Override in derived endpoints.
   * @param message Status-change notification.
   */
  virtual void zbIASZoneStatusChangeNotification(const ezb_zcl_ias_zone_status_change_notif_message_t *message) {};
  /**
   * @brief Handle an IAS Zone enroll request. Override in derived endpoints.
   * @param message Enroll request.
   */
  virtual void zbIASZoneEnrollRequest(const ezb_zcl_ias_zone_enroll_req_message_t *message) {};
  /**
   * @brief Handle an IAS Zone enroll response. Override in derived endpoints.
   * @param message Enroll response.
   */
  virtual void zbIASZoneEnrollResponse(const ezb_zcl_ias_zone_enroll_rsp_message_t *message) {};
  /**
   * @brief Handle a default response (implemented in the base class).
   * @param message Default response message.
   */
  virtual void zbDefaultResponse(const ezb_zcl_cmd_default_rsp_message_t *message);
  /**
   * @brief Handle a manufacturer-specific privilege command.
   * @param message Manufacturer-specific command message.
   */
  virtual void zbPrivilegeCommand(const ezb_zcl_manuf_spec_cmd_message_t *message);
  /**
   * @brief Handle a manufacturer-specific custom cluster command.
   * @param message Manufacturer-specific command message.
   */
  virtual void zbCustomClusterCommand(const ezb_zcl_manuf_spec_cmd_message_t *message);

  /**
   * @brief Add a device to this endpoint's bound-device list.
   * @param device Device parameters to store (ownership remains with the caller).
   */
  virtual void addBoundDevice(zb_device_params_t *device) {
    _bound_devices.push_back(device);
    _is_bound = true;
  }

  /**
   * @brief Remove a bound device by endpoint and IEEE address.
   * @param endpoint Remote endpoint number.
   * @param ieee_addr Remote IEEE address.
   */
  virtual void removeBoundDevice(uint8_t endpoint, const uint8_t *ieee_addr);
  /**
   * @brief Remove a bound device by pointer.
   * @param device Device parameters previously added with `addBoundDevice()`.
   */
  virtual void removeBoundDevice(zb_device_params_t *device);

  /**
   * @brief Remove all bound devices from this endpoint.
   */
  virtual void clearBoundDevices() {
    _bound_devices.clear();
    _is_bound = false;
  }

  /**
   * @brief Set a callback for identify events.
   * @param callback Function called with identify time in seconds.
   */
  void onIdentify(void (*callback)(uint16_t)) {
    _on_identify = callback;
  }

  /**
   * @brief Set a callback for Identify Query Response messages.
   * @param callback Function called with remaining identify timeout (seconds),
   *                 source endpoint, and source address.
   */
  void onIdentifyQueryResponse(void (*callback)(uint16_t timeout, uint8_t src_endpoint, ezb_address_t src_address)) {
    _on_identify_query_response = callback;
  }

  /**
   * @brief Send an Identify Query command.
   *
   * Devices that are currently identifying reply with Identify Query Response
   * (see `onIdentifyQueryResponse`). Useful for finding-and-binding discovery.
   *
   * @param short_addr Destination short address (default `0xFFFF` = broadcast).
   * @param dst_endpoint Destination endpoint (default broadcast endpoint).
   * @return true if the command was sent.
   */
  bool identifyQuery(uint16_t short_addr = 0xFFFF, uint8_t dst_endpoint = EZB_AF_ENDPOINT_BROADCAST_ID);

  /**
   * @brief Set a callback for OTA transfer lifecycle.
   * @param callback Called with true when download/apply becomes active, false when inactive.
   */
  void onOTAStateChange(void (*callback)(bool active)) {
    _on_ota_state_change = callback;
  }

  /**
   * @brief Set a callback for OTA transfer completion.
   * @param callback Called with true on successful FINISH (before reboot), false on abort/failure.
   */
  void onOTAComplete(void (*callback)(bool success)) {
    _on_ota_complete = callback;
  }

  /**
   * @brief Set a callback for default response messages on this endpoint.
   * @param callback Function called with the command being answered and the ZCL status.
   */
  void onDefaultResponse(void (*callback)(zb_cmd_type_t resp_to_cmd, ezb_zcl_status_t status)) {
    _on_default_response = callback;
  }

  /**
   * @brief Set a callback for intercepted privilege commands.
   * @param callback Function called with the manufacturer-specific command message.
   */
  void onPrivilegeCommand(void (*callback)(const ezb_zcl_manuf_spec_cmd_message_t *message)) {
    _on_privilege_command = callback;
  }

  /**
   * @brief Set a callback for custom cluster commands.
   * @param callback Function called with the manufacturer-specific command message.
   */
  void onCustomClusterCommand(void (*callback)(const ezb_zcl_manuf_spec_cmd_message_t *message)) {
    _on_custom_cluster_command = callback;
  }

private:
  char *_read_manufacturer;
  char *_read_model;
  void (*_on_identify)(uint16_t time);
  void (*_on_identify_query_response)(uint16_t timeout, uint8_t src_endpoint, ezb_address_t src_address);
  void (*_on_ota_state_change)(bool active);
  void (*_on_ota_complete)(bool success);
  void (*_on_default_response)(zb_cmd_type_t resp_to_cmd, ezb_zcl_status_t status);
  void (*_on_privilege_command)(const ezb_zcl_manuf_spec_cmd_message_t *message);
  void (*_on_custom_cluster_command)(const ezb_zcl_manuf_spec_cmd_message_t *message);
  time_t _read_time;
  int32_t _read_timezone;

protected:
  uint8_t _endpoint;
  uint16_t _device_id;
  ezb_af_ep_config_t _ep_config;
  ezb_af_ep_desc_t _ep_desc;
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
  // Start reporting from an opaque handle (ezb_zcl_reporting_info_find / ezb_zcl_reporting_info_update).
  bool setClusterReporting(ezb_zcl_reporting_info_t reporting_info);
  bool configureClusterReporting(ezb_zcl_config_report_cmd_t *report_cmd);

  // Acquire/release pair for outgoing ZCL/ZDO commands (caller invokes SDK between them).
  bool acquireCommandLock();
  void releaseCommandLock();

  bool requireBeforeAddEndpoint(const char *api) const;

  // Descriptor-time helpers (before Zigbee.addEndpoint()): read/update attributes on _ep_desc.
  using ep_cluster_desc_add_attr_fn = ezb_err_t (*)(ezb_zcl_cluster_desc_t cluster_desc, uint16_t attr_id, const void *value);

  ezb_zcl_cluster_desc_t getEpClusterDesc(uint16_t cluster_id, uint8_t cluster_role = EZB_ZCL_CLUSTER_SERVER) const;
  ezb_zcl_attr_desc_t getEpClusterAttrDesc(uint16_t cluster_id, uint8_t cluster_role, uint16_t attr_id) const;
  bool setEpClusterAttrValue(uint16_t cluster_id, uint8_t cluster_role, uint16_t attr_id, const void *value) const;
  bool addOrSetEpClusterAttr(uint16_t cluster_id, uint8_t cluster_role, uint16_t attr_id, const void *value, ep_cluster_desc_add_attr_fn add_attr) const;
  bool configureEpClusterAttr(const char *api, uint16_t cluster_id, uint8_t cluster_role, uint16_t attr_id, void *value, ep_cluster_desc_add_attr_fn add_attr);

  char _zb_manufacturer[ZB_MAX_NAME_LENGTH + 2];
  char _zb_model[ZB_MAX_NAME_LENGTH + 2];
  uint8_t _app_version;
  uint8_t _hw_version;
  bool _device_enabled;

  ezb_zcl_power_config_cluster_server_config_t _power_config_cfg;
  uint8_t _battery_percentage;  // BatteryPercentageRemaining (0-200), not in power_config cluster config
  uint8_t _battery_voltage;     // BatteryVoltage in 100 mV, not in power_config cluster config

  ezb_zcl_time_cluster_server_config_t _time_server_cfg;
  int32_t _time_gmt_offset;              // TimeZone, not in time server cluster config
  bool _time_server = false;             // addTimeCluster() was called (a Time server cluster exists)
  bool _time_server_registered = false;  // registerTimeServer() succeeded (interface active)

  ezb_zcl_ota_upgrade_cluster_client_config_t _ota_client_cfg;
  uint32_t _ota_current_file_version;     // CurrentFileVersion, not in OTA client cluster config
  uint32_t _ota_downloaded_file_version;  // DownloadedFileVersion, not in OTA client cluster config
  uint16_t _ota_hw_version = 0;           // Applied via ezb_zcl_ota_upgrade_set_hw_version after begin
  uint8_t _ota_max_data_size = 48;        // Applied via ezb_zcl_ota_upgrade_set_download_block_size after begin (stack default)
  bool _ota_client_added = false;
  bool _ota_runtime_configured = false;
  bool _ota_in_progress = false;  // true while download/apply is active (skip periodic queries)
  uint16_t _ota_server_addr = 0xffff;
  uint8_t _ota_server_ep = 0xff;
  esp_timer_handle_t _ota_query_timer = nullptr;

  bool applyOTAClientRuntimeConfig();
  void queryOTAServer(uint16_t short_addr, uint8_t endpoint);
  void startOTAQueryTimer();
  static void findOTAServerCb(const ezb_zdo_match_desc_req_result_t *result, void *user_ctx);
  static void otaQueryTimerCb(void *arg);

  // Friend class declaration to allow access to protected members
  friend class ZigbeeCore;
};

#endif  // CONFIG_ZB_ENABLED
