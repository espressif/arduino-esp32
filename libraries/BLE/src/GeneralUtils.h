/*
 * GeneralUtils.h
 *
 *  Created on: May 20, 2017
 *      Author: kolban
 */

#ifndef COMPONENTS_CPP_UTILS_GENERALUTILS_H_
#define COMPONENTS_CPP_UTILS_GENERALUTILS_H_
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
	static bool        base64Decode(const std::string& in, std::string* out);
	static bool        base64Encode(const std::string& in, std::string* out);
	static void        dumpInfo();
	static bool        endsWith(std::string str, char c);
	static const char* errorToString(esp_err_t errCode);
	static const char* wifiErrorToString(uint8_t value);
	static void        hexDump(const uint8_t* pData, uint32_t length);
	static std::string ipToString(uint8_t* ip);
	static std::vector<std::string> split(std::string source, char delimiter);
	static std::string toLower(std::string& value);
	static std::string trim(const std::string& str);

};

#endif /* COMPONENTS_CPP_UTILS_GENERALUTILS_H_ */
