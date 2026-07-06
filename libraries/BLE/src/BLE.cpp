/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "impl/common/BLEGuards.h"
#if BLE_ENABLED

#include "BLE.h"
#include "HEXBuilder.h"
#include "base64.h"
#include "esp32-hal-log.h"

#if SOC_BLE_SUPPORTED
#include <esp_bt.h>
#endif

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_BLE)
BLEClass BLE;
#endif

BLEClass::operator bool() const {
  return isInitialized();
}

// --------------------------------------------------------------------------
// Shared implementations (identical across all backends)
// --------------------------------------------------------------------------

bool BLEClass::isInitialized() const {
  return _initialized;
}

String BLEClass::getDeviceName() const {
  return _deviceName;
}

BTAddress::Type BLEClass::getOwnAddressType() const {
  return _ownAddressType;
}

bool BLEClass::isOnWhiteList(const BTAddress &address) const {
  // Linear scan of the internal whitelist vector.
  for (const auto &a : _whiteList) {
    if (a == address) {
      return true;
    }
  }
  return false;
}

String BLEClass::getLocalIRKString() const {
  uint8_t irk[16];
  if (!getLocalIRK(irk)) {
    return "";
  }
  return HEXBuilder::bytes2hex(irk, sizeof(irk));
}

String BLEClass::getLocalIRKBase64() const {
  uint8_t irk[16];
  if (!getLocalIRK(irk)) {
    return "";
  }
  return base64::encode(irk, sizeof(irk));
}

String BLEClass::getPeerIRKString(const BTAddress &peer) const {
  uint8_t irk[16];
  if (!getPeerIRK(peer, irk)) {
    return "";
  }
  return HEXBuilder::bytes2hex(irk, sizeof(irk));
}

String BLEClass::getPeerIRKBase64(const BTAddress &peer) const {
  uint8_t irk[16];
  if (!getPeerIRK(peer, irk)) {
    return "";
  }
  return base64::encode(irk, sizeof(irk));
}

String BLEClass::getPeerIRKReverse(const BTAddress &peer) const {
  uint8_t irk[16];
  if (!getPeerIRK(peer, irk)) {
    return "";
  }

  unsigned char reversed[16];
  for (int i = 0; i < 16; i++) {
    reversed[i] = irk[15 - i];
  }
  return HEXBuilder::bytes2hex(reversed, sizeof(reversed));
}

void BLEClass::setPower(int8_t txPowerDbm) {
  if (!_initialized) {
    return;
  }
#if SOC_BLE_SUPPORTED
  esp_power_level_t level;
  if (txPowerDbm <= -12) {
    level = ESP_PWR_LVL_N12;
  } else if (txPowerDbm <= -9) {
    level = ESP_PWR_LVL_N9;
  } else if (txPowerDbm <= -6) {
    level = ESP_PWR_LVL_N6;
  } else if (txPowerDbm <= -3) {
    level = ESP_PWR_LVL_N3;
  } else if (txPowerDbm <= 0) {
    level = ESP_PWR_LVL_N0;
  } else if (txPowerDbm <= 3) {
    level = ESP_PWR_LVL_P3;
  } else if (txPowerDbm <= 6) {
    level = ESP_PWR_LVL_P6;
  } else {
    level = ESP_PWR_LVL_P9;
  }
  log_d("setPower: %d dBm (level=%d)", txPowerDbm, level);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, level);
#else
  log_w("setPower not supported with hosted HCI");
#endif
}

int8_t BLEClass::getPower() const {
  if (!_initialized) {
    return -128;
  }
#if SOC_BLE_SUPPORTED
  switch (esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_DEFAULT)) {
    case ESP_PWR_LVL_N12: return -12;
    case ESP_PWR_LVL_N9:  return -9;
    case ESP_PWR_LVL_N6:  return -6;
    case ESP_PWR_LVL_N3:  return -3;
    case ESP_PWR_LVL_N0:  return 0;
    case ESP_PWR_LVL_P3:  return 3;
    case ESP_PWR_LVL_P6:  return 6;
    case ESP_PWR_LVL_P9:  return 9;
    default:              return -128;
  }
#else
  return 0;
#endif
}

#endif /* BLE_ENABLED */
