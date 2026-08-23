/**
 *
 * @file HTTPUpdate.h based on ESP8266HTTPUpdate.h
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

#ifndef ___HTTP_UPDATE_H___
#define ___HTTP_UPDATE_H___

#include <Arduino.h>
#include <Network.h>
#include <HTTPClient.h>
#include <Update.h>

/// note we use HTTP client errors too so we start at 100
#define HTTP_UE_TOO_LESS_SPACE           (-100)
#define HTTP_UE_SERVER_NOT_REPORT_SIZE   (-101)
#define HTTP_UE_SERVER_FILE_NOT_FOUND    (-102)
#define HTTP_UE_SERVER_FORBIDDEN         (-103)
#define HTTP_UE_SERVER_WRONG_HTTP_CODE   (-104)
#define HTTP_UE_SERVER_FAULTY_MD5        (-105)
#define HTTP_UE_BIN_VERIFY_HEADER_FAILED (-106)
#define HTTP_UE_BIN_FOR_WRONG_FLASH      (-107)
#define HTTP_UE_NO_PARTITION             (-108)
#define HTTP_UE_SERVER_FAULTY_SHA256     (-109)

enum HTTPUpdateResult {
  HTTP_UPDATE_FAILED,
  HTTP_UPDATE_NO_UPDATES,
  HTTP_UPDATE_OK
};

typedef HTTPUpdateResult t_httpUpdate_return;  // backward compatibility

using HTTPUpdateStartCB = std::function<void()>;
using HTTPUpdateRequestCB = std::function<void(HTTPClient *)>;
using HTTPUpdateEndCB = std::function<void()>;
using HTTPUpdateErrorCB = std::function<void(int)>;
using HTTPUpdateProgressCB = std::function<void(int, int)>;

class HTTPUpdate {
public:
#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_UPDATE)
  HTTPUpdate(UpdateClass *updater = &Update) : HTTPUpdate(8000, updater){};
  HTTPUpdate(int httpClientTimeout, UpdateClass *updater = &Update);
#else
  HTTPUpdate(UpdateClass *updater = nullptr) : HTTPUpdate(8000, updater){};
  HTTPUpdate(int httpClientTimeout, UpdateClass *updater = nullptr);
#endif
  ~HTTPUpdate(void);

  void rebootOnUpdate(bool reboot) {
    _rebootOnUpdate = reboot;
  }

  /**
      * set redirect follow mode. See `followRedirects_t` enum for available modes.
      * @param follow
      */
  void setFollowRedirects(followRedirects_t follow) {
    _followRedirects = follow;
  }

  void setLedPin(int ledPin = -1, uint8_t ledOn = HIGH) {
    _ledPin = ledPin;
    _ledOn = ledOn;
  }

  void setMD5sum(const String &md5Sum) {
    _md5Sum = md5Sum;
  }

  void setSHA256sum(const String &sha256Sum) {
    _sha256Sum = sha256Sum;
  }

  /**
   * Optional URL of a small checksum sidecar (e.g. firmware.bin.md5).
   * Used only when setMD5sum() is empty; response header x-MD5 still wins.
   * Calling this pulls in the sidecar fetch code (flash cost only when used).
   */
  void setMD5sumUrl(const String &url);

  /**
   * Optional URL of a small checksum sidecar (e.g. firmware.bin.sha256).
   * Used only when setSHA256sum() is empty; response header x-SHA256 still wins.
   * Calling this pulls in the sidecar fetch code (flash cost only when used).
   */
  void setSHA256sumUrl(const String &url);

  void setAuthorization(const String &user, const String &password) {
    _user = user;
    _password = password;
  }

  void setAuthorization(const String &auth) {
    _auth = auth;
  }

  //Sets instance of UpdateClass to perform updating operations
  void setUpdaterInstance(UpdateClass *updater) {
    _updater = updater;
  };

  t_httpUpdate_return update(NetworkClient &client, const String &url, const String &currentVersion = "", HTTPUpdateRequestCB requestCB = NULL);

  t_httpUpdate_return update(
    NetworkClient &client, const String &host, uint16_t port, const String &uri = "/", const String &currentVersion = "", HTTPUpdateRequestCB requestCB = NULL
  );

  t_httpUpdate_return updateFs(NetworkClient &client, const String &url, const String &currentVersion = "", HTTPUpdateRequestCB requestCB = NULL);
  t_httpUpdate_return updateSpiffs(NetworkClient &client, const String &url, const String &currentVersion = "", HTTPUpdateRequestCB requestCB = NULL);
  t_httpUpdate_return updateFatfs(NetworkClient &client, const String &url, const String &currentVersion = "", HTTPUpdateRequestCB requestCB = NULL);
  t_httpUpdate_return updateLittlefs(NetworkClient &client, const String &url, const String &currentVersion = "", HTTPUpdateRequestCB requestCB = NULL);

  t_httpUpdate_return update(HTTPClient &httpClient, const String &currentVersion = "", HTTPUpdateRequestCB requestCB = NULL);

  t_httpUpdate_return updateFs(HTTPClient &httpClient, const String &currentVersion = "", HTTPUpdateRequestCB requestCB = NULL);
  t_httpUpdate_return updateSpiffs(HTTPClient &httpClient, const String &currentVersion = "", HTTPUpdateRequestCB requestCB = NULL);
  t_httpUpdate_return updateFatfs(HTTPClient &httpClient, const String &currentVersion = "", HTTPUpdateRequestCB requestCB = NULL);
  t_httpUpdate_return updateLittlefs(HTTPClient &httpClient, const String &currentVersion = "", HTTPUpdateRequestCB requestCB = NULL);

  // Notification callbacks
  void onStart(HTTPUpdateStartCB cbOnStart) {
    _cbStart = cbOnStart;
  }
  void onEnd(HTTPUpdateEndCB cbOnEnd) {
    _cbEnd = cbOnEnd;
  }
  void onError(HTTPUpdateErrorCB cbOnError) {
    _cbError = cbOnError;
  }
  void onProgress(HTTPUpdateProgressCB cbOnProgress) {
    _cbProgress = cbOnProgress;
  }

  int getLastError(void);
  String getLastErrorString(void);

protected:
  t_httpUpdate_return handleUpdate(HTTPClient &http, const String &currentVersion, uint8_t type = U_FLASH, HTTPUpdateRequestCB requestCB = NULL);
  bool runUpdate(Stream &in, uint32_t size, String md5, int command = U_FLASH, String sha256 = "");

  // Set the error and potentially use a CB to notify the application
  void _setLastError(int err) {
    _lastError = err;
    if (_cbError) {
      _cbError(err);
    }
  }
  int _lastError = 0;
  bool _rebootOnUpdate = true;

private:
  using ChecksumSidecarFetchFn = uint8_t (*)(
    NetworkClient *client, const String &md5Url, const String &sha256Url, uint8_t requested, String &md5, String &sha256, int timeout, followRedirects_t follow
  );

  static constexpr uint8_t SIDECAR_MD5_FAILED = 0x01;
  static constexpr uint8_t SIDECAR_SHA256_FAILED = 0x02;

  int _httpClientTimeout;
  UpdateClass *_updater;
  followRedirects_t _followRedirects;
  String _user;
  String _password;
  String _auth;
  String _md5Sum;
  String _sha256Sum;
  String _md5SumUrl;
  String _sha256SumUrl;
  ChecksumSidecarFetchFn _checksumSidecarFetch = nullptr;

  // Callbacks
  HTTPUpdateStartCB _cbStart;
  HTTPUpdateEndCB _cbEnd;
  HTTPUpdateErrorCB _cbError;
  HTTPUpdateProgressCB _cbProgress;

  int _ledPin{-1};
  uint8_t _ledOn = HIGH;
};

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_HTTPUPDATE)
extern HTTPUpdate httpUpdate;
#endif

#endif /* ___HTTP_UPDATE_H___ */
