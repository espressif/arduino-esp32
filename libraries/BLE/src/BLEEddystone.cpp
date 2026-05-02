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

#include "impl/BLEGuards.h"
#if BLE_ENABLED

#include "BLEEddystone.h"
#include <cstdio>
#include <cstring>

static const BLEUUID kEddystoneUUID(static_cast<uint16_t>(0xFEAA));

// =========================================================================
// BLEEddystoneURL
// =========================================================================

static const char *kPrefixes[] = {"http://www.", "https://www.", "http://", "https://"};
static const char *kSuffixes[] = {".com/", ".org/", ".edu/", ".net/", ".info/", ".biz/", ".gov/", ".com", ".org", ".edu", ".net", ".info", ".biz", ".gov"};

/**
 * @brief Construct an Eddystone-URL frame with default TX power (-20 dBm).
 */
BLEEddystoneURL::BLEEddystoneURL() {}

/**
 * @brief Set the URL to compress into the Eddystone-URL frame.
 * @param url Full URL string.
 */
void BLEEddystoneURL::setURL(const String &url) {
  _url = url;
}

/**
 * @brief Get the currently configured URL.
 * @return The decoded URL string.
 */
String BLEEddystoneURL::getURL() const {
  return _url;
}

/**
 * @brief Set the calibrated TX power at 0 m (ranging reference).
 * @param txPower TX power in dBm.
 */
void BLEEddystoneURL::setTxPower(int8_t txPower) {
  _txPower = txPower;
}

/**
 * @brief Get the calibrated TX power at 0 m.
 * @return TX power in dBm.
 */
int8_t BLEEddystoneURL::getTxPower() const {
  return _txPower;
}

/**
 * @brief Eddystone service UUID (0xFEAA).
 * @return 16-bit UUID.
 */
BLEUUID BLEEddystoneURL::serviceUUID() {
  return kEddystoneUUID;
}

/**
 * @brief Encode a URL prefix into an Eddystone prefix code.
 * @param url The full URL string to match against known prefixes.
 * @param consumed Receives the number of characters consumed by the prefix match.
 * @return Prefix code (0-3), or 0 with consumed=0 if no known prefix matches.
 */
uint8_t BLEEddystoneURL::encodePrefix(const String &url, size_t &consumed) {
  for (uint8_t i = 0; i < 4; i++) {
    if (url.startsWith(kPrefixes[i])) {
      consumed = strlen(kPrefixes[i]);
      return i;
    }
  }
  consumed = 0;
  return 0;
}

/**
 * @brief Encode a URL suffix at the given position into an Eddystone suffix code.
 * @param url The full URL string.
 * @param pos Starting position in the URL to attempt suffix matching.
 * @param consumed Receives the number of characters consumed by the suffix match.
 * @return Suffix code (0-13), or 0xFF with consumed=0 if no known suffix matches.
 */
uint8_t BLEEddystoneURL::encodeSuffix(const String &url, size_t pos, size_t &consumed) {
  String remainder = url.substring(pos);
  for (uint8_t i = 0; i < 14; i++) {
    if (remainder.startsWith(kSuffixes[i])) {
      consumed = strlen(kSuffixes[i]);
      return i;
    }
  }
  consumed = 0;
  return 0xFF;
}

/**
 * @brief Decode an Eddystone prefix code back to its URL prefix string.
 * @param code Prefix code (0-3).
 * @return The URL prefix string, or an empty String if code is out of range.
 */
String BLEEddystoneURL::decodePrefix(uint8_t code) {
  return (code < 4) ? String(kPrefixes[code]) : "";
}

/**
 * @brief Decode an Eddystone suffix code back to its URL suffix string.
 * @param code Suffix code (0-13).
 * @return The URL suffix string, or an empty String if code is out of range.
 */
String BLEEddystoneURL::decodeSuffix(uint8_t code) {
  return (code < 14) ? String(kSuffixes[code]) : "";
}

/**
 * @brief Build a BLEAdvertisementData payload from the current Eddystone-URL fields.
 * @return Advertisement data containing an Eddystone service data field with
 *         frame type 0x10, TX power, encoded prefix, and compressed URL body.
 */
BLEAdvertisementData BLEEddystoneURL::getAdvertisementData() const {
  BLEAdvertisementData advData;
  std::vector<uint8_t> serviceData;

  serviceData.push_back(0x10);  // Eddystone URL frame type
  serviceData.push_back(static_cast<uint8_t>(_txPower));

  size_t consumed;
  uint8_t prefix = encodePrefix(_url, consumed);
  serviceData.push_back(prefix);

  size_t pos = consumed;
  while (pos < _url.length()) {
    size_t suffixConsumed;
    uint8_t suffix = encodeSuffix(_url, pos, suffixConsumed);
    if (suffix != 0xFF) {
      serviceData.push_back(suffix);
      pos += suffixConsumed;
    } else {
      serviceData.push_back(static_cast<uint8_t>(_url[pos]));
      pos++;
    }
  }

  advData.setServiceData(kEddystoneUUID, serviceData.data(), serviceData.size());
  return advData;
}

/**
 * @brief Parse Eddystone-URL fields from raw service data.
 * @param data Pointer to the service data bytes (byte 0 = frame type).
 * @param len Length of the service data in bytes.
 * @note Silently returns if len < 3 or frame type is not 0x10 (URL).
 *       Suffix codes < 14 are expanded; other bytes are treated as literal ASCII.
 */
void BLEEddystoneURL::setFromServiceData(const uint8_t *data, size_t len) {
  if (len < 3 || data[0] != 0x10) {
    return;
  }
  _txPower = static_cast<int8_t>(data[1]);
  _url = decodePrefix(data[2]);
  for (size_t i = 3; i < len; i++) {
    if (data[i] < 14) {
      _url += decodeSuffix(data[i]);
    } else {
      _url += static_cast<char>(data[i]);
    }
  }
}

// =========================================================================
// BLEEddystoneTLM
// =========================================================================

/**
 * @brief Construct an Eddystone-TLM frame with zeroed telemetry values.
 */
BLEEddystoneTLM::BLEEddystoneTLM() {}

/**
 * @brief Set the battery voltage field for TLM.
 * @param mv Battery voltage in millivolts.
 */
void BLEEddystoneTLM::setBatteryVoltage(uint16_t mv) {
  _voltage = mv;
}

/**
 * @brief Get the stored battery voltage.
 * @return Voltage in millivolts.
 */
uint16_t BLEEddystoneTLM::getBatteryVoltage() const {
  return _voltage;
}

/**
 * @brief Set the beacon temperature as an 8.8 fixed-point value.
 * @param celsius Temperature in degrees Celsius.
 * @note Internally stored as int16_t fixed-point (multiply by 256).
 */
void BLEEddystoneTLM::setTemperature(float celsius) {
  int16_t fixed = static_cast<int16_t>(celsius * 256.0f);
  _rawTemp = fixed;
}

/**
 * @brief Get the beacon temperature converted from 8.8 fixed-point.
 * @return Temperature in degrees Celsius.
 */
float BLEEddystoneTLM::getTemperature() const {
  return static_cast<float>(_rawTemp) / 256.0f;
}

/**
 * @brief Set the TLM advertising PDU counter.
 * @param count Frames sent since power-on.
 */
void BLEEddystoneTLM::setAdvertisingCount(uint32_t count) {
  _advCount = count;
}

/**
 * @brief Get the advertising PDU counter.
 * @return Frames sent since power-on.
 */
uint32_t BLEEddystoneTLM::getAdvertisingCount() const {
  return _advCount;
}

/**
 * @brief Set the beacon uptime field.
 * @param ds Uptime in deciseconds (0.1 s units).
 */
void BLEEddystoneTLM::setUptime(uint32_t ds) {
  _uptime = ds;
}

/**
 * @brief Get the uptime field.
 * @return Uptime in deciseconds.
 */
uint32_t BLEEddystoneTLM::getUptime() const {
  return _uptime;
}

/**
 * @brief Eddystone service UUID (0xFEAA).
 * @return 16-bit UUID.
 */
BLEUUID BLEEddystoneTLM::serviceUUID() {
  return kEddystoneUUID;
}

/**
 * @brief Build a BLEAdvertisementData payload from the current TLM fields.
 * @return Advertisement data containing a 14-byte Eddystone service data field
 *         with frame type 0x20, version 0x00, and big-endian voltage, temperature,
 *         advertising count, and uptime fields.
 */
BLEAdvertisementData BLEEddystoneTLM::getAdvertisementData() const {
  uint8_t serviceData[14];
  serviceData[0] = 0x20;  // TLM frame type
  serviceData[1] = 0x00;  // version
  serviceData[2] = (_voltage >> 8) & 0xFF;
  serviceData[3] = _voltage & 0xFF;
  serviceData[4] = (_rawTemp >> 8) & 0xFF;
  serviceData[5] = _rawTemp & 0xFF;
  serviceData[6] = (_advCount >> 24) & 0xFF;
  serviceData[7] = (_advCount >> 16) & 0xFF;
  serviceData[8] = (_advCount >> 8) & 0xFF;
  serviceData[9] = _advCount & 0xFF;
  serviceData[10] = (_uptime >> 24) & 0xFF;
  serviceData[11] = (_uptime >> 16) & 0xFF;
  serviceData[12] = (_uptime >> 8) & 0xFF;
  serviceData[13] = _uptime & 0xFF;

  BLEAdvertisementData advData;
  advData.setServiceData(kEddystoneUUID, serviceData, sizeof(serviceData));
  return advData;
}

/**
 * @brief Parse Eddystone-TLM fields from raw service data.
 * @param data Pointer to the service data bytes (byte 0 = frame type).
 * @param len Length of the service data in bytes.
 * @note Silently returns if len < 14 or frame type is not 0x20 (TLM).
 *       All multi-byte fields are decoded from big-endian wire format.
 */
void BLEEddystoneTLM::setFromServiceData(const uint8_t *data, size_t len) {
  if (len < 14 || data[0] != 0x20) {
    return;
  }
  _voltage = (data[2] << 8) | data[3];
  _rawTemp = static_cast<int16_t>((data[4] << 8) | data[5]);
  _advCount = (data[6] << 24) | (data[7] << 16) | (data[8] << 8) | data[9];
  _uptime = (data[10] << 24) | (data[11] << 16) | (data[12] << 8) | data[13];
}

/**
 * @brief Get a human-readable summary of the TLM frame.
 * @return Formatted string with voltage, temperature, advertising count, and uptime.
 */
String BLEEddystoneTLM::toString() const {
  char buf[128];
  snprintf(buf, sizeof(buf), "TLM: V=%dmV T=%.1fC Cnt=%lu Up=%lu", _voltage, getTemperature(), (unsigned long)_advCount, (unsigned long)_uptime);
  return String(buf);
}

#endif /* BLE_ENABLED */
