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
#include "ezbee/zha.h"
#include "ezbee/zcl/cluster/basic_desc.h"
#include "ezbee/zcl/cluster/identify_desc.h"
#include "ezbee/zcl/cluster/ias_zone.h"
#include "ezbee/zcl/cluster/ias_zone_desc.h"

// clang-format off
// NOTE(zb-v2): v1 defined a local ESP_ZB_ZCL_IAS_ZONE_ZONETYPE_DOOR_WINDOW_HANDLE because the v1 SDK lacked
// the enum value. v2.x provides EZB_ZCL_IAS_ZONE_ZONE_TYPE_DOOR_WINDOW_HANDLE in ias_zone_desc.h, so the
// local define is dropped.
#define ZIGBEE_DEFAULT_DOOR_WINDOW_HANDLE_CONFIG()                                                  \
    {                                                                                               \
        .basic_cfg =                                                                                \
            {                                                                                       \
                .zcl_version = EZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,                             \
                .power_source = EZB_ZCL_BASIC_POWER_SOURCE_DEFAULT_VALUE,                           \
            },                                                                                      \
        .identify_cfg =                                                                             \
            {                                                                                       \
                .identify_time = EZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE,                      \
            },                                                                                      \
        .ias_zone_cfg =                                                                             \
            {                                                                                       \
                .zone_state = EZB_ZCL_IAS_ZONE_ZONE_STATE_NOT_ENROLLED,                             \
                .zone_type = EZB_ZCL_IAS_ZONE_ZONE_TYPE_DOOR_WINDOW_HANDLE,                         \
                .zone_status = EZB_ZCL_IAS_ZONE_ZONE_STATUS_DEFAULT_VALUE,                          \
                .ias_cie_address = 0,                                                               \
                .zone_id = EZB_ZCL_IAS_ZONE_ZONE_ID_DEFAULT_VALUE,                                  \
            },                                                                                      \
    }
// clang-format on

// NOTE(zb-v2): the v2.x IasZone server config struct replaces the v1 ias_cie_addr byte array with a single
// uint64_t ias_cie_address and drops the zone_ctx field.
typedef struct zigbee_door_window_handle_cfg_s {
  ezb_zcl_basic_cluster_config_t basic_cfg;
  ezb_zcl_identify_cluster_config_t identify_cfg;
  ezb_zcl_ias_zone_cluster_config_t ias_zone_cfg;
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
  void zbIASZoneEnrollResponse(const ezb_zcl_ias_zone_enroll_rsp_message_t *message) override;
  uint16_t _zone_status;  // ZoneStatus is a 16-bit bitmap in v2.x
  uint8_t _zone_id;
  uint8_t _ias_cie_addr[8];  // EUI-64 (was esp_zb_ieee_addr_t)
  uint8_t _ias_cie_endpoint;
  bool _enrolled;
};

#endif  // CONFIG_ZB_ENABLED
