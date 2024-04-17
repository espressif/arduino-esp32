/*
 WiFi.h - esp32 Wifi support.
 Based on WiFi.h from Arduino WiFi shield library.
 Copyright (c) 2011-2014 Arduino.  All right reserved.
 Modified by Ivan Grokhotkov, December 2014

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include "soc/soc_caps.h"
#if SOC_WIFI_SUPPORTED

#include <stdint.h>

#include "Print.h"
#include "IPAddress.h"

#include "WiFiType.h"
#include "WiFiSTA.h"
#include "WiFiAP.h"
#include "WiFiScan.h"
#include "WiFiGeneric.h"

#include "WiFiClient.h"
#include "WiFiServer.h"
#include "WiFiUdp.h"

class WiFiClass : public WiFiGenericClass, public WiFiSTAClass, public WiFiScanClass, public WiFiAPClass {
private:
  bool prov_enable;
public:
  WiFiClass() {
    prov_enable = false;
  }

  using WiFiGenericClass::channel;

  using WiFiSTAClass::SSID;
  using WiFiSTAClass::RSSI;
  using WiFiSTAClass::BSSID;
  using WiFiSTAClass::BSSIDstr;

  using WiFiScanClass::SSID;
  using WiFiScanClass::encryptionType;
  using WiFiScanClass::RSSI;
  using WiFiScanClass::BSSID;
  using WiFiScanClass::BSSIDstr;
  using WiFiScanClass::channel;
public:
  void printDiag(Print& dest);
  friend class NetworkClient;
  friend class NetworkServer;
  friend class NetworkUDP;
  void enableProv(bool status);
  bool isProvEnabled();
};

extern WiFiClass WiFi;

#endif /* SOC_WIFI_SUPPORTED */
