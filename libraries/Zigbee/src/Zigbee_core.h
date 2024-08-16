/* Zigbee core class */

#pragma once

#include "esp_zigbee_core.h"
#include "zdo/esp_zigbee_zdo_common.h"
#include <esp32-hal-log.h>
#include <list>
#include "Zigbee_ep.h"
class Zigbee_EP;

typedef void (*voidFuncPtr)(void);
typedef void (*voidFuncPtrArg)(void *);

// structure to store callback function and argument
typedef struct {
  voidFuncPtr fn;
  void *arg;
} zigbee_cb_t;

typedef esp_zb_network_descriptor_t zigbee_scan_result_t;

// enum of Zigbee Roles
typedef enum {
    ZIGBEE_COORDINATOR = 0, 
    ZIGBEE_ROUTER = 1, 
    ZIGBEE_END_DEVICE = 2
} zigbee_role_t;

// default Zigbee configuration for each role
#define INSTALLCODE_POLICY_ENABLE   false /* enable the install code policy for security */
#define ED_AGING_TIMEOUT            ESP_ZB_ED_AGING_TIMEOUT_64MIN
#define ED_KEEP_ALIVE               3000 /* 3000 millisecond */
#define MAX_CHILDREN                10

#define ZB_SCAN_RUNNING (-1)
#define ZB_SCAN_FAILED  (-2)

#define ZIGBEE_DEFAULT_ED_CONFIG()                         \
  {                                                        \
    .esp_zb_role = ESP_ZB_DEVICE_TYPE_ED,                  \
    .install_code_policy = INSTALLCODE_POLICY_ENABLE,      \
    .nwk_cfg = {                                           \
      .zed_cfg =                                           \
        {                                                  \
          .ed_timeout = ED_AGING_TIMEOUT,                  \
          .keep_alive = ED_KEEP_ALIVE,                     \
        },                                                 \
    },                                                     \
  }

#define ZIGBEE_DEFAULT_ROUTER_CONFIG()                     \
  {                                                        \
    .esp_zb_role = ESP_ZB_DEVICE_TYPE_COORDINATOR,         \
    .install_code_policy = INSTALLCODE_POLICY_ENABLE,      \
    .nwk_cfg = {                                           \
      .zczr_cfg =                                          \
        {                                                  \
          .max_children = MAX_CHILDREN,                    \
        },                                                 \
    }                                                      \
  }


#define ZIGBEE_DEFAULT_COORDINATOR_CONFIG()                \
  {                                                        \
    .esp_zb_role = ESP_ZB_DEVICE_TYPE_COORDINATOR,         \
    .install_code_policy = INSTALLCODE_POLICY_ENABLE,      \
    .nwk_cfg = {                                           \
      .zczr_cfg =                                          \
        {                                                  \
          .max_children = MAX_CHILDREN,                    \
        },                                                 \
    }                                                      \
  }

// default Zigbee radio and host configuration
#define ZIGBEE_DEFAULT_RADIO_CONFIG() \
  { .radio_mode = ZB_RADIO_MODE_NATIVE, }

#define ZIGBEE_DEFAULT_HOST_CONFIG() \
  { .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE, }

class Zigbee_Core {
    private:
        esp_zb_radio_config_t _radio_config;
        esp_zb_host_config_t _host_config;
        uint32_t _primary_channel_mask;
        int16_t _scan_status;


        bool zigbeeInit(esp_zb_cfg_t *zb_cfg, bool erase_nvs);
        static void scanCompleteCallback(esp_zb_zdp_status_t zdo_status, uint8_t count, esp_zb_network_descriptor_t *nwk_descriptor);

    public:
        esp_zb_ep_list_t *_zb_ep_list;
        zigbee_role_t _role;
        bool _started;
        uint8_t _open_network;
        std::list<Zigbee_EP*> ep_objects;
        zigbee_scan_result_t *_scan_result;

        Zigbee_Core();
        ~Zigbee_Core();

        bool begin(zigbee_role_t role = ZIGBEE_END_DEVICE, bool erase_nvs = false);
        bool begin(esp_zb_cfg_t *role_cfg, bool erase_nvs = false);
        bool isStarted();
        // bool end();

        void addEndpoint(Zigbee_EP *ep);
        //void removeEndpoint(Zigbee_EP *ep);

        void setRadioConfig(esp_zb_radio_config_t config);
        esp_zb_radio_config_t getRadioConfig();

        void setHostConfig(esp_zb_host_config_t config);
        esp_zb_host_config_t getHostConfig();

        void setPrimaryChannelMask(uint32_t mask);
        void setRebootOpenNetwork(uint8_t time);

        //scan_duration Time spent scanning each channel, in units of ((1 << scan_duration) + 1) * a beacon time. (15.36 microseconds)
        void scanNetworks(uint32_t channel_mask = ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK, uint8_t scan_duration = 5);
        // Zigbee scan complete status check, -2: failed or not started, -1: running, 0: no networks found, >0: number of networks found
        int16_t scanComplete();
        zigbee_scan_result_t* getScanResult();
        void scanDelete();

        void factoryReset();

        const char* getDeviceTypeString(esp_zb_ha_standard_devices_t deviceId);
};

extern Zigbee_Core Zigbee;
