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

#include "impl/common/BLEGuards.h"
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

BLEEddystoneURL::BLEEddystoneURL() {}

void BLEEddystoneURL::setURL(const String &url) {
  _url = url;
}

String BLEEddystoneURL::getURL() const {
  return _url;
}

void BLEEddystoneURL::setTxPower(int8_t txPower) {
  _txPower = txPower;
}

int8_t BLEEddystoneURL::getTxPower() const {
  return _txPower;
}

BLEUUID BLEEddystoneURL::serviceUUID() {
  return kEddystoneUUID;
}

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

String BLEEddystoneURL::decodePrefix(uint8_t code) {
  return (code < 4) ? String(kPrefixes[code]) : "";
}

String BLEEddystoneURL::decodeSuffix(uint8_t code) {
  return (code < 14) ? String(kSuffixes[code]) : "";
}

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

BLEEddystoneTLM::BLEEddystoneTLM() {}

void BLEEddystoneTLM::setBatteryVoltage(uint16_t mv) {
  _voltage = mv;
}

uint16_t BLEEddystoneTLM::getBatteryVoltage() const {
  return _voltage;
}

void BLEEddystoneTLM::setTemperature(float celsius) {
  // Stored as int16_t 8.8 fixed-point (× 256).
  int16_t fixed = static_cast<int16_t>(celsius * 256.0f);
  _rawTemp = fixed;
}

float BLEEddystoneTLM::getTemperature() const {
  return static_cast<float>(_rawTemp) / 256.0f;
}

void BLEEddystoneTLM::setAdvertisingCount(uint32_t count) {
  _advCount = count;
}

uint32_t BLEEddystoneTLM::getAdvertisingCount() const {
  return _advCount;
}

void BLEEddystoneTLM::setUptime(uint32_t ds) {
  _uptime = ds;
}

uint32_t BLEEddystoneTLM::getUptime() const {
  return _uptime;
}

BLEUUID BLEEddystoneTLM::serviceUUID() {
  return kEddystoneUUID;
}

BLEAdvertisementData BLEEddystoneTLM::getAdvertisementData() const {
  // 14-byte TLM service data: frame type 0x20, version 0x00, then big-endian
  // voltage, temperature, advertising count, and uptime.
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

void BLEEddystoneTLM::setFromServiceData(const uint8_t *data, size_t len) {
  if (len < 14 || data[0] != 0x20) {
    return;
  }
  _voltage = (data[2] << 8) | data[3];
  _rawTemp = static_cast<int16_t>((data[4] << 8) | data[5]);
  _advCount = (data[6] << 24) | (data[7] << 16) | (data[8] << 8) | data[9];
  _uptime = (data[10] << 24) | (data[11] << 16) | (data[12] << 8) | data[13];
}

String BLEEddystoneTLM::toString() const {
  char buf[128];
  snprintf(buf, sizeof(buf), "TLM: V=%dmV T=%.1fC Cnt=%lu Up=%lu", _voltage, getTemperature(), (unsigned long)_advCount, (unsigned long)_uptime);
  return String(buf);
}

#endif /* BLE_ENABLED */
