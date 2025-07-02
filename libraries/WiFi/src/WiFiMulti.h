/**
 *
 * @file ESP8266WiFiMulti.h
 * @date 16.05.2015
 * @author Markus Sattler
 *
 * Copyright (c) 2015 Markus Sattler. All rights reserved.
 * This file is part of the esp8266 core for Arduino environment.
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

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if SOC_WIFI_SUPPORTED || CONFIG_ESP_WIFI_REMOTE_ENABLED

#include "WiFi.h"
#include <vector>

typedef struct {
  char *ssid;
  char *passphrase;
  bool hasFailed;
} WifiAPlist_t;

typedef std::function<bool(void)> ConnectionTestCB_t;

class WiFiMulti {
public:
  WiFiMulti();
  ~WiFiMulti();

  bool addAP(const char *ssid, const char *passphrase = NULL);
  uint8_t run(uint32_t connectTimeout = 5000, bool scanHidden = false);
#if CONFIG_LWIP_IPV6
  void enableIPv6(bool state);
#endif

  // Force (default: true) to only keep connected or to connect to an AP from the provided WiFiMulti list.
  // When bStrict is false, it will keep the last/current connected AP even if not in the WiFiMulti List.
  void setStrictMode(bool bStrict = true);

  // allows (true) to connect to ANY open AP, even if not in the user list
  // default false (do not connect to an open AP that has not been explicitaly added by the user to list)
  void setAllowOpenAP(bool bAllowOpenAP = false);

  // clears the current list of Multi APs and frees the memory
  void APlistClean(void);

  // allow the user to define a callback function that will validate the connection to the Internet.
  // if the callback returns true, the connection is considered valid and the AP will added to the validated AP list.
  // set the callback to NULL to disable the feature and validate any SSID that is in the list.
  void setConnectionTestCallbackFunc(ConnectionTestCB_t cbFunc);

private:
  std::vector<WifiAPlist_t> APlist;
  bool ipv6_support;

  bool _bStrict = true;
  bool _bAllowOpenAP = false;
  ConnectionTestCB_t _connectionTestCBFunc = NULL;
  bool _bWFMInit = false;

  void markAsFailed(int32_t i);
  void resetFails();
};

#endif /* SOC_WIFI_SUPPORTED */
