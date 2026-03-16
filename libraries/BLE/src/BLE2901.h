/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
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
  BLE2901.h

  GATT Descriptor 0x2901 Characteristic User Description

  The value of this description is a user-readable string
  describing the characteristic.

  The Characteristic User Description descriptor
  provides a textual user description for a characteristic
  value.
  If the Writable Auxiliary bit of the Characteristics
  Properties is set then this descriptor is written. Only one
  User Description descriptor exists in a characteristic
  definition.
*/

#ifndef COMPONENTS_CPP_UTILS_BLE2901_H_
#define COMPONENTS_CPP_UTILS_BLE2901_H_

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if defined(SOC_BLE_SUPPORTED) || defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
#if defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)

/***************************************************************************
 *                           Common includes                               *
 ***************************************************************************/

#include "BLEDescriptor.h"

/**
 * @brief GATT Descriptor 0x2901 Characteristic User Description
 */
class BLE2901 : public BLEDescriptor {
public:
  /***************************************************************************
   *                         Common public functions                         *
   ***************************************************************************/

  BLE2901();
  void setDescription(const String &desc);
};  // BLE2901

#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED || CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE */

#endif /* COMPONENTS_CPP_UTILS_BLE2901_H_ */
