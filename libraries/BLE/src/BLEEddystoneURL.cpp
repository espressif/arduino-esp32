/*
 * BLEEddystoneURL.cpp
 *
 *  Created on: Mar 12, 2018
 *      Author: pcbreflux
 */
#include "sdkconfig.h"
#if defined(CONFIG_BLUEDROID_ENABLED)
#include <string.h>
#include "esp32-hal-log.h"
#include "BLEEddystoneURL.h"

static const char LOG_TAG[] = "BLEEddystoneURL";

BLEEddystoneURL::BLEEddystoneURL() {
	beaconUUID = 0xFEAA;
	lengthURL = 0;
	m_eddystoneData.frameType = EDDYSTONE_URL_FRAME_TYPE;
	m_eddystoneData.advertisedTxPower = 0;
	memset(m_eddystoneData.url, 0, sizeof(m_eddystoneData.url));
} // BLEEddystoneURL


BLEEddystoneURL(BLEAdvertisedDevice *advertisedDevice){
	uint8_t *payLoad = advertisedDevice->getPayload();
	beaconUUID = 0xFEAA;
	m_eddystoneData.frameType = EDDYSTONE_URL_FRAME_TYPE;
	if(payload[11] != 0x10){ // Not Eddystone URL!
		log_e("Failed to interpret Advertised Device as Eddystone URL!");
		lengthURL = 0;
		m_eddystoneData.advertisedTxPower = 0;
		memset(m_eddystoneData.url, 0, sizeof(m_eddystoneData.url));
	}
		lengthURL = payload[7] - 6; // Subtracting 6 Bytes containing header and other data which are not actual URL data
		//setData(String((char*)payLoad+11, lengthURL));
		m_eddystoneData.advertisedTxPower = payload[12];
		log_d("using data from [14]=0x%02X=%c up to [%d]=0x%02X=%c", payload[14], payload[14], lengthURL, payload[14+lengthURL], payload[14+lengthURL]);
		memcpy(m_eddystoneData.url, payload[14], lengthURL);
}

String BLEEddystoneURL::getData() {
	return String((char*) &m_eddystoneData, sizeof(m_eddystoneData));
} // getData

BLEUUID BLEEddystoneURL::getUUID() {
	return BLEUUID(beaconUUID);
} // getUUID

int8_t BLEEddystoneURL::getPower() {
	return m_eddystoneData.advertisedTxPower;
} // getPower

String BLEEddystoneURL::getURL() {
	return String((char*) &m_eddystoneData.url, sizeof(m_eddystoneData.url));
} // getURL

String BLEEddystoneURL::getDecodedURL() {
	String decodedURL = "";
  log_d("prefix = m_eddystoneData.url[0] 0x%02X",m_eddystoneData.url[0]);
  log_e("prefix type m_eddystoneData.url[0]=%d", m_eddystoneData.url[0]); // this is actually debug
	switch (m_eddystoneData.url[0]) {
		case 0x00:
			decodedURL += "http://www.";
			break;
		case 0x01:
			decodedURL += "https://www.";
			break;
		case 0x02:
			decodedURL += "http://";
			break;
		case 0x03:
			decodedURL += "https://";
			break;
		default:
			decodedURL += m_eddystoneData.url[0];
	}

	for (int i = 1; i < lengthURL; i++) {
		if (m_eddystoneData.url[i] > 33 && m_eddystoneData.url[i] < 127) {
			decodedURL += m_eddystoneData.url[i];
		} else {
  		log_d("suffix = m_eddystoneData.url[%d] 0x%02X", i, m_eddystoneData.url[i]);
			switch (m_eddystoneData.url[i]) {
				case 0x00:
					decodedURL += ".com/";
					break;
				case 0x01:
					decodedURL += ".org/";
					break;
				case 0x02:
					decodedURL += ".edu/";
					break;
				case 0x03:
					decodedURL += ".net/";
					break;
				case 0x04:
					decodedURL += ".info/";
					break;
				case 0x05:
					decodedURL += ".biz/";
					break;
				case 0x06:
					decodedURL += ".gov/";
					break;
				case 0x07:
					decodedURL += ".com";
					break;
				case 0x08:
					decodedURL += ".org";
					break;
				case 0x09:
					decodedURL += ".edu";
					break;
				case 0x0A:
					decodedURL += ".net";
					break;
				case 0x0B:
					decodedURL += ".info";
					break;
				case 0x0C:
					decodedURL += ".biz";
					break;
				case 0x0D:
					decodedURL += ".gov";
					break;
				default:
					break;
			}
		}
	}
	return decodedURL;
} // getDecodedURL



/**
 * Set the raw data for the beacon record.
 */
void BLEEddystoneURL::setData(String data) {
	if (data.length() > sizeof(m_eddystoneData)) {
		log_e("Unable to set the data ... length passed in was %d and max expected %d", data.length(), sizeof(m_eddystoneData));
		return;
	}
	memset(&m_eddystoneData, 0, sizeof(m_eddystoneData));
	memcpy(&m_eddystoneData, data.data(), data.length());
	lengthURL = data.length() - (sizeof(m_eddystoneData) - sizeof(m_eddystoneData.url));
} // setData

void BLEEddystoneURL::setUUID(BLEUUID l_uuid) {
	beaconUUID = l_uuid.getNative()->uuid.uuid16;
} // setUUID

void BLEEddystoneURL::setPower(int8_t advertisedTxPower) {
	m_eddystoneData.advertisedTxPower = advertisedTxPower;
} // setPower

void BLEEddystoneURL::setURL(String url) {
  if (url.length() > sizeof(m_eddystoneData.url)) {
	log_e("Unable to set the url ... length passed in was %d and max expected %d", url.length(), sizeof(m_eddystoneData.url));
	return;
  }
  memset(m_eddystoneData.url, 0, sizeof(m_eddystoneData.url));
  memcpy(m_eddystoneData.url, url.data(), url.length());
  lengthURL = url.length();
} // setURL


#endif
