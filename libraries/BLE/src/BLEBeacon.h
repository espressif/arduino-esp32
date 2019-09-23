/*
 * BLEBeacon2.h
 *
 *  Created on: Jan 4, 2018
 *      Author: kolban
 */

#ifndef COMPONENTS_CPP_UTILS_BLEBEACON_H_
#define COMPONENTS_CPP_UTILS_BLEBEACON_H_
#include "BLEUUID.h"
/**
 * @brief Representation of a beacon.
 * See:
 * * https://en.wikipedia.org/wiki/IBeacon
 */
class BLEBeacon {
private:
	struct {
		uint16_t manufacturerId;
		uint8_t  subType;
		uint8_t  subTypeLength;
		uint8_t  proximityUUID[16];
		uint16_t major;
		uint16_t minor;
		int8_t   signalPower;
	} __attribute__((packed)) m_beaconData;
public:
	BLEBeacon();
	std::string getData();
	uint16_t    getMajor();
	uint16_t    getMinor();
	uint16_t    getManufacturerId();
	BLEUUID     getProximityUUID();
	int8_t      getSignalPower();
	void        setData(std::string data);
	void        setMajor(uint16_t major);
	void        setMinor(uint16_t minor);
	void        setManufacturerId(uint16_t manufacturerId);
	void        setProximityUUID(BLEUUID uuid);
	void        setSignalPower(int8_t signalPower);
}; // BLEBeacon

#endif /* COMPONENTS_CPP_UTILS_BLEBEACON_H_ */
