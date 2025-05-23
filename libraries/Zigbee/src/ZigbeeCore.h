/* Zigbee core class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "esp_zigbee_core.h"
#include "zdo/esp_zigbee_zdo_common.h"
#include "aps/esp_zigbee_aps.h"
#include <esp32-hal-log.h>
#include <list>
#include "ZigbeeEP.h"
class ZigbeeEP;

typedef void (*voidFuncPtr)(void);
typedef void (*voidFuncPtrArg)(void *);

typedef esp_zb_network_descriptor_t zigbee_scan_result_t;

// enum of Zigbee Roles
typedef enum {
  ZIGBEE_COORDINATOR = 0,
  ZIGBEE_ROUTER = 1,
  ZIGBEE_END_DEVICE = 2
} zigbee_role_t;

#define ZB_SCAN_RUNNING (-1)
#define ZB_SCAN_FAILED  (-2)

#define ZB_BEGIN_TIMEOUT_DEFAULT 30000  // 30 seconds

#define ZIGBEE_DEFAULT_ED_CONFIG()                                      \
  {                                                                     \
    .esp_zb_role = ESP_ZB_DEVICE_TYPE_ED, .install_code_policy = false, \
    .nwk_cfg = {                                                        \
      .zed_cfg =                                                        \
        {                                                               \
          .ed_timeout = ESP_ZB_ED_AGING_TIMEOUT_64MIN,                  \
          .keep_alive = 3000,                                           \
        },                                                              \
    },                                                                  \
  }

#define ZIGBEE_DEFAULT_ROUTER_CONFIG()                                                   \
  {                                                                                      \
    .esp_zb_role = ESP_ZB_DEVICE_TYPE_ROUTER, .install_code_policy = false, .nwk_cfg = { \
      .zczr_cfg =                                                                        \
        {                                                                                \
          .max_children = 10,                                                            \
        },                                                                               \
    }                                                                                    \
  }

#define ZIGBEE_DEFAULT_COORDINATOR_CONFIG()                                                   \
  {                                                                                           \
    .esp_zb_role = ESP_ZB_DEVICE_TYPE_COORDINATOR, .install_code_policy = false, .nwk_cfg = { \
      .zczr_cfg =                                                                             \
        {                                                                                     \
          .max_children = 10,                                                                 \
        },                                                                                    \
    }                                                                                         \
  }

#define ZIGBEE_DEFAULT_UART_RCP_RADIO_CONFIG()   \
  {                                              \
    .radio_mode = ZB_RADIO_MODE_UART_RCP,        \
    .radio_uart_config = {                       \
      .port = UART_NUM_1,                        \
      .rx_pin = GPIO_NUM_NC,                     \
      .tx_pin = GPIO_NUM_NC,                     \
      .uart_config =                             \
        {                                        \
          .baud_rate = 460800,                   \
          .data_bits = UART_DATA_8_BITS,         \
          .parity = UART_PARITY_DISABLE,         \
          .stop_bits = UART_STOP_BITS_1,         \
          .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, \
          .rx_flow_ctrl_thresh = 0,              \
          .source_clk = UART_SCLK_DEFAULT,       \
        },                                       \
    },                                           \
  }

class ZigbeeCore {
private:
  esp_zb_radio_config_t _radio_config;
  esp_zb_host_config_t _host_config;
  uint32_t _primary_channel_mask;
  uint32_t _begin_timeout;
  int16_t _scan_status;
  uint8_t _scan_duration;
  bool _rx_on_when_idle;

  esp_zb_ep_list_t *_zb_ep_list;
  zigbee_role_t _role;
  bool _started;
  bool _connected;

  uint8_t _open_network;
  zigbee_scan_result_t *_scan_result;
  SemaphoreHandle_t lock;
  bool _debug;

  bool zigbeeInit(esp_zb_cfg_t *zb_cfg, bool erase_nvs);
  static void scanCompleteCallback(esp_zb_zdp_status_t zdo_status, uint8_t count, esp_zb_network_descriptor_t *nwk_descriptor);
  const char *getDeviceTypeString(esp_zb_ha_standard_devices_t deviceId);
  void searchBindings();
  static void bindingTableCb(const esp_zb_zdo_binding_table_info_t *table_info, void *user_ctx);
  void resetNVRAMChannelMask();             // Reset to default mask also in NVRAM
  void setNVRAMChannelMask(uint32_t mask);  // Set channel mask in NVRAM

public:
  ZigbeeCore();
  ~ZigbeeCore() {}

  std::list<ZigbeeEP *> ep_objects;

  bool begin(zigbee_role_t role = ZIGBEE_END_DEVICE, bool erase_nvs = false);
  bool begin(esp_zb_cfg_t *role_cfg, bool erase_nvs = false);
  // bool end();

  bool started() {
    return _started;
  }
  bool connected() {
    return _connected;
  }
  zigbee_role_t getRole() {
    return _role;
  }

  bool addEndpoint(ZigbeeEP *ep);
  //void removeEndpoint(ZigbeeEP *ep);

  void setRadioConfig(esp_zb_radio_config_t config);
  esp_zb_radio_config_t getRadioConfig();

  void setHostConfig(esp_zb_host_config_t config);
  esp_zb_host_config_t getHostConfig();

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
  void scanNetworks(uint32_t channel_mask = ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK, uint8_t scan_duration = 5);
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

  // Friend function declaration to allow access to private members
  friend void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct);
  friend bool zb_apsde_data_indication_handler(esp_zb_apsde_data_ind_t ind);

  // Helper functions for formatting addresses
  static inline const char *formatIEEEAddress(const esp_zb_ieee_addr_t addr) {
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

extern ZigbeeCore Zigbee;

#endif  // CONFIG_ZB_ENABLED
