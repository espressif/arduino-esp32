/*
 * BTAdvertisedDeviceSet.cpp
 *
 *  Created on: Feb 5, 2021
 *      Author: Thomas M. (ArcticSnowSky)
 */

#include "sdkconfig.h"
#if defined(CONFIG_BT_ENABLED) && defined(CONFIG_BLUEDROID_ENABLED)

//#include <map>

#include "BTAdvertisedDevice.h"
//#include "BTScan.h"


BTAdvertisedDeviceSet::BTAdvertisedDeviceSet() {
	m_cod		       = 0;
	m_name             = "";
	m_rssi             = 0;

	m_haveCOD	       = false;
	m_haveName         = false;
	m_haveRSSI         = false;
} // BTAdvertisedDeviceSet

BTAddress   BTAdvertisedDeviceSet::getAddress()    { return m_address; }
uint32_t    BTAdvertisedDeviceSet::getCOD() const       { return m_cod; }
std::string BTAdvertisedDeviceSet::getName() const      { return m_name; }
int8_t      BTAdvertisedDeviceSet::getRSSI() const	    { return m_rssi; }


bool        BTAdvertisedDeviceSet::haveCOD() const  { return m_haveCOD; }
bool        BTAdvertisedDeviceSet::haveName() const { return m_haveName; }
bool        BTAdvertisedDeviceSet::haveRSSI() const { return m_haveRSSI; }

/**
 * @brief Create a string representation of this device.
 * @return A string representation of this device.
 */
std::string BTAdvertisedDeviceSet::toString() {
	std::string res = "Name: " + getName() + ", Address: " + std::string(getAddress().toString().c_str(), getAddress().toString().length());
	if (haveCOD()) {
		char val[6];
		snprintf(val, sizeof(val), "%d", getCOD());
		res += ", cod: ";
		res += val;
	}
	if (haveRSSI()) {
		char val[6];
		snprintf(val, sizeof(val), "%d", (int8_t)getRSSI());
		res += ", rssi: ";
		res += val;
	}
	return res;
} // toString


void BTAdvertisedDeviceSet::setAddress(BTAddress address) {
    m_address = address;
}

void BTAdvertisedDeviceSet::setCOD(uint32_t cod) {
    m_cod     = cod;
	m_haveCOD = true;
}

void BTAdvertisedDeviceSet::setName(std::string name) {
    m_name     = name;
	m_haveName = true;
}

void BTAdvertisedDeviceSet::setRSSI(int8_t rssi) {
    m_rssi     = rssi;
	m_haveRSSI = true;
}

#endif /* CONFIG_BT_ENABLED */
