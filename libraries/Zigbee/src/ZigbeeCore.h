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

/* Zigbee core class */

// =====================================================================================
// ESP-Zigbee-SDK v2.x migration (ZigbeeCore) -- STEP 1 of the library migration.
//
// This file targets the v2.x SDK (esp_zigbee.h + the ezbee/* core layer). It will NOT
// compile until:
//   1) the Arduino toolchain (esp32-arduino-libs / esp32-arduino-lib-builder) bundles
//      esp-zigbee-lib v2.x, and
//   2) the rest of the library (ZigbeeTypes, ZigbeeEP, ZigbeeHandlers and every src/ep/*)
//      is migrated to the new ZCL data model.
//
// Boundaries that are owned by the not-yet-migrated ZCL / ZigbeeEP layer are flagged with
// `TODO(zb-v2):` so they can be resolved in the follow-up steps.
// =====================================================================================

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "esp_zigbee.h"                 // all-in-one v2.x header (was esp_zigbee_core.h)
#include "ezbee/zha.h"                  // ZHA standard device IDs (EZB_ZHA_*_DEVICE_ID); not pulled in by esp_zigbee.h
#include "ezbee/zdo/zdo_nwk_mgmt.h"     // Mgmt_Bind_req (binding table retrieval)
#include "ezbee/zdo/zdo_dev_srv_disc.h" // Match_Desc_req (endpoint discovery)
#include <esp32-hal-log.h>
#include <list>
#include "ZigbeeTypes.h"
#include "ZigbeeEP.h"
class ZigbeeEP;

typedef void (*voidFuncPtr)(void);
typedef void (*voidFuncPtrArg)(void *);

// v2.x active-scan beacon descriptor (was esp_zb_network_descriptor_t)
typedef ezb_nwk_active_scan_result_t zigbee_scan_result_t;

// enum of Zigbee Roles (values match ezb_nwk_device_type_t)
typedef enum {
  ZIGBEE_COORDINATOR = 0,
  ZIGBEE_ROUTER = 1,
  ZIGBEE_END_DEVICE = 2
} zigbee_role_t;

#define ZB_SCAN_RUNNING (-1)
#define ZB_SCAN_FAILED  (-2)

#define ZB_BEGIN_TIMEOUT_DEFAULT 30000  // 30 seconds

// All 2.4GHz channels (11-26). Replaces removed ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK.
#define ZB_TRANSCEIVER_ALL_CHANNELS_MASK 0x07FFF800

#define ZIGBEE_DEFAULT_ED_CONFIG()                  \
  {                                                 \
    .device_type = EZB_NWK_DEVICE_TYPE_END_DEVICE,  \
    .install_code_policy = false,                   \
    .zed_config =                                   \
      {                                             \
        .ed_timeout = EZB_NWK_ED_TIMEOUT_64MIN,     \
        .keep_alive = 3000,                         \
      },                                            \
  }

#define ZIGBEE_DEFAULT_ROUTER_CONFIG()           \
  {                                              \
    .device_type = EZB_NWK_DEVICE_TYPE_ROUTER,   \
    .install_code_policy = false,                \
    .zczr_config = {.max_children = 10},         \
  }

#define ZIGBEE_DEFAULT_COORDINATOR_CONFIG()           \
  {                                                   \
    .device_type = EZB_NWK_DEVICE_TYPE_COORDINATOR,   \
    .install_code_policy = false,                     \
    .zczr_config = {.max_children = 10},              \
  }

// v2.x radio config: host_config was removed; uart field order changed.
#define ZIGBEE_DEFAULT_UART_RCP_RADIO_CONFIG()     \
  {                                                \
    .radio_mode = ESP_ZIGBEE_RADIO_MODE_UART_RCP,  \
    .radio_uart_config = {                         \
      .port = UART_NUM_1,                          \
      .uart_config =                               \
        {                                          \
          .baud_rate = 460800,                     \
          .data_bits = UART_DATA_8_BITS,           \
          .parity = UART_PARITY_DISABLE,           \
          .stop_bits = UART_STOP_BITS_1,           \
          .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,   \
          .rx_flow_ctrl_thresh = 0,                \
          .source_clk = UART_SCLK_DEFAULT,         \
        },                                         \
      .rx_pin = GPIO_NUM_NC,                       \
      .tx_pin = GPIO_NUM_NC,                       \
    },                                             \
  }

class ZigbeeCore {
private:
  esp_zigbee_radio_config_t _radio_config;  // host_config removed in v2.x
  uint32_t _primary_channel_mask;
  uint32_t _begin_timeout;
  int16_t _scan_status;
  uint8_t _scan_duration;
  bool _rx_on_when_idle;

  ezb_af_device_desc_t _zb_dev_desc;  // was esp_zb_ep_list_t *_zb_ep_list
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
  ZigbeeCore();
  ~ZigbeeCore() {}

  std::list<ZigbeeEP *> ep_objects;

  // Lifecycle (all steps are mandatory):
  //   role(role)      — esp_zigbee_init() + commissioning setup. Must be the first Zigbee call.
  //   role(cfg*)      — same with a custom esp_zigbee_device_config_t (sleepy ED, max_children, …).
  //   configure EPs   — call EP setters (setManufacturerAndModel, etc.) after role().
  //   addEndpoint(ep) — store the endpoint pointer; before begin().
  //   begin()         — attach all EPs, register the device descriptor and start the stack.
  bool role(zigbee_role_t role, bool erase_nvs = false);
  bool role(esp_zigbee_device_config_t *role_cfg, bool erase_nvs = false);
  bool begin();
  void stop();
  void start();

  bool initialized() {
    return _initialized;
  }
  bool endpointsRegistered() {
    return _endpoints_registered;
  }
  bool stackRunning() {
    return _stack_running;
  }
  bool started() {
    return _started;
  }
  bool paused() {
    return _paused;
  }
  bool connected() {
    return _connected;
  }

  zigbee_role_t getRole() {
    return _role;
  }

  bool addEndpoint(ZigbeeEP *ep);
  //void removeEndpoint(ZigbeeEP *ep);
  
  //   setRadioConfig() / setPrimaryChannelMask() must be called before role() is called.
  void setRadioConfig(esp_zigbee_radio_config_t config);
  esp_zigbee_radio_config_t getRadioConfig();

  // NOTE(zb-v2): host_config was removed from the SDK; the host-config accessors are gone.

  void setPrimaryChannelMask(uint32_t mask);  // By default all channels are scanned (11-26) -> mask 0x07FFF800

  void setScanDuration(uint8_t duration);  // Can be set from 1 - 4. 1 is fastest, 4 is slowest
  uint8_t getScanDuration() {
    return _scan_duration;
  }

  void setRxOnWhenIdle(bool rx_on_when_idle) {
    _rx_on_when_idle = rx_on_when_idle;
  }
  bool getRxOnWhenIdle() {
    return _rx_on_when_idle;
  }
  void setTimeout(uint32_t timeout) {
    _begin_timeout = timeout;
  }
  void setRebootOpenNetwork(uint8_t time);
  void openNetwork(uint8_t time);
  void closeNetwork();

  //scan_duration Time spent scanning each channel, in units of ((1 << scan_duration) + 1) * a beacon time. (15.36 microseconds)
  void scanNetworks(uint32_t channel_mask = ZB_TRANSCEIVER_ALL_CHANNELS_MASK, uint8_t scan_duration = 5);
  // Zigbee scan complete status check, -2: failed or not started, -1: running, 0: no networks found, >0: number of networks found
  int16_t scanComplete();
  zigbee_scan_result_t *getScanResult();
  void scanDelete();

  void factoryReset(bool restart = true);

  void setDebugMode(bool debug) {
    _debug = debug;
  }
  bool getDebugMode() {
    return _debug;
  }

  void allowMultiEndpointBinding(bool allow) {
    _allow_multi_endpoint_binding = allow;
  }
  bool allowMultiEndpointBinding() {
    return _allow_multi_endpoint_binding;
  }

  // Set global default response callback
  void onGlobalDefaultResponse(void (*callback)(zb_cmd_type_t resp_to_cmd, ezb_zcl_status_t status, uint8_t endpoint, uint16_t cluster)) {
    _global_default_response_cb = callback;
  }

  // Call global default response callback (for internal use)
  void callDefaultResponseCallback(zb_cmd_type_t resp_to_cmd, ezb_zcl_status_t status, uint8_t endpoint, uint16_t cluster);

  // Friend function declarations to allow access to private members.
  // v2.x: the application signal handler is registered via ezb_app_signal_add_handler()
  // (it returns bool and receives an opaque ezb_app_signal_t*), instead of overriding the
  // old weak esp_zb_app_signal_handler() symbol.
  friend bool zb_app_signal_handler(const ezb_app_signal_t *signal);
  friend bool zb_apsde_data_indication_handler(const ezb_apsde_data_ind_t *ind);

  // Helper functions for formatting addresses.
  // Type-agnostic 8-byte input so it works with both uint8_t[8] and ezb_eui64_s::u8.
  static inline const char *formatIEEEAddress(const uint8_t *addr) {
    static char buf[24];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X", addr[7], addr[6], addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
    return buf;
  }

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
