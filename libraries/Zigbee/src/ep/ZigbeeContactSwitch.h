/* Class of Zigbee contact switch (IAS Zone) endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ha/esp_zigbee_ha_standard.h"

// clang-format off
#define ZIGBEE_DEFAULT_CONTACT_SWITCH_CONFIG()                                                      \
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
                .zone_type = ESP_ZB_ZCL_IAS_ZONE_ZONETYPE_CONTACT_SWITCH,                           \
                .zone_status = 0,                                                                   \
                .ias_cie_addr = ESP_ZB_ZCL_ZONE_IAS_CIE_ADDR_DEFAULT,                               \
                .zone_id = 0xff,                                                                    \
                .zone_ctx = {0, 0, 0, 0},                                                           \
            },                                                                                      \
    }
// clang-format on

typedef struct zigbee_contact_switch_cfg_s {
  esp_zb_basic_cluster_cfg_t basic_cfg;
  esp_zb_identify_cluster_cfg_t identify_cfg;
  esp_zb_ias_zone_cluster_cfg_t ias_zone_cfg;
} zigbee_contact_switch_cfg_t;

class ZigbeeContactSwitch : public ZigbeeEP {
public:
  ZigbeeContactSwitch(uint8_t endpoint);
  ~ZigbeeContactSwitch() {}

  // Set the IAS Client endpoint number (default is 1)
  void setIASClientEndpoint(uint8_t ep_number);

  // Set the contact switch value to closed
  bool setClosed();

  // Set the contact switch value to open
  bool setOpen();

  // Report the contact switch value, done automatically after setting the position
  bool report();

private:
  void zbIASZoneEnrollResponse(const esp_zb_zcl_ias_zone_enroll_response_message_t *message) override;
  uint8_t _zone_status;
  uint8_t _zone_id;
  esp_zb_ieee_addr_t _ias_cie_addr;
  uint8_t _ias_cie_endpoint;
};

#endif  // CONFIG_ZB_ENABLED
