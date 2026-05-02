/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 * Copyright 2018 pcbreflux
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

#pragma once

#include "impl/BLEGuards.h"
#if BLE_ENABLED

#include "BLEUUID.h"
#include "BLEAdvertisementData.h"
#include "WString.h"

/**
 * @brief Eddystone URL frame builder/parser.
 */
class BLEEddystoneURL {
public:
  /**
   * @brief Construct an Eddystone-URL frame with default TX power (-20 dBm).
   */
  BLEEddystoneURL();

  /**
   * @brief Set the URL to advertise.
   * @param url Full URL string (e.g. "https://example.com"). Prefix and known
   *            suffixes are automatically compressed per the Eddystone-URL spec.
   */
  void setURL(const String &url);

  /**
   * @brief Get the currently configured URL.
   * @return The decoded URL string.
   */
  String getURL() const;

  /**
   * @brief Set the calibrated TX power at 0 meters.
   * @param txPower TX power in dBm.
   */
  void setTxPower(int8_t txPower);

  /**
   * @brief Get the calibrated TX power at 0 meters.
   * @return TX power in dBm.
   */
  int8_t getTxPower() const;

  /**
   * @brief Build a BLEAdvertisementData payload from the current Eddystone-URL fields.
   * @return Advertisement data ready to pass to BLEAdvertising.
   */
  BLEAdvertisementData getAdvertisementData() const;

  /**
   * @brief Parse Eddystone-URL fields from raw service data.
   * @param data Pointer to the Eddystone service data bytes (after the UUID).
   * @param len  Length of the service data in bytes.
   */
  void setFromServiceData(const uint8_t *data, size_t len);

  /**
   * @brief Get the Eddystone service UUID (0xFEAA).
   * @return The 16-bit Eddystone service UUID.
   */
  static BLEUUID serviceUUID();

private:
  int8_t _txPower = -20;
  String _url;

  static uint8_t encodePrefix(const String &url, size_t &consumed);
  static uint8_t encodeSuffix(const String &url, size_t pos, size_t &consumed);
  static String decodePrefix(uint8_t code);
  static String decodeSuffix(uint8_t code);
};

/**
 * @brief Eddystone TLM (telemetry) frame builder/parser.
 */
class BLEEddystoneTLM {
public:
  /**
   * @brief Construct an Eddystone-TLM frame with zeroed telemetry values.
   */
  BLEEddystoneTLM();

  /**
   * @brief Set the battery voltage.
   * @param millivolts Battery voltage in millivolts (0 = not supported).
   */
  void setBatteryVoltage(uint16_t millivolts);

  /**
   * @brief Get the battery voltage.
   * @return Battery voltage in millivolts.
   */
  uint16_t getBatteryVoltage() const;

  /**
   * @brief Set the beacon temperature.
   * @param celsius Temperature in degrees Celsius.
   */
  void setTemperature(float celsius);

  /**
   * @brief Get the beacon temperature.
   * @return Temperature in degrees Celsius.
   */
  float getTemperature() const;

  /**
   * @brief Set the advertising PDU count since power-on or reboot.
   * @param count Number of advertisement frames sent.
   */
  void setAdvertisingCount(uint32_t count);

  /**
   * @brief Get the advertising PDU count.
   * @return Number of advertisement frames sent since last reboot.
   */
  uint32_t getAdvertisingCount() const;

  /**
   * @brief Set the time since power-on or reboot.
   * @param deciseconds Uptime in units of 0.1 seconds.
   */
  void setUptime(uint32_t deciseconds);

  /**
   * @brief Get the time since power-on or reboot.
   * @return Uptime in units of 0.1 seconds.
   */
  uint32_t getUptime() const;

  /**
   * @brief Build a BLEAdvertisementData payload from the current TLM fields.
   * @return Advertisement data ready to pass to BLEAdvertising.
   */
  BLEAdvertisementData getAdvertisementData() const;

  /**
   * @brief Parse Eddystone-TLM fields from raw service data.
   * @param data Pointer to the Eddystone service data bytes (after the UUID).
   * @param len  Length of the service data in bytes.
   */
  void setFromServiceData(const uint8_t *data, size_t len);

  /**
   * @brief Get a human-readable summary of the TLM frame.
   * @return Formatted string with voltage, temperature, count, and uptime.
   */
  String toString() const;

  /**
   * @brief Get the Eddystone service UUID (0xFEAA).
   * @return The 16-bit Eddystone service UUID.
   */
  static BLEUUID serviceUUID();

private:
  uint16_t _voltage = 0;
  int16_t _rawTemp = 0;
  uint32_t _advCount = 0;
  uint32_t _uptime = 0;
};

#endif /* BLE_ENABLED */
