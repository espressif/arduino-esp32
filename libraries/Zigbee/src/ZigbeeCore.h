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

/* Zigbee core class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "esp_zigbee.h"
#include "ezbee/zha.h"                   // ZHA standard device IDs (EZB_ZHA_*_DEVICE_ID); not pulled in by esp_zigbee.h
#include "ezbee/zdo/zdo_nwk_mgmt.h"      // Mgmt_Bind_req (binding table retrieval)
#include "ezbee/zdo/zdo_dev_srv_disc.h"  // Match_Desc_req (endpoint discovery)
#include <esp32-hal-log.h>
#include <list>
#include "ZigbeeTypes.h"
#include "ZigbeeEP.h"
class ZigbeeEP;

/** @brief Zero-argument callback. */
using voidFuncPtr = void (*)();
/** @brief Callback with a user context pointer. */
using voidFuncPtrArg = void (*)(void *);

/** @brief Active-scan result type returned by ZigbeeCore::getScanResult(). */
typedef ezb_nwk_active_scan_result_t zigbee_scan_result_t;

/**
 * @brief Zigbee device role (values match ezb_nwk_device_type_t).
 */
typedef enum {
  ZIGBEE_COORDINATOR = 0, /**< Network coordinator; forms and manages the network. */
  ZIGBEE_ROUTER = 1,      /**< Router; extends range and routes messages (use for mains-powered devices). */
  ZIGBEE_END_DEVICE = 2   /**< End device; typically battery-powered and can sleep. */
} zigbee_role_t;

#define ZB_SCAN_RUNNING (-1)
#define ZB_SCAN_FAILED  (-2)

#define ZB_BEGIN_TIMEOUT_DEFAULT 30000  // 30 seconds

// All 2.4 GHz channels (11-26)
#define ZB_TRANSCEIVER_ALL_CHANNELS_MASK 0x07FFF800

#define ZIGBEE_DEFAULT_ED_CONFIG()                                               \
  {                                                                              \
    .device_type = EZB_NWK_DEVICE_TYPE_END_DEVICE, .install_code_policy = false, \
    .zed_config = {                                                              \
      .ed_timeout = EZB_NWK_ED_TIMEOUT_64MIN,                                    \
      .keep_alive = 3000,                                                        \
    },                                                                           \
  }

#define ZIGBEE_DEFAULT_ROUTER_CONFIG() \
  { .device_type = EZB_NWK_DEVICE_TYPE_ROUTER, .install_code_policy = false, .zczr_config = {.max_children = 10}, }

#define ZIGBEE_DEFAULT_COORDINATOR_CONFIG() \
  { .device_type = EZB_NWK_DEVICE_TYPE_COORDINATOR, .install_code_policy = false, .zczr_config = {.max_children = 10}, }

#define ZIGBEE_DEFAULT_UART_RCP_RADIO_CONFIG()    \
  {                                               \
    .radio_mode = ESP_ZIGBEE_RADIO_MODE_UART_RCP, \
    .radio_uart_config = {                        \
      .port = UART_NUM_1,                         \
      .uart_config =                              \
        {                                         \
          .baud_rate = 460800,                    \
          .data_bits = UART_DATA_8_BITS,          \
          .parity = UART_PARITY_DISABLE,          \
          .stop_bits = UART_STOP_BITS_1,          \
          .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,  \
          .rx_flow_ctrl_thresh = 0,               \
          .source_clk = UART_SCLK_DEFAULT,        \
        },                                        \
      .rx_pin = GPIO_NUM_NC,                      \
      .tx_pin = GPIO_NUM_NC,                      \
    },                                            \
  }

/**
 * @brief Main entry point for Zigbee network operations.
 *
 * Manages the device role, endpoints, network discovery, and stack lifecycle.
 * Use the global `Zigbee` object. It is omitted when `NO_GLOBAL_INSTANCES` or
 * `NO_GLOBAL_ZIGBEE` is defined.
 *
 * Typical lifecycle: `role()` (must be first), configure endpoints, `addEndpoint()`,
 * then `begin()`.
 */
class ZigbeeCore {
private:
  esp_zigbee_radio_config_t _radio_config;
  uint32_t _primary_channel_mask;
  uint32_t _begin_timeout;
  int16_t _scan_status;
  uint8_t _scan_duration;
  bool _rx_on_when_idle;

  ezb_af_device_desc_t _zb_dev_desc;
  zigbee_role_t _role;
  bool _initialized;
  bool _endpoints_registered;
  bool _stack_running;
  bool _started;
  bool _connected;
  bool _paused;

  uint8_t _open_network;
  zigbee_scan_result_t *_scan_result;
  uint16_t _scan_count;  // number of results accumulated during a streaming scan
  SemaphoreHandle_t lock;
  bool _debug;
  bool _allow_multi_endpoint_binding;

  // Global default response callback
  void (*_global_default_response_cb)(zb_cmd_type_t resp_to_cmd, ezb_zcl_status_t status, uint8_t endpoint, uint16_t cluster);
  // Parent lost callback (end device / sleepy ED keepalive failure)
  void (*_on_parent_lost)();

  bool zigbeeStackInit(esp_zigbee_device_config_t *zb_cfg, bool erase_nvs);
  bool zigbeeStartStack();
  bool registerEndpoints();
  static void scanCompleteCallback(ezb_nwk_active_scan_result_t *result, void *user_ctx);
  const char *getDeviceTypeString(uint16_t deviceId);  // ZHA device id (see EZB_ZHA_*_DEVICE_ID in ezbee/zha.h)
  void searchBindings();
  static void bindingTableCb(const ezb_zdo_nwk_mgmt_bind_req_result_t *result, void *user_ctx);
  void resetNVRAMChannelMask();             // Reset to default mask (persisted via NVS)
  void setNVRAMChannelMask(uint32_t mask);  // Set channel mask (persisted via NVS)

public:
  /**
   * @brief Construct the Zigbee core object.
   *
   * Sketches should use the global `Zigbee` instance.
   */
  ZigbeeCore();
  ~ZigbeeCore() {}

  /**
   * @brief Registered endpoint list.
   */
  std::list<ZigbeeEP *> ep_objects;

  /**
   * @brief Set the device role and initialize the stack.
   *
   * Must be the first Zigbee call, before configuring endpoints and calling `begin()`.
   *
   * @param role Device role (`ZIGBEE_COORDINATOR`, `ZIGBEE_ROUTER`, or `ZIGBEE_END_DEVICE`).
   * @param erase_nvs When true, erase only the Zigbee NVS namespace (`zigbee`) on the
   *                  shared `nvs` partition before init. Does not wipe Wi-Fi credentials
   *                  or Preferences. Default: false.
   * @return true if the role was set and the stack initialized successfully.
   *
   * @note Zigbee network state is stored in the default `nvs` partition (namespace
   *       `zigbee`), shared with Wi-Fi and Preferences. Zigbee partition schemes no
   *       longer use a separate `zb_storage` partition. Upgrading from Arduino-ESP32 3.x
   *       (or early 4.0 builds that used `zb_storage`) requires erasing flash or calling
   *       `factoryReset()`; network state is not migrated in place.
   * @note Set Zigbee mode and partition scheme in the Arduino IDE to match the role:
   *       coordinator/router use Zigbee ZCZR mode and a ZCZR partition scheme; end
   *       device uses Zigbee ED mode and a Zigbee xMB partition scheme.
   */
  bool role(zigbee_role_t role, bool erase_nvs = false);
  /**
   * @brief Set a custom device configuration and initialize the stack.
   *
   * Use this overload for a sleepy end device or a custom `max_children` value.
   * Must be the first Zigbee call, before configuring endpoints and calling `begin()`.
   *
   * @param role_cfg Custom role configuration (e.g. sleepy ED or custom `max_children`).
   * @param erase_nvs When true, erase only the Zigbee NVS namespace (`zigbee`) before init.
   * @return true if the role was set and the stack initialized successfully.
   *
   * @note Same NVS and partition-scheme notes as `role(zigbee_role_t)`.
   */
  bool role(esp_zigbee_device_config_t *role_cfg, bool erase_nvs = false);
  /**
   * @brief Attach all added endpoints, register the device descriptor, and start the stack.
   *
   * Call after `role()` and after all endpoints have been added with `addEndpoint()`.
   *
   * @return true if the stack started successfully.
   *
   * @note Set Zigbee mode and partition scheme in the Arduino IDE to match the role
   *       (ZCZR for coordinator/router, ED for end device).
   */
  bool begin();
  /**
   * @brief Stop the Zigbee stack.
   *
   * Use after `begin()` to save power or free the radio for other tasks.
   */
  void stop();
  /**
   * @brief Start the Zigbee stack again after `stop()`.
   */
  void start();

  /**
   * @brief Check whether `role()` has initialized the stack.
   * @return true if the stack has been initialized.
   */
  bool initialized() {
    return _initialized;
  }
  /**
   * @brief Check whether endpoints have been registered with the stack.
   * @return true if endpoints are registered.
   */
  bool endpointsRegistered() {
    return _endpoints_registered;
  }
  /**
   * @brief Check whether the Zigbee stack task is running.
   * @return true if the stack task is running.
   */
  bool stackRunning() {
    return _stack_running;
  }
  /**
   * @brief Check whether the Zigbee stack has been started and is not paused.
   * @return true if the stack is running.
   */
  bool started() {
    return _started && !_paused;
  }
  /**
   * @brief Check whether the stack is paused via `stop()`.
   * @return true if the stack is paused.
   */
  bool paused() {
    return _paused;
  }
  /**
   * @brief Check whether the device is connected to a Zigbee network.
   * @return true if connected to a network.
   */
  bool connected() {
    return _connected;
  }

  /**
   * @brief Get the current Zigbee device role.
   * @return Current role (`ZIGBEE_COORDINATOR`, `ZIGBEE_ROUTER`, or `ZIGBEE_END_DEVICE`).
   */
  zigbee_role_t getRole() {
    return _role;
  }

  /**
   * @brief Add an endpoint to the Zigbee network.
   * @param ep Pointer to the endpoint object to add.
   * @return true if the endpoint was added successfully.
   */
  bool addEndpoint(ZigbeeEP *ep);
  //void removeEndpoint(ZigbeeEP *ep);

  /**
   * @brief Set the radio configuration.
   *
   * Must be called before `role()`.
   *
   * @param config Radio configuration structure.
   */
  void setRadioConfig(esp_zigbee_radio_config_t config);
  /**
   * @brief Get the current radio configuration.
   * @return Current radio configuration.
   */
  esp_zigbee_radio_config_t getRadioConfig();

  /**
   * @brief Set the primary channel mask for network scanning and joining.
   *
   * Must be called before `role()`. By default all channels 11-26 are scanned
   * (mask `0x07FFF800`).
   *
   * @param mask Channel mask.
   */
  void setPrimaryChannelMask(uint32_t mask);

  /**
   * @brief Set the scan duration for network discovery.
   * @param duration Scan duration (1-4, where 1 is fastest and 4 is slowest).
   */
  void setScanDuration(uint8_t duration);
  /**
   * @brief Get the current scan duration setting.
   * @return Current scan duration (1-4).
   */
  uint8_t getScanDuration() {
    return _scan_duration;
  }

  /**
   * @brief Set whether the device keeps its receiver on when idle.
   * @param rx_on_when_idle true to keep the receiver on, false to allow sleep.
   */
  void setRxOnWhenIdle(bool rx_on_when_idle) {
    _rx_on_when_idle = rx_on_when_idle;
  }
  /**
   * @brief Get the current receiver idle setting.
   * @return true if the receiver stays on when idle.
   */
  bool getRxOnWhenIdle() {
    return _rx_on_when_idle;
  }
  /**
   * @brief Set the timeout for network operations.
   * @param timeout Timeout in milliseconds (default: 30000 ms).
   */
  void setTimeout(uint32_t timeout) {
    _begin_timeout = timeout;
  }
  /**
   * @brief Open the network for joining after reboot for a specified time.
   *
   * Applied only when the role is coordinator, after the network is formed.
   * On other roles the stored time is ignored.
   *
   * @param time Time in seconds to keep the network open after reboot.
   */
  void setRebootOpenNetwork(uint8_t time);
  /**
   * @brief Open the network for device joining for a specified time.
   *
   * Calls the stack when it is running. Intended for a coordinator; the call
   * itself does not check the device role.
   *
   * @param time Time in seconds to keep the network open.
   */
  void openNetwork(uint8_t time);
  /**
   * @brief Close the network to prevent new devices from joining.
   *
   * Calls the stack when it is running. Intended for a coordinator; the call
   * itself does not check the device role.
   */
  void closeNetwork();

  /**
   * @brief Require a router to rejoin during BDB commissioning initialization.
   *
   * Call after `role()`, typically before `begin()`. Useful when a router should
   * re-attach to the coordinator/Trust Center after power loss instead of assuming
   * the previous network link is still valid.
   *
   * @param required true to require router rejoin on BDB init, false to skip it.
   */
  void setRouterRejoinRequired(bool required);
  /**
   * @brief Set Trust Center policy for rejoins using the well-known network key.
   *
   * Coordinator/TC only. Call after `role()`. Defaults depend on the stack; set
   * explicitly for production networks that should reject well-known-key rejoins.
   *
   * @param allow true to allow well-known-key rejoins, false to disallow them.
   */
  void setAllowRejoinsWithWellKnownKey(bool allow);
  /**
   * @brief Get the Trust Center policy for well-known-key rejoins.
   * @return true if well-known-key rejoins are allowed.
   */
  bool getAllowRejoinsWithWellKnownKey();
  // Joiner: distributed security. role() leaves this off. Required for a Philips Hue Bridge.
  void setDistributedSecurity(bool enable);
  // Joiner: 16-byte global Trust Center link key. The library does not ship a default key.
  bool setGlobalLinkKey(const uint8_t *key);

  /**
   * @brief Scan for available Zigbee networks.
   *
   * Scan duration is the time spent on each channel:
   * `((1 << scan_duration) + 1) * 15.36 ms` (IEEE 802.15.4 aBaseSuperframeDuration).
   *
   * @param channel_mask Channels to scan (default: all channels).
   * @param scan_duration Scan duration (default: 5).
   */
  void scanNetworks(uint32_t channel_mask = ZB_TRANSCEIVER_ALL_CHANNELS_MASK, uint8_t scan_duration = 5);
  /**
   * @brief Check whether network scanning is complete.
   * @return `-2` if the scan failed or was not started, `-1` if running, `0` if no
   *         networks were found, or a positive count of networks found.
   */
  int16_t scanComplete();
  /**
   * @brief Get the scan results.
   *
   * Each result includes network identity (PAN ID, channel, capacities, extended
   * PAN ID) plus link quality from the received beacon: `lqa` (0-255, higher is
   * better) and `rssi` (dBm).
   *
   * @return Pointer to scan results, or `NULL` if there are no results.
   */
  zigbee_scan_result_t *getScanResult();
  /**
   * @brief Delete the scan results from memory.
   */
  void scanDelete();

  /**
   * @brief Perform a factory reset, clearing all network settings.
   *
   * @param restart true (default) resets and reboots, and does not return.
   *                false is not supported and does nothing.
   */
  void factoryReset(bool restart = true);

  /**
   * @brief Enable or disable debug mode.
   * @param debug true to enable debug output, false to disable.
   */
  void setDebugMode(bool debug) {
    _debug = debug;
  }
  /**
   * @brief Get the current debug mode setting.
   * @return true if debug mode is enabled.
   */
  bool getDebugMode() {
    return _debug;
  }

  /**
   * @brief Allow or disallow binding the same remote device to multiple local endpoints.
   * @param allow true to allow multi-endpoint binding.
   */
  void allowMultiEndpointBinding(bool allow) {
    _allow_multi_endpoint_binding = allow;
  }
  /**
   * @brief Check whether multi-endpoint binding is allowed.
   * @return true if the same remote device may bind to multiple local endpoints.
   */
  bool allowMultiEndpointBinding() {
    return _allow_multi_endpoint_binding;
  }

  /**
   * @brief Set a global callback for default response messages.
   *
   * Called for all endpoints when a default response is received.
   *
   * @param callback Function pointer, or `nullptr` to clear.
   */
  void onGlobalDefaultResponse(void (*callback)(zb_cmd_type_t resp_to_cmd, ezb_zcl_status_t status, uint8_t endpoint, uint16_t cluster)) {
    _global_default_response_cb = callback;
  }

  /**
   * @brief Invoke the global default-response callback (used internally by endpoints).
   * @param resp_to_cmd Command that the default response answers.
   * @param status ZCL status from the response.
   * @param endpoint Endpoint that received the response.
   * @param cluster Cluster ID of the response.
   */
  void callDefaultResponseCallback(zb_cmd_type_t resp_to_cmd, ezb_zcl_status_t status, uint8_t endpoint, uint16_t cluster);

  /**
   * @brief Set a callback for end-device parent loss.
   *
   * Fired when keepalive failed for a full ED timeout (`EZB_ZDO_SIGNAL_PARENT_LOST`).
   * The callback runs in the Zigbee stack context — keep it short. The stack still
   * performs its default rejoin handling. `connected()` becomes false until the
   * device rejoins.
   *
   * @param callback Function pointer, or `nullptr` to clear.
   */
  void onParentLost(void (*callback)()) {
    _on_parent_lost = callback;
  }

  // Friend function declarations to allow access to private members.
  friend bool zb_app_signal_handler(const ezb_app_signal_t *signal);
  friend bool zb_apsde_data_indication_handler(const ezb_apsde_data_ind_t *ind);

  /**
   * @brief Format an IEEE address for display.
   *
   * Type-agnostic 8-byte input so it works with both `uint8_t[8]` and `ezb_eui64_s::u8`.
   *
   * @param addr IEEE address to format.
   * @return Formatted address string (static buffer, not thread-safe).
   */
  static inline const char *formatIEEEAddress(const uint8_t *addr) {
    static char buf[24];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X", addr[7], addr[6], addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
    return buf;
  }

  /**
   * @brief Format a short address for display.
   * @param addr Short address to format.
   * @return Formatted address string (static buffer, not thread-safe).
   */
  static inline const char *formatShortAddress(uint16_t addr) {
    static char buf[7];
    snprintf(buf, sizeof(buf), "0x%04X", addr);
    return buf;
  }
};

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_ZIGBEE)
extern ZigbeeCore Zigbee;
#endif

#endif  // CONFIG_ZB_ENABLED
