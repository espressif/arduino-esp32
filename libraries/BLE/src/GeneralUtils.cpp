/*
 * GeneralUtils.cpp
 *
 *  Created on: May 20, 2017
 *      Author: kolban
 */

#include "GeneralUtils.h"
#include <esp_system.h>
#include <string.h>
#include <stdio.h>
#include <string>
#include <sstream>
#include <iomanip>
#include "RTOS.h"
#include <esp_err.h>
#include <nvs.h>
#include <esp_wifi.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include "esp32-hal-log.h"

static const char kBase64Alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz"
	"0123456789+/";

static int base64EncodedLength(size_t length) {
	return (length + 2 - ((length + 2) % 3)) / 3 * 4;
} // base64EncodedLength


static int base64EncodedLength(const std::string& in) {
	return base64EncodedLength(in.length());
} // base64EncodedLength


static void a3_to_a4(unsigned char* a4, unsigned char* a3) {
	a4[0] = (a3[0] & 0xfc) >> 2;
	a4[1] = ((a3[0] & 0x03) << 4) + ((a3[1] & 0xf0) >> 4);
	a4[2] = ((a3[1] & 0x0f) << 2) + ((a3[2] & 0xc0) >> 6);
	a4[3] = (a3[2] & 0x3f);
} // a3_to_a4


static void a4_to_a3(unsigned char* a3, unsigned char* a4) {
	a3[0] = (a4[0] << 2) + ((a4[1] & 0x30) >> 4);
	a3[1] = ((a4[1] & 0xf) << 4) + ((a4[2] & 0x3c) >> 2);
	a3[2] = ((a4[2] & 0x3) << 6) + a4[3];
} // a4_to_a3


/**
 * @brief Encode a string into base 64.
 * @param [in] in
 * @param [out] out
 */
bool GeneralUtils::base64Encode(const std::string& in, std::string* out) {
	int i = 0, j = 0;
	size_t enc_len = 0;
	unsigned char a3[3];
	unsigned char a4[4];

	out->resize(base64EncodedLength(in));

	int input_len = in.size();
	std::string::const_iterator input = in.begin();

	while (input_len--) {
		a3[i++] = *(input++);
		if (i == 3) {
			a3_to_a4(a4, a3);

			for (i = 0; i < 4; i++) {
				(*out)[enc_len++] = kBase64Alphabet[a4[i]];
			}

			i = 0;
		}
	}

	if (i) {
		for (j = i; j < 3; j++) {
			a3[j] = '\0';
		}

		a3_to_a4(a4, a3);

		for (j = 0; j < i + 1; j++) {
			(*out)[enc_len++] = kBase64Alphabet[a4[j]];
		}

		while ((i++ < 3)) {
			(*out)[enc_len++] = '=';
		}
	}

	return (enc_len == out->size());
} // base64Encode


/**
 * @brief Dump general info to the log.
 * Data includes:
 * * Amount of free RAM
 */
void GeneralUtils::dumpInfo() {
	esp_chip_info_t chipInfo;
	esp_chip_info(&chipInfo);
	log_v("--- dumpInfo ---");
	log_v("Free heap: %d", heap_caps_get_free_size(MALLOC_CAP_8BIT));
	log_v("Chip Info: Model: %d, cores: %d, revision: %d", chipInfo.model, chipInfo.cores, chipInfo.revision);
	log_v("ESP-IDF version: %s", esp_get_idf_version());
	log_v("---");
} // dumpInfo


/**
 * @brief Does the string end with a specific character?
 * @param [in] str The string to examine.
 * @param [in] c The character to look form.
 * @return True if the string ends with the given character.
 */
bool GeneralUtils::endsWith(std::string str, char c) {
	if (str.empty()) {
		return false;
	}
	if (str.at(str.length() - 1) == c) {
		return true;
	}
	return false;
} // endsWidth


static int DecodedLength(const std::string& in) {
	int numEq = 0;
	int n = (int) in.size();

	for (std::string::const_reverse_iterator it = in.rbegin(); *it == '='; ++it) {
		++numEq;
	}
	return ((6 * n) / 8) - numEq;
} // DecodedLength


static unsigned char b64_lookup(unsigned char c) {
	if(c >='A' && c <='Z') return c - 'A';
	if(c >='a' && c <='z') return c - 71;
	if(c >='0' && c <='9') return c + 4;
	if(c == '+') return 62;
	if(c == '/') return 63;
	return 255;
}; // b64_lookup


/**
 * @brief Decode a chunk of data that is base64 encoded.
 * @param [in] in The string to be decoded.
 * @param [out] out The resulting data.
 */
bool GeneralUtils::base64Decode(const std::string& in, std::string* out) {
	int i = 0, j = 0;
	size_t dec_len = 0;
	unsigned char a3[3];
	unsigned char a4[4];

	int input_len = in.size();
	std::string::const_iterator input = in.begin();

	out->resize(DecodedLength(in));

	while (input_len--) {
		if (*input == '=') {
			break;
		}

		a4[i++] = *(input++);
		if (i == 4) {
			for (i = 0; i <4; i++) {
				a4[i] = b64_lookup(a4[i]);
			}

			a4_to_a3(a3,a4);

			for (i = 0; i < 3; i++) {
				(*out)[dec_len++] = a3[i];
			}

			i = 0;
		}
	}

	if (i) {
		for (j = i; j < 4; j++) {
			a4[j] = '\0';
		}

		for (j = 0; j < 4; j++) {
			a4[j] = b64_lookup(a4[j]);
		}

		a4_to_a3(a3,a4);

		for (j = 0; j < i - 1; j++) {
			(*out)[dec_len++] = a3[j];
		}
	}

	return (dec_len == out->size());
 } // base64Decode

/*
void GeneralUtils::hexDump(uint8_t* pData, uint32_t length) {
	uint32_t index=0;
	std::stringstream ascii;
	std::stringstream hex;
	char asciiBuf[80];
	char hexBuf[80];
	hex.str("");
	ascii.str("");
	while(index < length) {
		hex << std::setfill('0') << std::setw(2) << std::hex << (int)pData[index] << ' ';
		if (std::isprint(pData[index])) {
			ascii << pData[index];
		} else {
			ascii << '.';
		}
		index++;
		if (index % 16 == 0) {
			strcpy(hexBuf, hex.str().c_str());
			strcpy(asciiBuf, ascii.str().c_str());
			log_v("%s %s", hexBuf, asciiBuf);
			hex.str("");
			ascii.str("");
		}
	}
	if (index %16 != 0) {
		while(index % 16 != 0) {
			hex << "   ";
			index++;
		}
		strcpy(hexBuf, hex.str().c_str());
		strcpy(asciiBuf, ascii.str().c_str());
		log_v("%s %s", hexBuf, asciiBuf);
		//log_v("%s %s", hex.str().c_str(), ascii.str().c_str());
	}
	FreeRTOS::sleep(1000);
}
*/

/*
void GeneralUtils::hexDump(uint8_t* pData, uint32_t length) {
	uint32_t index=0;
	static std::stringstream ascii;
	static std::stringstream hex;
	hex.str("");
	ascii.str("");
	while(index < length) {
		hex << std::setfill('0') << std::setw(2) << std::hex << (int)pData[index] << ' ';
		if (std::isprint(pData[index])) {
			ascii << pData[index];
		} else {
			ascii << '.';
		}
		index++;
		if (index % 16 == 0) {
			log_v("%s %s", hex.str().c_str(), ascii.str().c_str());
			hex.str("");
			ascii.str("");
		}
	}
	if (index %16 != 0) {
		while(index % 16 != 0) {
			hex << "   ";
			index++;
		}
		log_v("%s %s", hex.str().c_str(), ascii.str().c_str());
	}
	FreeRTOS::sleep(1000);
}
*/


/**
 * @brief Dump a representation of binary data to the console.
 *
 * @param [in] pData Pointer to the start of data to be logged.
 * @param [in] length Length of the data (in bytes) to be logged.
 * @return N/A.
 */
void GeneralUtils::hexDump(const uint8_t* pData, uint32_t length) {
	char ascii[80];
	char hex[80];
	char tempBuf[80];
	uint32_t lineNumber = 0;

	log_v("     00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f");
	log_v("     -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --");
	strcpy(ascii, "");
	strcpy(hex, "");
	uint32_t index = 0;
	while (index < length) {
		sprintf(tempBuf, "%.2x ", pData[index]);
		strcat(hex, tempBuf);
		if (isprint(pData[index])) {
			sprintf(tempBuf, "%c", pData[index]);
		} else {
			sprintf(tempBuf, ".");
		}
		strcat(ascii, tempBuf);
		index++;
		if (index % 16 == 0) {
			log_v("%.4x %s %s", lineNumber * 16, hex, ascii);
			strcpy(ascii, "");
			strcpy(hex, "");
			lineNumber++;
		}
	}
	if (index %16 != 0) {
		while (index % 16 != 0) {
			strcat(hex, "   ");
			index++;
		}
		log_v("%.4x %s %s", lineNumber * 16, hex, ascii);
	}
} // hexDump


/**
 * @brief Convert an IP address to string.
 * @param ip The 4 byte IP address.
 * @return A string representation of the IP address.
 */
std::string GeneralUtils::ipToString(uint8_t *ip) {
	auto size = 16;
	char *val = (char*)malloc(size);
	snprintf(val, size, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
	std::string res(val);
	free(val);
	return res;
} // ipToString


/**
 * @brief Split a string into parts based on a delimiter.
 * @param [in] source The source string to split.
 * @param [in] delimiter The delimiter characters.
 * @return A vector of strings that are the split of the input.
 */
std::vector<std::string> GeneralUtils::split(std::string source, char delimiter) {
	// See also: https://stackoverflow.com/questions/5167625/splitting-a-c-stdstring-using-tokens-e-g
	std::vector<std::string> strings;
	std::size_t current, previous = 0;
	current = source.find(delimiter);
	while (current != std::string::npos) {
		strings.push_back(trim(source.substr(previous, current - previous)));
		previous = current + 1;
		current = source.find(delimiter, previous);
	}
	strings.push_back(trim(source.substr(previous, current - previous)));
	return strings;
} // split


/**
 * @brief Convert an ESP error code to a string.
 * @param [in] errCode The errCode to be converted.
 * @return A string representation of the error code.
 */
const char* GeneralUtils::errorToString(esp_err_t errCode) {
	switch (errCode) {
#if CONFIG_LOG_DEFAULT_LEVEL > 4
		case ESP_OK:
			return "ESP_OK";
		case ESP_FAIL:
			return "ESP_FAIL";
		case ESP_ERR_NO_MEM:
			return "ESP_ERR_NO_MEM";
		case ESP_ERR_INVALID_ARG:
			return "ESP_ERR_INVALID_ARG";
		case ESP_ERR_INVALID_SIZE:
			return "ESP_ERR_INVALID_SIZE";
		case ESP_ERR_INVALID_STATE:
			return "ESP_ERR_INVALID_STATE";
		case ESP_ERR_NOT_FOUND:
			return "ESP_ERR_NOT_FOUND";
		case ESP_ERR_NOT_SUPPORTED:
			return "ESP_ERR_NOT_SUPPORTED";
		case ESP_ERR_TIMEOUT:
			return "ESP_ERR_TIMEOUT";
		case ESP_ERR_NVS_NOT_INITIALIZED:
			return "ESP_ERR_NVS_NOT_INITIALIZED";
		case ESP_ERR_NVS_NOT_FOUND:
			return "ESP_ERR_NVS_NOT_FOUND";
		case ESP_ERR_NVS_TYPE_MISMATCH:
			return "ESP_ERR_NVS_TYPE_MISMATCH";
		case ESP_ERR_NVS_READ_ONLY:
			return "ESP_ERR_NVS_READ_ONLY";
		case ESP_ERR_NVS_NOT_ENOUGH_SPACE:
			return "ESP_ERR_NVS_NOT_ENOUGH_SPACE";
		case ESP_ERR_NVS_INVALID_NAME:
			return "ESP_ERR_NVS_INVALID_NAME";
		case ESP_ERR_NVS_INVALID_HANDLE:
			return "ESP_ERR_NVS_INVALID_HANDLE";
		case ESP_ERR_NVS_REMOVE_FAILED:
			return "ESP_ERR_NVS_REMOVE_FAILED";
		case ESP_ERR_NVS_KEY_TOO_LONG:
			return "ESP_ERR_NVS_KEY_TOO_LONG";
		case ESP_ERR_NVS_PAGE_FULL:
			return "ESP_ERR_NVS_PAGE_FULL";
		case ESP_ERR_NVS_INVALID_STATE:
			return "ESP_ERR_NVS_INVALID_STATE";
		case ESP_ERR_NVS_INVALID_LENGTH:
			return "ESP_ERR_NVS_INVALID_LENGTH";
		case ESP_ERR_WIFI_NOT_INIT:
			return "ESP_ERR_WIFI_NOT_INIT";
		//case ESP_ERR_WIFI_NOT_START:
		//	return "ESP_ERR_WIFI_NOT_START";
		case ESP_ERR_WIFI_IF:
			return "ESP_ERR_WIFI_IF";
		case ESP_ERR_WIFI_MODE:
			return "ESP_ERR_WIFI_MODE";
		case ESP_ERR_WIFI_STATE:
			return "ESP_ERR_WIFI_STATE";
		case ESP_ERR_WIFI_CONN:
			return "ESP_ERR_WIFI_CONN";
		case ESP_ERR_WIFI_NVS:
			return "ESP_ERR_WIFI_NVS";
		case ESP_ERR_WIFI_MAC:
			return "ESP_ERR_WIFI_MAC";
		case ESP_ERR_WIFI_SSID:
			return "ESP_ERR_WIFI_SSID";
		case ESP_ERR_WIFI_PASSWORD:
			return "ESP_ERR_WIFI_PASSWORD";
		case ESP_ERR_WIFI_TIMEOUT:
			return "ESP_ERR_WIFI_TIMEOUT";
		case ESP_ERR_WIFI_WAKE_FAIL:
			return "ESP_ERR_WIFI_WAKE_FAIL";
#endif
		default:
			return "Unknown ESP_ERR error";
		}
} // errorToString

/**
 * @brief Convert a wifi_err_reason_t code to a string.
 * @param [in] errCode The errCode to be converted.
 * @return A string representation of the error code.
 *
 * @note: wifi_err_reason_t values as of April 2018 are: (1-24, 200-204) and are defined in ~/esp-idf/components/esp32/include/esp_wifi_types.h.
 */
const char* GeneralUtils::wifiErrorToString(uint8_t errCode) {
	if (errCode == ESP_OK) return "ESP_OK (received SYSTEM_EVENT_STA_GOT_IP event)";
	if (errCode == UINT8_MAX) return "Not Connected (default value)";

	switch ((wifi_err_reason_t) errCode) {
#if CONFIG_LOG_DEFAULT_LEVEL > 4
		case WIFI_REASON_UNSPECIFIED:
			return "WIFI_REASON_UNSPECIFIED";
		case WIFI_REASON_AUTH_EXPIRE:
			return "WIFI_REASON_AUTH_EXPIRE";
		case WIFI_REASON_AUTH_LEAVE:
			return "WIFI_REASON_AUTH_LEAVE";
		case WIFI_REASON_ASSOC_EXPIRE:
			return "WIFI_REASON_ASSOC_EXPIRE";
		case WIFI_REASON_ASSOC_TOOMANY:
			return "WIFI_REASON_ASSOC_TOOMANY";
		case WIFI_REASON_NOT_AUTHED:
			return "WIFI_REASON_NOT_AUTHED";
		case WIFI_REASON_NOT_ASSOCED:
			return "WIFI_REASON_NOT_ASSOCED";
		case WIFI_REASON_ASSOC_LEAVE:
			return "WIFI_REASON_ASSOC_LEAVE";
		case WIFI_REASON_ASSOC_NOT_AUTHED:
			return "WIFI_REASON_ASSOC_NOT_AUTHED";
		case WIFI_REASON_DISASSOC_PWRCAP_BAD:
			return "WIFI_REASON_DISASSOC_PWRCAP_BAD";
		case WIFI_REASON_DISASSOC_SUPCHAN_BAD:
			return "WIFI_REASON_DISASSOC_SUPCHAN_BAD";
		case WIFI_REASON_IE_INVALID:
			return "WIFI_REASON_IE_INVALID";
		case WIFI_REASON_MIC_FAILURE:
			return "WIFI_REASON_MIC_FAILURE";
		case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
			return "WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT";
		case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT:
			return "WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT";
		case WIFI_REASON_IE_IN_4WAY_DIFFERS:
			return "WIFI_REASON_IE_IN_4WAY_DIFFERS";
		case WIFI_REASON_GROUP_CIPHER_INVALID:
			return "WIFI_REASON_GROUP_CIPHER_INVALID";
		case WIFI_REASON_PAIRWISE_CIPHER_INVALID:
			return "WIFI_REASON_PAIRWISE_CIPHER_INVALID";
		case WIFI_REASON_AKMP_INVALID:
			return "WIFI_REASON_AKMP_INVALID";
		case WIFI_REASON_UNSUPP_RSN_IE_VERSION:
			return "WIFI_REASON_UNSUPP_RSN_IE_VERSION";
		case WIFI_REASON_INVALID_RSN_IE_CAP:
			return "WIFI_REASON_INVALID_RSN_IE_CAP";
		case WIFI_REASON_802_1X_AUTH_FAILED:
			return "WIFI_REASON_802_1X_AUTH_FAILED";
		case WIFI_REASON_CIPHER_SUITE_REJECTED:
			return "WIFI_REASON_CIPHER_SUITE_REJECTED";
		case WIFI_REASON_BEACON_TIMEOUT:
			return "WIFI_REASON_BEACON_TIMEOUT";
		case WIFI_REASON_NO_AP_FOUND:
			return "WIFI_REASON_NO_AP_FOUND";
		case WIFI_REASON_AUTH_FAIL:
			return "WIFI_REASON_AUTH_FAIL";
		case WIFI_REASON_ASSOC_FAIL:
			return "WIFI_REASON_ASSOC_FAIL";
		case WIFI_REASON_HANDSHAKE_TIMEOUT:
			return "WIFI_REASON_HANDSHAKE_TIMEOUT";
#endif
		default:
			return "Unknown ESP_ERR error";
	}
} // wifiErrorToString


/**
 * @brief Convert a string to lower case.
 * @param [in] value The string to convert to lower case.
 * @return A lower case representation of the string.
 */
std::string GeneralUtils::toLower(std::string& value) {
	// Question: Could this be improved with a signature of:
	// std::string& GeneralUtils::toLower(std::string& value)
	std::transform(value.begin(), value.end(), value.begin(), ::tolower);
	return value;
} // toLower


/**
 * @brief Remove white space from a string.
 */
std::string GeneralUtils::trim(const std::string& str) {
	size_t first = str.find_first_not_of(' ');
	if (std::string::npos == first) return str;
	size_t last = str.find_last_not_of(' ');
	return str.substr(first, (last - first + 1));
} // trim
