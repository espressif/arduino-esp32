/**
 *
 * @file HTTPUpdate.cpp based om ESP8266HTTPUpdate.cpp
 * @date 16.10.2018
 * @author Markus Sattler
 *
 * Copyright (c) 2015 Markus Sattler. All rights reserved.
 * This file is part of the ESP32 Http Updater.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "HTTPUpdate.h"
#include <StreamString.h>
#if SOC_WIFI_SUPPORTED
#include "WiFi.h"
#endif

#include <esp_partition.h>
#include <esp_ota_ops.h>  // get running partition

// To do extern "C" uint32_t _SPIFFS_start;
// To do extern "C" uint32_t _SPIFFS_end;

HTTPUpdate::HTTPUpdate(void) : HTTPUpdate(8000) {}

HTTPUpdate::HTTPUpdate(int httpClientTimeout) : _httpClientTimeout(httpClientTimeout), _ledPin(-1) {
  _followRedirects = HTTPC_DISABLE_FOLLOW_REDIRECTS;
  _md5Sum = String();
  _user = String();
  _password = String();
  _auth = String();
}

HTTPUpdate::~HTTPUpdate(void) {}

HTTPUpdateResult HTTPUpdate::update(NetworkClient &client, const String &url, const String &currentVersion, HTTPUpdateRequestCB requestCB) {
  HTTPClient http;
  if (!http.begin(client, url)) {
    return HTTP_UPDATE_FAILED;
  }
  return handleUpdate(http, currentVersion, false, requestCB);
}

HTTPUpdateResult HTTPUpdate::updateSpiffs(HTTPClient &httpClient, const String &currentVersion, HTTPUpdateRequestCB requestCB) {
  return handleUpdate(httpClient, currentVersion, true, requestCB);
}

HTTPUpdateResult HTTPUpdate::updateSpiffs(NetworkClient &client, const String &url, const String &currentVersion, HTTPUpdateRequestCB requestCB) {
  HTTPClient http;
  if (!http.begin(client, url)) {
    return HTTP_UPDATE_FAILED;
  }
  return handleUpdate(http, currentVersion, true, requestCB);
}

HTTPUpdateResult HTTPUpdate::update(HTTPClient &httpClient, const String &currentVersion, HTTPUpdateRequestCB requestCB) {
  return handleUpdate(httpClient, currentVersion, false, requestCB);
}

HTTPUpdateResult
  HTTPUpdate::update(NetworkClient &client, const String &host, uint16_t port, const String &uri, const String &currentVersion, HTTPUpdateRequestCB requestCB) {
  HTTPClient http;
  if (!http.begin(client, host, port, uri)) {
    return HTTP_UPDATE_FAILED;
  }
  return handleUpdate(http, currentVersion, false, requestCB);
}

/**
 * return error code as int
 * @return int error code
 */
int HTTPUpdate::getLastError(void) {
  return _lastError;
}

/**
 * return error code as String
 * @return String error
 */
String HTTPUpdate::getLastErrorString(void) {

  if (_lastError == 0) {
    return String();  // no error
  }

  // error from Update class
  if (_lastError > 0) {
    StreamString error;
    Update.printError(error);
    error.trim();  // remove line ending
    return String("Update error: ") + error;
  }

  // error from http client
  if (_lastError > -100) {
    return String("HTTP error: ") + HTTPClient::errorToString(_lastError);
  }

  switch (_lastError) {
    case HTTP_UE_TOO_LESS_SPACE:           return "Not Enough space";
    case HTTP_UE_SERVER_NOT_REPORT_SIZE:   return "Server Did Not Report Size";
    case HTTP_UE_SERVER_FILE_NOT_FOUND:    return "File Not Found (404)";
    case HTTP_UE_SERVER_FORBIDDEN:         return "Forbidden (403)";
    case HTTP_UE_SERVER_WRONG_HTTP_CODE:   return "Wrong HTTP Code";
    case HTTP_UE_SERVER_FAULTY_MD5:        return "Wrong MD5";
    case HTTP_UE_BIN_VERIFY_HEADER_FAILED: return "Verify Bin Header Failed";
    case HTTP_UE_BIN_FOR_WRONG_FLASH:      return "New Binary Does Not Fit Flash Size";
    case HTTP_UE_NO_PARTITION:             return "Partition Could Not be Found";
  }

  return String();
}

String getSketchSHA256() {
  const size_t HASH_LEN = 32;  // SHA-256 digest length

  uint8_t sha_256[HASH_LEN] = {0};

  // get sha256 digest for running partition
  if (esp_partition_get_sha256(esp_ota_get_running_partition(), sha_256) == 0) {
    char buffer[2 * HASH_LEN + 1];

    for (size_t index = 0; index < HASH_LEN; index++) {
      uint8_t nibble = (sha_256[index] & 0xf0) >> 4;
      buffer[2 * index] = nibble < 10 ? char(nibble + '0') : char(nibble - 10 + 'A');

      nibble = sha_256[index] & 0x0f;
      buffer[2 * index + 1] = nibble < 10 ? char(nibble + '0') : char(nibble - 10 + 'A');
    }

    buffer[2 * HASH_LEN] = '\0';

    return String(buffer);
  } else {

    return String();
  }
}

/**
 *
 * @param http HTTPClient *
 * @param currentVersion const char *
 * @return HTTPUpdateResult
 */
HTTPUpdateResult HTTPUpdate::handleUpdate(HTTPClient &http, const String &currentVersion, bool spiffs, HTTPUpdateRequestCB requestCB) {

  HTTPUpdateResult ret = HTTP_UPDATE_FAILED;

  // use HTTP/1.0 for update since the update handler not support any transfer Encoding
  http.useHTTP10(true);
  http.setTimeout(_httpClientTimeout);
  http.setFollowRedirects(_followRedirects);
  http.setUserAgent("ESP32-http-Update");
  http.addHeader("Cache-Control", "no-cache");
  http.addHeader("x-ESP32-BASE-MAC", Network.macAddress());
#if SOC_WIFI_SUPPORTED
  http.addHeader("x-ESP32-STA-MAC", WiFi.macAddress());
  http.addHeader("x-ESP32-AP-MAC", WiFi.softAPmacAddress());
#endif
  http.addHeader("x-ESP32-free-space", String(ESP.getFreeSketchSpace()));
  http.addHeader("x-ESP32-sketch-size", String(ESP.getSketchSize()));
  String sketchMD5 = ESP.getSketchMD5();
  if (sketchMD5.length() != 0) {
    http.addHeader("x-ESP32-sketch-md5", sketchMD5);
  }
  // Add also a SHA256
  String sketchSHA256 = getSketchSHA256();
  if (sketchSHA256.length() != 0) {
    http.addHeader("x-ESP32-sketch-sha256", sketchSHA256);
  }
  http.addHeader("x-ESP32-chip-size", String(ESP.getFlashChipSize()));
  http.addHeader("x-ESP32-sdk-version", ESP.getSdkVersion());

  if (spiffs) {
    http.addHeader("x-ESP32-mode", "spiffs");
  } else {
    http.addHeader("x-ESP32-mode", "sketch");
  }

  if (currentVersion && currentVersion[0] != 0x00) {
    http.addHeader("x-ESP32-version", currentVersion);
  }
  if (requestCB) {
    requestCB(&http);
  }

  if (!_user.isEmpty() && !_password.isEmpty()) {
    http.setAuthorization(_user.c_str(), _password.c_str());
  }

  if (!_auth.isEmpty()) {
    http.setAuthorization(_auth.c_str());
  }

  const char *headerkeys[] = {"x-MD5"};
  size_t headerkeyssize = sizeof(headerkeys) / sizeof(char *);

  // track these headers
  http.collectHeaders(headerkeys, headerkeyssize);

  int code = http.GET();
  int len = http.getSize();

  if (code <= 0) {
    log_e("HTTP error: %s\n", http.errorToString(code).c_str());
    _lastError = code;
    http.end();
    return HTTP_UPDATE_FAILED;
  }

  log_d("Header read fin.\n");
  log_d("Server header:\n");
  log_d(" - code: %d\n", code);
  log_d(" - len: %d\n", len);

  String md5;
  if (_md5Sum.length()) {
    md5 = _md5Sum;
  } else if (http.hasHeader("x-MD5")) {
    md5 = http.header("x-MD5");
  }
  if (md5.length()) {
    log_d(" - MD5: %s\n", md5.c_str());
  }

  log_d("ESP32 info:\n");
  log_d(" - free Space: %d\n", ESP.getFreeSketchSpace());
  log_d(" - current Sketch Size: %d\n", ESP.getSketchSize());

  if (currentVersion && currentVersion[0] != 0x00) {
    log_d(" - current version: %s\n", currentVersion.c_str());
  }

  switch (code) {
    case HTTP_CODE_OK:  ///< OK (Start Update)
      if (len > 0) {
        bool startUpdate = true;
        if (spiffs) {
          const esp_partition_t *_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, NULL);
          if (!_partition) {
            _lastError = HTTP_UE_NO_PARTITION;
            return HTTP_UPDATE_FAILED;
          }

          if (len > _partition->size) {
            log_e("spiffsSize to low (%d) needed: %d\n", _partition->size, len);
            startUpdate = false;
          }
        } else {
          int sketchFreeSpace = ESP.getFreeSketchSpace();
          if (!sketchFreeSpace) {
            _lastError = HTTP_UE_NO_PARTITION;
            return HTTP_UPDATE_FAILED;
          }

          if (len > sketchFreeSpace) {
            log_e("FreeSketchSpace to low (%d) needed: %d\n", sketchFreeSpace, len);
            startUpdate = false;
          }
        }

        if (!startUpdate) {
          _lastError = HTTP_UE_TOO_LESS_SPACE;
          ret = HTTP_UPDATE_FAILED;
        } else {
          // Warn main app we're starting up...
          if (_cbStart) {
            _cbStart();
          }

          NetworkClient *tcp = http.getStreamPtr();

          // To do?                NetworkUDP::stopAll();
          // To do?                NetworkClient::stopAllExcept(tcp);

          delay(100);

          int command;

          if (spiffs) {
            command = U_SPIFFS;
            log_d("runUpdate spiffs...\n");
          } else {
            command = U_FLASH;
            log_d("runUpdate flash...\n");
          }

          if (!spiffs) {
            /* To do
                    uint8_t buf[4];
                    if(tcp->peekBytes(&buf[0], 4) != 4) {
                        log_e("peekBytes magic header failed\n");
                        _lastError = HTTP_UE_BIN_VERIFY_HEADER_FAILED;
                        http.end();
                        return HTTP_UPDATE_FAILED;
                    }
*/

            // check for valid first magic byte
            //                    if(buf[0] != 0xE9) {
            if (tcp->peek() != 0xE9) {
              log_e("Magic header does not start with 0xE9\n");
              _lastError = HTTP_UE_BIN_VERIFY_HEADER_FAILED;
              http.end();
              return HTTP_UPDATE_FAILED;
            }
            /* To do
                    uint32_t bin_flash_size = ESP.magicFlashChipSize((buf[3] & 0xf0) >> 4);

                    // check if new bin fits to SPI flash
                    if(bin_flash_size > ESP.getFlashChipRealSize()) {
                        log_e("New binary does not fit SPI Flash size\n");
                        _lastError = HTTP_UE_BIN_FOR_WRONG_FLASH;
                        http.end();
                        return HTTP_UPDATE_FAILED;
                    }
*/
          }
          if (runUpdate(*tcp, len, md5, command)) {
            ret = HTTP_UPDATE_OK;
            log_d("Update ok\n");
            http.end();
            // Warn main app we're all done
            if (_cbEnd) {
              _cbEnd();
            }

            if (_rebootOnUpdate && !spiffs) {
              ESP.restart();
            }

          } else {
            ret = HTTP_UPDATE_FAILED;
            log_e("Update failed\n");
          }
        }
      } else {
        _lastError = HTTP_UE_SERVER_NOT_REPORT_SIZE;
        ret = HTTP_UPDATE_FAILED;
        log_e("Content-Length was 0 or wasn't set by Server?!\n");
      }
      break;
    case HTTP_CODE_NOT_MODIFIED:
      ///< Not Modified (No updates)
      ret = HTTP_UPDATE_NO_UPDATES;
      break;
    case HTTP_CODE_NOT_FOUND:
      _lastError = HTTP_UE_SERVER_FILE_NOT_FOUND;
      ret = HTTP_UPDATE_FAILED;
      break;
    case HTTP_CODE_FORBIDDEN:
      _lastError = HTTP_UE_SERVER_FORBIDDEN;
      ret = HTTP_UPDATE_FAILED;
      break;
    default:
      _lastError = HTTP_UE_SERVER_WRONG_HTTP_CODE;
      ret = HTTP_UPDATE_FAILED;
      log_e("HTTP Code is (%d)\n", code);
      break;
  }

  http.end();
  return ret;
}

/**
 * write Update to flash
 * @param in Stream&
 * @param size uint32_t
 * @param md5 String
 * @return true if Update ok
 */
bool HTTPUpdate::runUpdate(Stream &in, uint32_t size, String md5, int command) {

  StreamString error;

  if (_cbProgress) {
    Update.onProgress(_cbProgress);
  }

  if (!Update.begin(size, command, _ledPin, _ledOn)) {
    _lastError = Update.getError();
    Update.printError(error);
    error.trim();  // remove line ending
    log_e("Update.begin failed! (%s)\n", error.c_str());
    return false;
  }

  if (_cbProgress) {
    _cbProgress(0, size);
  }

  if (md5.length()) {
    if (!Update.setMD5(md5.c_str())) {
      _lastError = HTTP_UE_SERVER_FAULTY_MD5;
      log_e("Update.setMD5 failed! (%s)\n", md5.c_str());
      return false;
    }
  }

  // To do: the SHA256 could be checked if the server sends it

  if (Update.writeStream(in) != size) {
    _lastError = Update.getError();
    Update.printError(error);
    error.trim();  // remove line ending
    log_e("Update.writeStream failed! (%s)\n", error.c_str());
    return false;
  }

  if (_cbProgress) {
    _cbProgress(size, size);
  }

  if (!Update.end()) {
    _lastError = Update.getError();
    Update.printError(error);
    error.trim();  // remove line ending
    log_e("Update.end failed! (%s)\n", error.c_str());
    return false;
  }

  return true;
}

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_HTTPUPDATE)
HTTPUpdate httpUpdate;
#endif
