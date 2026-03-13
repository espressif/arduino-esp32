/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 * Copyright 2020-2025 Ryan Powell <ryan@nable-embedded.io> and
 * esp-nimble-cpp, NimBLE-Arduino contributors.
 * Copyright 2017 Neil Kolban
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * BLEBeacon2.h
 *
 *  Created on: Jan 4, 2018
 *      Author: kolban
 *
 *  Modified on: Feb 18, 2025
 *      Author: lucasssvaz (based on kolban's and h2zero's work)
 *      Description: Added support for NimBLE
 */

#ifndef COMPONENTS_CPP_UTILS_BLEBEACON_H_
#define COMPONENTS_CPP_UTILS_BLEBEACON_H_

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if defined(SOC_BLE_SUPPORTED) || defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
#if defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)

/***************************************************************************
 *                           Common includes                               *
 ***************************************************************************/

#include "BLEUUID.h"
/**
 * @brief Representation of a beacon.
 * See:
 * * https://en.wikipedia.org/wiki/IBeacon
 */
class BLEBeacon {
private:
  /***************************************************************************
   *                           Common types                                  *
   ***************************************************************************/

  struct {
    uint16_t manufacturerId;
    uint8_t subType;
    uint8_t subTypeLength;
    uint8_t proximityUUID[16];
    uint16_t major;
    uint16_t minor;
    int8_t signalPower;
  } __attribute__((packed)) m_beaconData;

public:
  /***************************************************************************
   *                           Common public declarations                    *
   ***************************************************************************/

  BLEBeacon();
  String getData();
  uint16_t getMajor();
  uint16_t getMinor();
  uint16_t getManufacturerId();
  BLEUUID getProximityUUID();
  int8_t getSignalPower();
  void setData(const String &data);
  void setMajor(uint16_t major);
  void setMinor(uint16_t minor);
  void setManufacturerId(uint16_t manufacturerId);
  void setProximityUUID(BLEUUID uuid);
  void setSignalPower(int8_t signalPower);
};  // BLEBeacon

#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED || CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE */

#endif /* COMPONENTS_CPP_UTILS_BLEBEACON_H_ */
