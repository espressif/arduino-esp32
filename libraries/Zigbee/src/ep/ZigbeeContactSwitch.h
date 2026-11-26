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

/* Class of Zigbee contact switch (IAS Zone) endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ezbee/zcl/cluster/ias_zone_desc.h"

/**
 * @brief Contact switch endpoint for door/window sensors and other binary contacts.
 *
 * Implements the Zigbee HA IAS Zone cluster for open/closed contact detection.
 */
class ZigbeeContactSwitch : public ZigbeeEP {
public:
  /**
   * @brief Create a Zigbee contact switch endpoint.
   * @param endpoint Endpoint number (1-254).
   */
  ZigbeeContactSwitch(uint8_t endpoint);
  ~ZigbeeContactSwitch() {}

  /**
   * @brief Set the IAS Client endpoint number (default is 1).
   * @param ep_number IAS Client endpoint number.
   */
  void setIASClientEndpoint(uint8_t ep_number);

  /**
   * @brief Set the contact switch to closed state.
   * @return true if successful.
   */
  bool setClosed();

  /**
   * @brief Set the contact switch to open state.
   * @return true if successful.
   */
  bool setOpen();

  /**
   * @brief Manually report the current contact state.
   *
   * Reporting is also done automatically after setting the position.
   *
   * @return true if successful.
   */
  bool report();

  /**
   * @brief Request a new IAS Zone enrollment.
   *
   * Can enroll a new device or re-enroll an already enrolled device. Check
   * `enrolled()` after waiting for the enrollment response.
   *
   * @return true if the enrollment request was sent.
   */
  bool requestIASZoneEnroll();

  /**
   * @brief Restore IAS Zone enrollment from stored attributes.
   *
   * Call after rebooting an already enrolled device. Restores zone ID and IAS CIE
   * address from flash, which is faster for sleepy devices than a new enrollment.
   * Call after `Zigbee.begin()`.
   *
   * @return true if enrollment was restored (stored attributes must be present).
   */
  bool restoreIASZoneEnroll();

  /**
   * @brief Check whether the device is currently enrolled in the IAS Zone.
   * @return true if enrolled.
   */
  bool enrolled() {
    return _enrolled;
  }

private:
  ezb_zcl_ias_zone_cluster_config_t _ias_zone_cfg;
  void zbIASZoneEnrollResponse(const ezb_zcl_ias_zone_enroll_rsp_message_t *message) override;
  uint16_t _zone_status;  // ZoneStatus bitmap
  uint8_t _zone_id;
  uint8_t _ias_cie_addr[8];  // EUI-64
  uint8_t _ias_cie_endpoint;
  bool _enrolled;
};

#endif  // CONFIG_ZB_ENABLED
