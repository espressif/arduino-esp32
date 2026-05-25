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

/* Class of Zigbee door window handle (IAS Zone) endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

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
  bool setClosed();

  // Set the door/window handle value to open
  bool setOpen();

  // Set the door/window handle value to tilted
  bool setTilted();

  // Report the door/window handle value, done automatically after setting the position
  bool report();

  // Request a new IAS zone enroll, can be called to enroll a new device or to re-enroll an already enrolled device
  bool requestIASZoneEnroll();

  // Restore IAS Zone enroll, needed to be called after rebooting already enrolled device - restored from flash memory (faster for sleepy devices)
  bool restoreIASZoneEnroll();

  // Check if the device is enrolled in the IAS Zone
  bool enrolled() {
    return _enrolled;
  }

private:
  void zbIASZoneEnrollResponse(const esp_zb_zcl_ias_zone_enroll_response_message_t *message) override;
  uint8_t _zone_status;
  uint8_t _zone_id;
  esp_zb_ieee_addr_t _ias_cie_addr;
  uint8_t _ias_cie_endpoint;
  bool _enrolled;
};

#endif  // CONFIG_ZB_ENABLED
