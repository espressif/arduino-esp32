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

/* Class of Zigbee contact switch (IAS Zone) endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ezbee/zcl/cluster/ias_zone_desc.h"

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

  // Request a new IAS zone enroll, can be called to enroll a new device or to re-enroll an already enrolled device
  bool requestIASZoneEnroll();

  // Restore IAS Zone enroll, needed to be called after rebooting already enrolled device - restored from flash memory (faster for sleepy devices)
  bool restoreIASZoneEnroll();

  // Check if the device is enrolled in the IAS Zone
  bool enrolled() {
    return _enrolled;
  }

private:
  ezb_zcl_ias_zone_cluster_config_t _ias_zone_cfg;
  void zbIASZoneEnrollResponse(const ezb_zcl_ias_zone_enroll_rsp_message_t *message) override;
  uint16_t _zone_status;  // ZoneStatus is a 16-bit bitmap in v2.x
  uint8_t _zone_id;
  uint8_t _ias_cie_addr[8];  // EUI-64 (was esp_zb_ieee_addr_t)
  uint8_t _ias_cie_endpoint;
  bool _enrolled;
};

#endif  // CONFIG_ZB_ENABLED
