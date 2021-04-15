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
#include "sdkconfig.h"
#if defined(CONFIG_BLUEDROID_ENABLED)
#include <string.h>
#include <stdio.h>
#include "esp32-hal-log.h"
#include "BLEEddystoneTLM.h"

static const char LOG_TAG[] = "BLEEddystoneTLM";
#define ENDIAN_CHANGE_U16(x) ((((x)&0xFF00)>>8) + (((x)&0xFF)<<8))
#define ENDIAN_CHANGE_U32(x) ((((x)&0xFF000000)>>24) + (((x)&0x00FF0000)>>8)) + ((((x)&0xFF00)<<8) + (((x)&0xFF)<<24))

BLEEddystoneTLM::BLEEddystoneTLM() {
	beaconUUID = 0xFEAA;
	m_eddystoneData.frameType = EDDYSTONE_TLM_FRAME_TYPE;
	m_eddystoneData.version = 0;
	m_eddystoneData.volt = 3300; // 3300mV = 3.3V
	m_eddystoneData.temp = (uint16_t) ((float) 23.00)/256;
	m_eddystoneData.advCount = 0;
	m_eddystoneData.tmil = 0;
} // BLEEddystoneTLM

std::string BLEEddystoneTLM::getData() {
	return std::string((char*) &m_eddystoneData, sizeof(m_eddystoneData));
} // getData

BLEUUID BLEEddystoneTLM::getUUID() {
	return BLEUUID(beaconUUID);
} // getUUID

uint8_t BLEEddystoneTLM::getVersion() {
	return m_eddystoneData.version;
} // getVersion

uint16_t BLEEddystoneTLM::getVolt() {
	return ENDIAN_CHANGE_U16(m_eddystoneData.volt);
} // getVolt

float BLEEddystoneTLM::getTemp() {
	return ENDIAN_CHANGE_U16(m_eddystoneData.temp) / 256.0f;
} // getTemp

uint32_t BLEEddystoneTLM::getCount() {
	return ENDIAN_CHANGE_U32(m_eddystoneData.advCount);
} // getCount

uint32_t BLEEddystoneTLM::getTime() {
	return (ENDIAN_CHANGE_U32(m_eddystoneData.tmil)) / 10;
} // getTime

std::string BLEEddystoneTLM::toString() {
  std::string out = "";
  uint32_t rawsec = ENDIAN_CHANGE_U32(m_eddystoneData.tmil);
  char val[12];

  out += "Version "; // + std::string(m_eddystoneData.version);
  snprintf(val, sizeof(val), "%d", m_eddystoneData.version);
  out += val;
  out += "\n";
  out += "Battery Voltage "; // + ENDIAN_CHANGE_U16(m_eddystoneData.volt);
  snprintf(val, sizeof(val), "%d", ENDIAN_CHANGE_U16(m_eddystoneData.volt));
  out += val;
  out += " mV\n";

  out += "Temperature ";
  snprintf(val, sizeof(val), "%.2f", ENDIAN_CHANGE_U16(m_eddystoneData.temp) / 256.0f);
  out += val;
  out += " C\n";

  out += "Adv. Count ";
  snprintf(val, sizeof(val), "%d", ENDIAN_CHANGE_U32(m_eddystoneData.advCount));
  out += val;
  out += "\n";

  out += "Time in seconds ";
  snprintf(val, sizeof(val), "%d", rawsec/10);
  out += val;
  out += "\n";

  out += "Time ";

  snprintf(val, sizeof(val), "%04d", rawsec / 864000);
  out += val;
  out += ".";

  snprintf(val, sizeof(val), "%02d", (rawsec / 36000) % 24);
  out += val;
  out += ":";

  snprintf(val, sizeof(val), "%02d", (rawsec / 600) % 60);
  out += val;
  out += ":";

  snprintf(val, sizeof(val), "%02d", (rawsec / 10) % 60);
  out += val;
  out += "\n";

  return out;
} // toString

/**
 * Set the raw data for the beacon record.
 */
void BLEEddystoneTLM::setData(std::string data) {
	if (data.length() != sizeof(m_eddystoneData)) {
		log_e("Unable to set the data ... length passed in was %d and expected %d", data.length(), sizeof(m_eddystoneData));
		return;
	}
  memcpy(&m_eddystoneData, data.data(), data.length());
} // setData

void BLEEddystoneTLM::setUUID(BLEUUID l_uuid) {
	beaconUUID = l_uuid.getNative()->uuid.uuid16;
} // setUUID

void BLEEddystoneTLM::setVersion(uint8_t version) {
	m_eddystoneData.version = version;
} // setVersion

void BLEEddystoneTLM::setVolt(uint16_t volt) {
	m_eddystoneData.volt = volt;
} // setVolt

void BLEEddystoneTLM::setTemp(float temp) {
	m_eddystoneData.temp = (uint16_t)temp;
} // setTemp

void BLEEddystoneTLM::setCount(uint32_t advCount) {
	m_eddystoneData.advCount = advCount;
} // setCount

void BLEEddystoneTLM::setTime(uint32_t tmil) {
	m_eddystoneData.tmil = tmil;
} // setTime

#endif
