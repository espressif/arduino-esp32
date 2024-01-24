/*
 * BLEEddystoneTLM.cpp
 *
 *  Created on: Mar 12, 2018
 *      Author: pcbreflux
 *  Edited on: Mar 20, 2020 by beegee-tokyo
 *  Fix temperature value (8.8 fixed format)
 *  Fix time stamp (0.1 second resolution)
 *  Fixes based on EddystoneTLM frame specification https://github.com/google/eddystone/blob/master/eddystone-tlm/tlm-plain.md
 * 
 */
#include "soc/soc_caps.h"
#if SOC_BLE_SUPPORTED

#include "sdkconfig.h"
#if defined(CONFIG_BLUEDROID_ENABLED)
#include <string.h>
#include <stdio.h>
#include "esp32-hal-log.h"
#include "BLEEddystoneTLM.h"

static const char LOG_TAG[] = "BLEEddystoneTLM";

BLEEddystoneTLM::BLEEddystoneTLM() {
  m_eddystoneData.frameType = EDDYSTONE_TLM_FRAME_TYPE;
  m_eddystoneData.version = 0;
  m_eddystoneData.volt = 3300; // 3300mV = 3.3V
  m_eddystoneData.temp = (uint16_t) ((float) 23.00)/256;
  m_eddystoneData.advCount = 0;
  m_eddystoneData.tmil = 0;
} // BLEEddystoneTLM

BLEEddystoneTLM::BLEEddystoneTLM(BLEAdvertisedDevice *advertisedDevice){
  char* payload = (char*)advertisedDevice->getPayload();
  for(int i = 0; i < advertisedDevice->getPayloadLength(); ++i){
    if(payload[i] == 0x16 && advertisedDevice->getPayloadLength() >= i+2+sizeof(m_eddystoneData) && payload[i+1] == 0xAA && payload[i+2] == 0xFE && payload[i+3] == 0x20){
      log_d("Eddystone TLM data frame starting at byte [%d]", i+3);
      setData(String(payload+i+3, sizeof(m_eddystoneData)));
      break;
    }
  }
}

String BLEEddystoneTLM::getData() {
  return String((char*) &m_eddystoneData, sizeof(m_eddystoneData));
} // getData

BLEUUID BLEEddystoneTLM::getUUID() {
  return beaconUUID;
} // getUUID

uint8_t BLEEddystoneTLM::getVersion() {
  return m_eddystoneData.version;
} // getVersion

uint16_t BLEEddystoneTLM::getVolt() {
  return ENDIAN_CHANGE_U16(m_eddystoneData.volt);
} // getVolt

float BLEEddystoneTLM::getTemp() {
  return EDDYSTONE_TEMP_U16_TO_FLOAT(m_eddystoneData.temp);
} // getTemp

uint16_t BLEEddystoneTLM::getRawTemp() {
  return ENDIAN_CHANGE_U16(m_eddystoneData.temp);
} // getRawTemp

uint32_t BLEEddystoneTLM::getCount() {
  return ENDIAN_CHANGE_U32(m_eddystoneData.advCount);
} // getCount

uint32_t BLEEddystoneTLM::getTime() {
  return (ENDIAN_CHANGE_U32(m_eddystoneData.tmil)) / 10;
} // getTime

String BLEEddystoneTLM::toString() {
  String out = "";
  uint32_t rawsec = ENDIAN_CHANGE_U32(m_eddystoneData.tmil);
  char val[12];

  out += "Version " + String(m_eddystoneData.version);
  //snprintf(val, sizeof(val), "%d", m_eddystoneData.version);
  //out += val;
  out += "\n";
  out += "Battery Voltage "; // + ENDIAN_CHANGE_U16(m_eddystoneData.volt);
  snprintf(val, sizeof(val), "%d", ENDIAN_CHANGE_U16(m_eddystoneData.volt));
  out += val;
  out += " mV\n";

  out += "Temperature ";
  snprintf(val, sizeof(val), "%.2f", ((int16_t)ENDIAN_CHANGE_U16(m_eddystoneData.temp)) / 256.0f);
  out += val;
  out += " C\n";

  out += "Adv. Count ";
  snprintf(val, sizeof(val), "%ld", ENDIAN_CHANGE_U32(m_eddystoneData.advCount));
  out += val;
  out += "\n";

  out += "Time in seconds ";
  snprintf(val, sizeof(val), "%ld", rawsec/10);
  out += val;
  out += "\n";

  out += "Time ";

  snprintf(val, sizeof(val), "%04ld", rawsec / 864000);
  out += val;
  out += ".";

  snprintf(val, sizeof(val), "%02ld", (rawsec / 36000) % 24);
  out += val;
  out += ":";

  snprintf(val, sizeof(val), "%02ld", (rawsec / 600) % 60);
  out += val;
  out += ":";

  snprintf(val, sizeof(val), "%02ld", (rawsec / 10) % 60);
  out += val;
  out += "\n";

  return out;
} // toString

/**
 * Set the raw data for the beacon record.
 * Example:
 * uint8_t *payload = advertisedDevice.getPayload();
 * eddystoneTLM.setData(String((char*)payload+22, advertisedDevice.getPayloadLength() - 22));
 * Note: the offset 22 works for current implementation of example BLE_EddystoneTLM Beacon.ino, however
 *   the position is not static and it is programmers responsibility to align the data.
 * Data frame:
 * | Field  || Len | Type | UUID        | EddyStone TLM |
 * | Offset || 0   | 1    | 2           | 4             |
 * | Len    || 1 B | 1 B  | 2 B         | 14 B          |
 * | Data   || ??  | ??   | 0xAA | 0xFE | ???           |
 *
 * EddyStone TLM frame:
 * | Field  || Type  | Version | Batt mV     | Beacon temp | Cnt since boot | Time since boot |
 * | Offset || 0     | 1       | 2           | 4           | 6              | 10              |
 * | Len    || 1 B   | 1 B     | 2 B         | 2 B         | 4 B            | 4 B             |
 * | Data   || 0x20  | ??      | ??   | ??   | ??    | ??  |   |   |   |    |   |   |   |     |
 */
void BLEEddystoneTLM::setData(String data) {
  if (data.length() != sizeof(m_eddystoneData)) {
    log_e("Unable to set the data ... length passed in was %d and expected %d", data.length(), sizeof(m_eddystoneData));
    return;
  }
  memcpy(&m_eddystoneData, data.c_str(), data.length());
} // setData

void BLEEddystoneTLM::setUUID(BLEUUID l_uuid) {
  beaconUUID = l_uuid;
} // setUUID

void BLEEddystoneTLM::setVersion(uint8_t version) {
  m_eddystoneData.version = version;
} // setVersion

// Set voltage in ESP32 native Big endian and convert it to little endian used for BLE Frame
void BLEEddystoneTLM::setVolt(uint16_t volt) {
  m_eddystoneData.volt = ENDIAN_CHANGE_U16(volt);
} // setVolt

void BLEEddystoneTLM::setTemp(float temp) {
  m_eddystoneData.temp = EDDYSTONE_TEMP_FLOAT_TO_U16(temp);
} // setTemp

void BLEEddystoneTLM::setCount(uint32_t advCount) {
  m_eddystoneData.advCount = advCount;
} // setCount

void BLEEddystoneTLM::setTime(uint32_t tmil) {
  m_eddystoneData.tmil = tmil;
} // setTime

#endif /* CONFIG_BLUEDROID_ENABLED */
#endif /* SOC_BLE_SUPPORTED */
