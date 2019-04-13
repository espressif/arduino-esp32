/*
 * BLEUUID.h
 *
 *  Created on: Jun 21, 2017
 *      Author: kolban
 */

#ifndef COMPONENTS_CPP_UTILS_BLEUUID_H_
#define COMPONENTS_CPP_UTILS_BLEUUID_H_
#include "sdkconfig.h"
#if defined(CONFIG_BT_ENABLED)
#include <esp_gatt_defs.h>
#include <string>

/**
 * @brief A model of a %BLE UUID.
 */
class BLEUUID {
public:
	BLEUUID(std::string uuid);
	BLEUUID(uint16_t uuid);
	BLEUUID(uint32_t uuid);
	BLEUUID(esp_bt_uuid_t uuid);
	BLEUUID(uint8_t* pData, size_t size, bool msbFirst);
	BLEUUID(esp_gatt_id_t gattId);
	BLEUUID();
	uint8_t        bitSize();   // Get the number of bits in this uuid.
	bool           equals(BLEUUID uuid);
	esp_bt_uuid_t* getNative();
	BLEUUID        to128();
	std::string    toString();
	static BLEUUID fromString(std::string uuid);  // Create a BLEUUID from a string

private:
	esp_bt_uuid_t m_uuid;       		// The underlying UUID structure that this class wraps.
	bool          m_valueSet = false;   // Is there a value set for this instance.
}; // BLEUUID
#endif /* CONFIG_BT_ENABLED */
#endif /* COMPONENTS_CPP_UTILS_BLEUUID_H_ */
