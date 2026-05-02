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

#include "impl/BLEGuards.h"
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

/**
 * @brief Boolean conversion — equivalent to isInitialized().
 *
 * @return true if the BLE stack has been initialized and not yet shut down.
 */
BLEClass::operator bool() const {
  return isInitialized();
}

// --------------------------------------------------------------------------
// Shared implementations (identical across all backends)
// --------------------------------------------------------------------------

/**
 * @brief Check whether the BLE stack has been initialized.
 *
 * @return true if begin() completed successfully and end() has not been called.
 */
bool BLEClass::isInitialized() const {
  return _initialized;
}

/**
 * @brief Get the device name that was set via begin().
 *
 * @return The current device name as a String.
 */
String BLEClass::getDeviceName() const {
  return _deviceName;
}

/**
 * @brief Get the currently configured own-address type.
 *
 * @return The BTAddress::Type in use (Public, Random, RPA, etc.).
 */
BTAddress::Type BLEClass::getOwnAddressType() const {
  return _ownAddressType;
}

/**
 * @brief Check whether an address is on the whitelist.
 *
 * Performs a linear scan of the internal whitelist vector.
 *
 * @param address  The peer address to look up.
 * @return true if the address is currently on the whitelist.
 */
bool BLEClass::isOnWhiteList(const BTAddress &address) const {
  for (const auto &a : _whiteList) {
    if (a == address) {
      return true;
    }
  }
  return false;
}

/**
 * @brief Get the local Identity Resolving Key as a hex string.
 *
 * Delegates to getLocalIRK() for the raw bytes, then hex-encodes them.
 *
 * @return Hex-encoded 16-byte IRK, or an empty string if retrieval fails.
 */
String BLEClass::getLocalIRKString() const {
  uint8_t irk[16];
  if (!getLocalIRK(irk)) {
    return "";
  }
  return HEXBuilder::bytes2hex(irk, sizeof(irk));
}

/**
 * @brief Get the local Identity Resolving Key as a Base64-encoded string.
 *
 * Delegates to getLocalIRK() for the raw bytes, then Base64-encodes them.
 *
 * @return Base64-encoded 16-byte IRK, or an empty string if retrieval fails.
 */
String BLEClass::getLocalIRKBase64() const {
  uint8_t irk[16];
  if (!getLocalIRK(irk)) {
    return "";
  }
  return base64::encode(irk, sizeof(irk));
}

/**
 * @brief Get a bonded peer's IRK as a hex string.
 *
 * @param peer  Address of the bonded peer.
 * @return Hex-encoded 16-byte IRK, or an empty string if the peer is not bonded.
 */
String BLEClass::getPeerIRKString(const BTAddress &peer) const {
  uint8_t irk[16];
  if (!getPeerIRK(peer, irk)) {
    return "";
  }
  return HEXBuilder::bytes2hex(irk, sizeof(irk));
}

/**
 * @brief Get a bonded peer's IRK as a Base64-encoded string.
 *
 * @param peer  Address of the bonded peer.
 * @return Base64-encoded 16-byte IRK, or an empty string if the peer is not bonded.
 */
String BLEClass::getPeerIRKBase64(const BTAddress &peer) const {
  uint8_t irk[16];
  if (!getPeerIRK(peer, irk)) {
    return "";
  }
  return base64::encode(irk, sizeof(irk));
}

/**
 * @brief Get a bonded peer's IRK as a byte-reversed hex string.
 *
 * Returns the 16-byte IRK in reversed byte order, which is the format
 * expected by some Home Assistant and ESPresense integrations.
 *
 * @param peer  Address of the bonded peer.
 * @return Reversed hex-encoded IRK, or an empty string if the peer is not bonded.
 */
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

/**
 * @brief Set the BLE transmit power.
 *
 * Maps the requested dBm value to the nearest ESP power level enum. The
 * actual power is quantized to one of: -12, -9, -6, -3, 0, +3, +6, +9 dBm.
 *
 * @note No-op if the BLE stack has not been initialized.
 * @note On hosted-HCI platforms (e.g. ESP32-P4), this is unsupported and logs a warning.
 *
 * @param txPowerDbm  Desired transmit power in dBm. Clamped to the nearest supported level.
 */
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

/**
 * @brief Get the current BLE transmit power.
 *
 * Reads the ESP power level enum and converts it back to a dBm value.
 *
 * @note Returns -128 if the stack is not initialized (sentinel for "unknown").
 * @note On hosted-HCI platforms, always returns 0 as the power level is not queryable.
 *
 * @return Transmit power in dBm, or -128 if the stack is not initialized.
 */
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
