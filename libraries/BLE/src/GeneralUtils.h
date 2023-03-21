/*
 * GeneralUtils.h
 *
 *  Created on: May 20, 2017
 *      Author: kolban
 */

#ifndef COMPONENTS_CPP_UTILS_GENERALUTILS_H_
#define COMPONENTS_CPP_UTILS_GENERALUTILS_H_
#include "Arduino.h"
#include <stdint.h>
#include <string>
#include <esp_err.h>
#include <algorithm>
#include <vector>

/**
 * @brief General utilities.
 */
class GeneralUtils {
public:
	static bool        base64Decode(const String& in, String* out);
	static bool        base64Encode(const String& in, String* out);
	static void        dumpInfo();
	static bool        endsWith(String str, char c);
	static const char* errorToString(esp_err_t errCode);
	static const char* wifiErrorToString(uint8_t value);
	static void        hexDump(const uint8_t* pData, uint32_t length);
	static String ipToString(uint8_t* ip);
	static std::vector<String> split(String source, char delimiter);
	static String toLower(String& value);
	static String trim(const String& str);

};

#endif /* COMPONENTS_CPP_UTILS_GENERALUTILS_H_ */
