/*
 * BLEEddystoneURL.cpp
 *
 *  Created on: Mar 12, 2018
 *      Author: pcbreflux
 *  Upgraded on: Feb 20, 2023
 *      By: Tomas Pilny
 */
#include "soc/soc_caps.h"
#if SOC_BLE_SUPPORTED

#include "sdkconfig.h"
#if defined(CONFIG_BLUEDROID_ENABLED)
#include <string.h>
#include "esp32-hal-log.h"
#include "BLEEddystoneURL.h"

String EDDYSTONE_URL_PREFIX[] = {
  "http://www.",   // 0x00
  "https://www.",  // 0x01
  "http://",       // 0x02
  "https://",      // 0x03
  ""               // Any other code number results in empty string
};

String EDDYSTONE_URL_SUFFIX[] = {
  ".com/",   // 0x00
  ".org/",   // 0x01
  ".edu/",   // 0x02
  ".net/",   // 0x03
  ".info/",  // 0x04
  ".biz/",   // 0x05
  ".gov/",   // 0x06
  ".com",    // 0x07
  ".org",    // 0x08
  ".edu",    // 0x09
  ".net",    // 0x0A
  ".info",   // 0x0B
  ".biz",    // 0x0C
  ".gov",    // 0x0D
  ""         // Any other code number results in empty string
};

BLEEddystoneURL::BLEEddystoneURL() {
  lengthURL = 0;
  m_eddystoneData.advertisedTxPower = 0;
  memset(m_eddystoneData.url, 0, sizeof(m_eddystoneData.url));
  _initHeadder();
}  // BLEEddystoneURL

BLEEddystoneURL::BLEEddystoneURL(BLEAdvertisedDevice *advertisedDevice) {
  const char *payload = (char *)advertisedDevice->getPayload();
  memset(m_eddystoneData.url, 0, sizeof(m_eddystoneData.url));
  lengthURL = 0;
  m_eddystoneData.advertisedTxPower = 0;
  for (int i = 0; i < advertisedDevice->getPayloadLength(); ++i) {
    if (payload[i] == 0x16 && advertisedDevice->getPayloadLength() >= i + 2 + sizeof(m_eddystoneData) && payload[i + 1] == 0xAA && payload[i + 2] == 0xFE
        && payload[i + 3] == 0x10) {
      lengthURL = payload[i - 1] - 5;  // Subtracting 5 Bytes containing header and other data which are not actual URL data
      m_eddystoneData.advertisedTxPower = payload[i + 1];
      if (lengthURL <= 18) {
        setData(String(payload + i + 4, lengthURL + 1));
      } else {
        log_e("Too long URL %d", lengthURL);
      }
    }
  }
  _initHeadder();
}

String BLEEddystoneURL::getData() {
  return String((char *)&m_eddystoneData, sizeof(m_eddystoneData));
}  // getData

String BLEEddystoneURL::getFrame() {
  BLEHeadder[7] = lengthURL + 5;  // Fill in real: Type + 2B UUID + Frame Type + Tx power + URL (note: the Byte holding the length does not count itself)
  String frame(BLEHeadder, sizeof(BLEHeadder));
  frame += String((char *)&m_eddystoneData, lengthURL + 1);  // + 1 for TX power

  return frame;
}  // getFrame

BLEUUID BLEEddystoneURL::getUUID() {
  uint16_t uuid = (((uint16_t)BLEHeadder[10]) << 8) | BLEHeadder[9];
  return BLEUUID(uuid);
}  // getUUID

int8_t BLEEddystoneURL::getPower() {
  return m_eddystoneData.advertisedTxPower;
}  // getPower

String BLEEddystoneURL::getURL() {
  return String((char *)&m_eddystoneData.url, lengthURL);
}  // getURL

String BLEEddystoneURL::getPrefix() {
  if (m_eddystoneData.url[0] <= 0x03) {
    return EDDYSTONE_URL_PREFIX[m_eddystoneData.url[0]];
  } else {
    return "";
  }
}

String BLEEddystoneURL::getSuffix() {
  if (m_eddystoneData.url[lengthURL - 1] <= 0x0D) {
    return EDDYSTONE_URL_SUFFIX[m_eddystoneData.url[lengthURL - 1]];
  } else {
    return "";
  }
}

String BLEEddystoneURL::getDecodedURL() {
  std::string decodedURL = "";
  decodedURL += getPrefix().c_str();
  if (decodedURL.length() == 0) {  // No prefix extracted - interpret byte [0] as character
    decodedURL += (char)m_eddystoneData.url[0];
  }
  for (int i = 1; i < lengthURL; i++) {
    if (m_eddystoneData.url[i] >= 33 && m_eddystoneData.url[i] < 127) {
      decodedURL += (char)m_eddystoneData.url[i];
    } else {
      if (i != lengthURL - 1 || m_eddystoneData.url[i] > 0x0D) {  // Ignore last Byte and values used for suffix
        log_e("Unexpected unprintable char in URL 0x%02X: m_eddystoneData.url[%d]", m_eddystoneData.url[i], i);
      }
    }
  }
  decodedURL += getSuffix().c_str();
  return String(decodedURL.c_str());
}  // getDecodedURL

/**
 * Set the raw data for the beacon record.
 * Example:
 * uint8_t *payload = advertisedDevice.getPayload();
 * eddystoneTLM.setData(String((char*)payload+11, advertisedDevice.getPayloadLength() - 11));
 * Note: the offset 11 works for current implementation of example BLE_EddystoneTLM Beacon.ino, however
 *   the position is not static and it is programmers responsibility to align the data.
 * Data frame:
 * | Field  || Len | Type | UUID        | EddyStone URL |
 * | Offset || 0   | 1    | 2           | 4             |
 * | Len    || 1 B | 1 B  | 2 B         | up to 20 B    |
 * | Data   || ??  | ??   | 0xAA | 0xFE | ???           |
 *
 * EddyStone TLM frame:
 * | Field  || Type  | TX Power | URL prefix | URL    |
 * | Offset || 0     | 1        | 2          | 3      |
 * | Len    || 1 B   | 1 B      | 1 B        | 0-17 B |
 * | Data   || 0x10  | ??       | ??         | ??     |
 */
void BLEEddystoneURL::setData(String data) {
  if (data.length() > sizeof(m_eddystoneData)) {
    log_e("Unable to set the data ... length passed in was %d and max expected %d", data.length(), sizeof(m_eddystoneData));
    return;
  }
  memset(&m_eddystoneData, 0, sizeof(m_eddystoneData));
  memcpy(&m_eddystoneData, data.c_str(), data.length());
  lengthURL = data.length() - (sizeof(m_eddystoneData) - sizeof(m_eddystoneData.url));
}  // setData

void BLEEddystoneURL::setUUID(BLEUUID l_uuid) {
  uint16_t beaconUUID = l_uuid.getNative()->uuid.uuid16;
  BLEHeadder[10] = beaconUUID >> 8;
  BLEHeadder[9] = beaconUUID & 0x00FF;
}  // setUUID

void BLEEddystoneURL::setPower(esp_power_level_t advertisedTxPower) {
  int tx_power;
  switch (advertisedTxPower) {
    case ESP_PWR_LVL_N12:  // 12dbm
      tx_power = -12;
      break;
    case ESP_PWR_LVL_N9:  // -9dbm
      tx_power = -9;
      break;
    case ESP_PWR_LVL_N6:  // -6dbm
      tx_power = -6;
      break;
    case ESP_PWR_LVL_N3:  // -3dbm
      tx_power = -3;
      break;
    case ESP_PWR_LVL_N0:  //  0dbm
      tx_power = 0;
      break;
    case ESP_PWR_LVL_P3:  // +3dbm
      tx_power = +3;
      break;
    case ESP_PWR_LVL_P6:  // +6dbm
      tx_power = +6;
      break;
    case ESP_PWR_LVL_P9:  // +9dbm
      tx_power = +9;
      break;
    default: tx_power = 0;
  }
  m_eddystoneData.advertisedTxPower = int8_t((tx_power - -100) / 2);
}  // setPower

void BLEEddystoneURL::setPower(int8_t advertisedTxPower) {
  m_eddystoneData.advertisedTxPower = advertisedTxPower;
}  // setPower

// Set URL bytes including prefix and optional suffix
// | Field   | Prefix  | URL + optional Suffix |
// | Offset  | 0       | 1                     |
// | Length  | 1 B     | 0 - 17 B              |
// | Example | 0x02    | 0x676F6F676C65 0x07   |
// | Decoded | http:// |   g o o g l e  .com   |
void BLEEddystoneURL::setURL(String url) {
  if (url.length() > sizeof(m_eddystoneData.url)) {
    log_e("Unable to set the url ... length passed in was %d and max expected %d", url.length(), sizeof(m_eddystoneData.url));
    return;
  }
  memset(m_eddystoneData.url, 0, sizeof(m_eddystoneData.url));
  memcpy(m_eddystoneData.url, url.c_str(), url.length());
  lengthURL = url.length();
}  // setURL

int BLEEddystoneURL::setSmartURL(String url) {
  if (url.length() == 0) {
    log_e("URL String has 0 length");
    return 0;  // ERROR
  }
  for (auto character : url) {
    if (!isPrintable(character)) {
      log_e("URL contains unprintable character(s)");
      return 0;  // ERROR
    }
  }
  bool hasPrefix = false;
  bool hasSuffix = false;
  m_eddystoneData.url[0] = 0x00;  // Init with default prefix "http://www."
  uint8_t suffix = 0x0E;          // Init with empty string
  log_d("Encode url \"%s\" with length %d", url.c_str(), url.length());
  for (uint8_t i = 0; i < 4; ++i) {
    if (url.substring(0, EDDYSTONE_URL_PREFIX[i].length()) == EDDYSTONE_URL_PREFIX[i]) {
      m_eddystoneData.url[0] = i;
      hasPrefix = true;
      break;
    }
  }

  if (hasPrefix == false) {
    log_w("Prefix not found - using default prefix \"http://www.\" = 0x00\n\tNote: URL must contain one of the prefixes: \"http://www.\", \"https://www.\", "
          "\"http://\", \"https://\"");
  }

  for (uint8_t i = 0; i < 0x0E; ++i) {
    std::string std_url(url.c_str());
    std::string std_suffix(EDDYSTONE_URL_SUFFIX[i].c_str());
    size_t found_pos = std_url.find(std_suffix);
    if (found_pos != std::string::npos) {
      hasSuffix = true;
      suffix = i;
      break;
    }
  }

  size_t baseUrlLen = url.length() - (hasPrefix ? EDDYSTONE_URL_PREFIX[m_eddystoneData.url[0]].length() : 0) - EDDYSTONE_URL_SUFFIX[suffix].length();
  lengthURL = baseUrlLen + 1 + (hasSuffix ? 1 : 0);
  if (lengthURL > 18) {
    log_e("Encoded URL is too long %d B - max 18 B", lengthURL);
    return 0;  // ERROR
  }
  String baseUrl = url.substring(
    (hasPrefix ? EDDYSTONE_URL_PREFIX[m_eddystoneData.url[0]].length() : 0),
    baseUrlLen + (hasPrefix ? EDDYSTONE_URL_PREFIX[m_eddystoneData.url[0]].length() : 0)
  );
  memcpy((void *)(m_eddystoneData.url + 1), (void *)baseUrl.c_str(), baseUrl.length());  // substr for Arduino String

  if (hasSuffix) {
    m_eddystoneData.url[1 + baseUrlLen] = suffix;
  }

  return 1;  // OK
}  // setSmartURL

void BLEEddystoneURL::_initHeadder() {
  BLEHeadder[0] = 0x02;   // Len
  BLEHeadder[1] = 0x01;   // Type Flags
  BLEHeadder[2] = 0x06;   // GENERAL_DISC_MODE 0x02 | BR_EDR_NOT_SUPPORTED 0x04
  BLEHeadder[3] = 0x03;   // Len
  BLEHeadder[4] = 0x03;   // Type 16-Bit UUID
  BLEHeadder[5] = 0xAA;   // Eddystone UUID 2 -> 0xFEAA LSB
  BLEHeadder[6] = 0xFE;   // Eddystone UUID 1 MSB
  BLEHeadder[7] = 0x00;   // Length of Beacon Data shall be calculated later
  BLEHeadder[8] = 0x16;   // Type Service Data
  BLEHeadder[9] = 0xAA;   // Eddystone UUID 2 -> 0xFEAA LSB
  BLEHeadder[10] = 0xFE;  // Eddystone UUID 1 MSB
  BLEHeadder[11] = 0x10;  // Eddystone Frame Type - URL
}

#endif
#endif /* SOC_BLE_SUPPORTED */
