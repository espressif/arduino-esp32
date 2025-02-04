/* Class of Zigbee door window handle (IAS Zone) endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if SOC_IEEE802154_SUPPORTED && CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ha/esp_zigbee_ha_standard.h"

#define ESP_ZB_ZCL_IAS_ZONE_ZONETYPE_DOOR_WINDOW_HANDLE 0x0016
// clang-format off
#define ZIGBEE_DEFAULT_DOOR_WINDOW_HANDLE_CONFIG()                                                  \
    {                                                                                               \
        .basic_cfg =                                                                                \
            {                                                                                       \
                .zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,                          \
                .power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_DEFAULT_VALUE,                        \
            },                                                                                      \
        .identify_cfg =                                                                             \
            {                                                                                       \
                .identify_time = ESP_ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE,                   \
            },                                                                                      \
        .ias_zone_cfg =                                                                             \
            {                                                                                       \
                .zone_state = ESP_ZB_ZCL_IAS_ZONE_ZONESTATE_NOT_ENROLLED,                           \
                .zone_type = ESP_ZB_ZCL_IAS_ZONE_ZONETYPE_DOOR_WINDOW_HANDLE,                       \
                .zone_status = 0,                                                                   \
                .ias_cie_addr = ESP_ZB_ZCL_ZONE_IAS_CIE_ADDR_DEFAULT,                               \
                .zone_id = 0xff,                                                                    \
                .zone_ctx = {0, 0, 0, 0},                                                           \
            },                                                                                      \
    }
// clang-format on

typedef struct zigbee_door_window_handle_cfg_s {
  esp_zb_basic_cluster_cfg_t basic_cfg;
  esp_zb_identify_cluster_cfg_t identify_cfg;
  esp_zb_ias_zone_cluster_cfg_t ias_zone_cfg;
} zigbee_door_window_handle_cfg_t;

class ZigbeeDoorWindowHandle : public ZigbeeEP {
public:
  ZigbeeDoorWindowHandle(uint8_t endpoint);
  ~ZigbeeDoorWindowHandle() {}

  // Set the IAS Client endpoint number (default is 1)
  void setIASClientEndpoint(uint8_t ep_number);

  // Set the door/window handle value to closed
  void setClosed();

  // Set the door/window handle value to open
  void setOpen();

  // Set the door/window handle value to tilted
  void setTilted();

  // Report the door/window handle value, done automatically after setting the position
  void report();

private:
  void zbIASZoneEnrollResponse(const esp_zb_zcl_ias_zone_enroll_response_message_t *message) override;
  uint8_t _zone_status;
  uint8_t _zone_id;
  esp_zb_ieee_addr_t _ias_cie_addr;
  uint8_t _ias_cie_endpoint;
};

#endif  //SOC_IEEE802154_SUPPORTED && CONFIG_ZB_ENABLED
