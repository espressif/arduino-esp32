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
  "http://www.",  // 0x00
  "https://www.", // 0x01
  "http://",      // 0x02
  "https://",     // 0x03
  ""              // Any other code number results in empty string
};

String EDDYSTONE_URL_SUFFIX[] = {
  ".com/",  // 0x00
  ".org/",  // 0x01
  ".edu/",  // 0x02
  ".net/",  // 0x03
  ".info/", // 0x04
  ".biz/",  // 0x05
  ".gov/",  // 0x06
  ".com",   // 0x07
  ".org",   // 0x08
  ".edu",   // 0x09
  ".net",   // 0x0A
  ".info",  // 0x0B
  ".biz",   // 0x0C
  ".gov",   // 0x0D
  ""        // Any other code number results in empty string
};

BLEEddystoneURL::BLEEddystoneURL() {
  lengthURL = 0;
  m_eddystoneData.advertisedTxPower = 0;
  memset(m_eddystoneData.url, 0, sizeof(m_eddystoneData.url));
  _initHeadder();
} // BLEEddystoneURL

BLEEddystoneURL::BLEEddystoneURL(BLEAdvertisedDevice *advertisedDevice){
  const uint8_t *payload = advertisedDevice->getPayload();
  memset(m_eddystoneData.url, 0, sizeof(m_eddystoneData.url));
  if(payload[11] != 0x10){ // Not Eddystone URL!
    log_e("Failed to interpret Advertised Device as Eddystone URL!");
    lengthURL = 0;
    m_eddystoneData.advertisedTxPower = 0;
  }else{
    lengthURL = payload[7] - 5; // Subtracting 5 Bytes containing header and other data which are not actual URL data
    log_d("lengthURL = %d <= payload[7](=%d) - 5", lengthURL, payload[7]);
    //setData(std::string((char*)payLoad+11, lengthURL));
    m_eddystoneData.advertisedTxPower = payload[12];
    log_d("using data from [13]=0x%02X (prefix) up to [%d]=0x%02X=%c (if value==0x00-0x0 => suffix)", payload[13], 13+lengthURL, payload[13+lengthURL], payload[13+lengthURL]);
    if(lengthURL <= 18){
      memcpy(m_eddystoneData.url, (void*)(payload+13), (size_t)lengthURL); // Subtracting from lengthURL 1 Byte containing prefix
    }else{
      log_e("too long URL %d", lengthURL);
    }
  }
  _initHeadder();
}

std::string BLEEddystoneURL::getData() {
  return std::string((char*) &m_eddystoneData, sizeof(m_eddystoneData));
} // getData

String BLEEddystoneURL::getFrame() {
  BLEHeadder[7] = lengthURL + 5; // Fill in real: Type + 2B UUID + Frame Type + Tx power + URL (note: the Byte holding the length does not count itself)
  //Serial.printf("BLEHeadder[7](%d) = sizeof(BLEHeadder)(%d) + lengthURL(%d)\n", BLEHeadder[7], sizeof(BLEHeadder), lengthURL);

  String frame(BLEHeadder, sizeof(BLEHeadder));
  /*
  Serial.printf("BLEHeadder:\n");
  for(int i = 0; i < frame.length(); ++i){
    Serial.printf("[%d] 0x%02X=%d='%c'\n",i , frame.c_str()[i],frame.c_str()[i],frame.c_str()[i]);
  }
  Serial.printf("m_eddystoneData.advertisedTxPower = 0x%02X=%d:\nm_eddystoneData.url:\n", m_eddystoneData.advertisedTxPower, m_eddystoneData.advertisedTxPower);
  for(int i = 0; i < lengthURL; ++i){
    Serial.printf("[%d] 0x%02X=%d='%c'\n",i , m_eddystoneData.url[i],m_eddystoneData.url[i],m_eddystoneData.url[i]);
  }
  */

  frame += String((char*) &m_eddystoneData, lengthURL+1); // + 1 for TX power
  /*
  Serial.println("Returning frame:");
  for(int i = 0; i < frame.length(); ++i){
    Serial.printf("[%d] 0x%02X=%d='%c'\n",i , frame.c_str()[i],frame.c_str()[i],frame.c_str()[i]);
  }
  */
  return frame;
} // getFrame

// TODO test his change !!!
BLEUUID BLEEddystoneURL::getUUID() {
  uint16_t uuid = (((uint16_t)BLEHeadder[10]) << 8) & BLEHeadder[9];
  return BLEUUID(uuid);
} // getUUID

int8_t BLEEddystoneURL::getPower() {
  return m_eddystoneData.advertisedTxPower;
} // getPower

std::string BLEEddystoneURL::getURL() {
  return std::string((char*) &m_eddystoneData.url, sizeof(m_eddystoneData.url));
} // getURL

String BLEEddystoneURL::getPrefix(){
  if(m_eddystoneData.url[0] <= 0x03){
    return EDDYSTONE_URL_PREFIX[m_eddystoneData.url[0]];
  }else{
    return "";
  }
}

String BLEEddystoneURL::getSuffix(){
  log_d("suffix = m_eddystoneData.url[%d] 0x%02X", lengthURL-1, m_eddystoneData.url[lengthURL-1]);
  if(m_eddystoneData.url[lengthURL-1] <= 0x0D){
    return EDDYSTONE_URL_SUFFIX[m_eddystoneData.url[lengthURL-1]];
  }else{
    return "";
  }
}

std::string BLEEddystoneURL::getDecodedURL() {
  std::string decodedURL = "";
  log_d("prefix = m_eddystoneData.url[0] 0x%02X",m_eddystoneData.url[0]);
  log_e("prefix type m_eddystoneData.url[0]=%d", m_eddystoneData.url[0]); // this is actually debug

  decodedURL += std::string(getPrefix().c_str());
  if(decodedURL.length() == 0){ // No prefix extracted - interpret byte [0] as character
    decodedURL += m_eddystoneData.url[0];
  }

  for (int i = 1; i < lengthURL; i++) {
    if (m_eddystoneData.url[i] >= 33 && m_eddystoneData.url[i] < 127) {
      decodedURL += m_eddystoneData.url[i];
    }else{
      log_e("Unexpected unprintable char in URL 0x%02X: m_eddystoneData.url[%d]", m_eddystoneData.url[i], i);
    }
  }
  decodedURL += std::string(getSuffix().c_str());
  return decodedURL;
} // getDecodedURL

/**
 * Set the raw data for the beacon record.
 */
void BLEEddystoneURL::setData(std::string data) {
  if (data.length() > sizeof(m_eddystoneData)) {
    log_e("Unable to set the data ... length passed in was %d and max expected %d", data.length(), sizeof(m_eddystoneData));
    return;
  }
  memset(&m_eddystoneData, 0, sizeof(m_eddystoneData));
  memcpy(&m_eddystoneData, data.data(), data.length());
  lengthURL = data.length() - (sizeof(m_eddystoneData) - sizeof(m_eddystoneData.url));
} // setData

// TODO test his change !!!
void BLEEddystoneURL::setUUID(BLEUUID l_uuid) {
  uint16_t beaconUUID = l_uuid.getNative()->uuid.uuid16;
  BLEHeadder[10] = beaconUUID >> 8;
  BLEHeadder[9] = beaconUUID & 0x00FF;
} // setUUID


void BLEEddystoneURL::setPower(esp_power_level_t advertisedTxPower) {
  int tx_power;
  switch(advertisedTxPower){
    case ESP_PWR_LVL_N12: // 12dbm
      tx_power = -12;
      break;
    case ESP_PWR_LVL_N9: // -9dbm
      tx_power =   -9;
      break;
    case ESP_PWR_LVL_N6: // -6dbm
      tx_power =   -6;
      break;
    case ESP_PWR_LVL_N3: // -3dbm
      tx_power =   -3;
      break;
    case ESP_PWR_LVL_N0: //  0dbm
      tx_power =    0;
      break;
    case ESP_PWR_LVL_P3: // +3dbm
      tx_power =   +3;
      break;
    case ESP_PWR_LVL_P6: // +6dbm
      tx_power =   +6;
      break;
    case ESP_PWR_LVL_P9: // +9dbm
      tx_power =   +9;
      break;
    default:  tx_power = 0;
  }
  m_eddystoneData.advertisedTxPower = int8_t((tx_power - -100) / 2);
} // setPower


void BLEEddystoneURL::setPower(int8_t advertisedTxPower) {
  m_eddystoneData.advertisedTxPower = advertisedTxPower;
} // setPower

void BLEEddystoneURL::setURL(std::string url) {
  if (url.length() > sizeof(m_eddystoneData.url)) {
  log_e("Unable to set the url ... length passed in was %d and max expected %d", url.length(), sizeof(m_eddystoneData.url));
  return;
  }
  memset(m_eddystoneData.url, 0, sizeof(m_eddystoneData.url));
  memcpy(m_eddystoneData.url, url.data(), url.length());
  lengthURL = url.length();
} // setURL

int BLEEddystoneURL::setSmartURL(String url) {
  bool hasPrefix = false;
  bool hasSuffix = false;
  m_eddystoneData.url[0] = 0x00; // Init with default prefix "http://www."
  uint8_t suffix = 0x0E; // Init with empty string
  log_d("encode url \"%s\" with length %d", url.c_str(), url.length());
  for(uint8_t i = 0; i < 4; ++i){
    //log_d("check if substr \"%s\" == prefix \"%s\" ", url.substring(0, EDDYSTONE_URL_PREFIX[i].length()), EDDYSTONE_URL_PREFIX[i]);
    if(url.substring(0, EDDYSTONE_URL_PREFIX[i].length()) == EDDYSTONE_URL_PREFIX[i]){
      //log_d("Found prefix 0x%02X = \"%s\"", i, EDDYSTONE_URL_PREFIX[i]);
      m_eddystoneData.url[0] = i;
      hasPrefix = true;
      break;
    }
  }

  if(hasPrefix == false){
    log_w("Prefix not found - using default prefix \"http://www.\" = 0x00\n\tNote: URL must contain one of the prefixes: \"http://www.\", \"https://www.\", \"http://\", \"https://\"");
  }

  for(uint8_t i = 0; i < 0x0E; ++i){
    std::string std_url(url.c_str());
    std::string std_suffix(EDDYSTONE_URL_SUFFIX[i].c_str());
    size_t found_pos = std_url.find(std_suffix);
    //log_d("check if in url \"%s\" can find suffix \"%s\": found_pos = %d", std_url.c_str(), std_suffix.c_str(), found_pos);
    if(found_pos != std::string::npos){
      log_d("Found suffix 0x%02X = \"%s\" at position %d", i, EDDYSTONE_URL_SUFFIX[i], found_pos);
      hasSuffix = true;
      suffix = i;
      break;
    }
  }

  size_t baseUrlLen = url.length() - (hasPrefix ? EDDYSTONE_URL_PREFIX[m_eddystoneData.url[0]].length() : 0) - EDDYSTONE_URL_SUFFIX[suffix].length();
  lengthURL = baseUrlLen + 1 + (hasSuffix ? 1 : 0);
  Serial.printf("Original URL has length %dB - prefix 1 Byte - suffix len (%d)B = base url len %dB\n=> complete url len with prefix and suffix =%d\n", url.length(), EDDYSTONE_URL_SUFFIX[suffix].length(), baseUrlLen, lengthURL); // debug
  if(lengthURL > 18){
    log_e("Encoded URL is too long %d B - max 18 B", lengthURL);
    return 0; // ERROR
  }
  String baseUrl = url.substring((hasPrefix ? EDDYSTONE_URL_PREFIX[m_eddystoneData.url[0]].length() : 0), baseUrlLen+(hasPrefix ? EDDYSTONE_URL_PREFIX[m_eddystoneData.url[0]].length() : 0));
  Serial.printf("Prefix #%02X = \"%s\", length =%d\n", m_eddystoneData.url[0], EDDYSTONE_URL_PREFIX[m_eddystoneData.url[0]].c_str(), EDDYSTONE_URL_PREFIX[m_eddystoneData.url[0]].length()); // debug
  Serial.printf("Base URL and length %d = : \"%s\"\n", baseUrlLen, baseUrl.c_str()); // debug
  memcpy((void*)(m_eddystoneData.url+1), (void*)baseUrl.c_str(), baseUrl.length()); // substr for Arduino String

  if(hasSuffix){
    Serial.printf("Put suffix on position m_eddystoneData.url[%d] value %d = : \"%s\"\n", 1+baseUrlLen, suffix, EDDYSTONE_URL_SUFFIX[suffix].c_str()); // debug
    m_eddystoneData.url[1+baseUrlLen] = suffix;
  }

  Serial.printf("[0] 0x%02X = \"%s\" prefix\n", m_eddystoneData.url[0], EDDYSTONE_URL_PREFIX[m_eddystoneData.url[0]].c_str());
  for(int i = 1; i < baseUrlLen+1; ++i){
    Serial.printf("[%d] 0x%02X = \'%c\'\n",i , m_eddystoneData.url[i], m_eddystoneData.url[i]);
  }
  if(hasSuffix){
    Serial.printf("[%d] 0x%02X = \"%s\" suffix\n", lengthURL-1, m_eddystoneData.url[lengthURL-1], EDDYSTONE_URL_SUFFIX[m_eddystoneData.url[lengthURL-1]].c_str());
  }


  return 1; // OK
} // setSmartURL

void BLEEddystoneURL::_initHeadder(){
  BLEHeadder[0]  = 0x02; // Len
  BLEHeadder[1]  = 0x01; // Type Flags
  BLEHeadder[2]  = 0x06; // GENERAL_DISC_MODE 0x02 | BR_EDR_NOT_SUPPORTED 0x04
  BLEHeadder[3]  = 0x03; // Len
  BLEHeadder[4]  = 0x03; // Type 16-Bit UUID
  BLEHeadder[5]  = 0xAA; // Eddystone UUID 2 -> 0xFEAA LSB
  BLEHeadder[6]  = 0xFE; // Eddystone UUID 1 MSB
  BLEHeadder[7]  = 0x00; // Length of Beacon Data shall be calculated later
  BLEHeadder[8]  = 0x16; // Type Service Data
  BLEHeadder[9]  = 0xAA; // Eddystone UUID 2 -> 0xFEAA LSB
  BLEHeadder[10] = 0xFE; // Eddystone UUID 1 MSB
  BLEHeadder[11] = 0x10; // Eddystone Frame Type
}

#endif
#endif /* SOC_BLE_SUPPORTED */
