/*
 ESP8266WiFiSTA.h - esp8266 Wifi support.
 Based on WiFi.h from Ardiono WiFi shield library.
 Copyright (c) 2011-2014 Arduino.  All right reserved.
 Modified by Ivan Grokhotkov, December 2014
 Reworked by Markus Sattler, December 2015

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

#ifndef ESP32WIFISTA_H_
#define ESP32WIFISTA_H_


#include "WiFiType.h"
#include "WiFiGeneric.h"
#ifdef ESP_IDF_VERSION_MAJOR
#include "esp_event.h"
#endif


class WiFiSTAClass
{
    // ----------------------------------------------------------------------------------------------
    // ---------------------------------------- STA function ----------------------------------------
    // ----------------------------------------------------------------------------------------------

public:

    wl_status_t begin(const char* ssid, const char *passphrase = NULL, int32_t channel = 0, const uint8_t* bssid = NULL, bool connect = true);
    wl_status_t begin(char* ssid, char *passphrase = NULL, int32_t channel = 0, const uint8_t* bssid = NULL, bool connect = true);
    wl_status_t begin();

    bool config(IPAddress local_ip, IPAddress gateway, IPAddress subnet, IPAddress dns1 = (uint32_t)0x00000000, IPAddress dns2 = (uint32_t)0x00000000);

    bool reconnect();
    bool disconnect(bool wifioff = false, bool eraseap = false);

    bool isConnected();

    bool setAutoConnect(bool autoConnect);
    bool getAutoConnect();

    bool setAutoReconnect(bool autoReconnect);
    bool getAutoReconnect();

    uint8_t waitForConnectResult();

    // STA network info
    IPAddress localIP();

    uint8_t * macAddress(uint8_t* mac);
    String macAddress();

    IPAddress subnetMask();
    IPAddress gatewayIP();
    IPAddress dnsIP(uint8_t dns_no = 0);

    IPAddress broadcastIP();
    IPAddress networkID();
    uint8_t subnetCIDR();
    
    bool enableIpV6();
    IPv6Address localIPv6();

    // STA WiFi info
    static wl_status_t status();
    String SSID() const;
    String psk() const;

    uint8_t * BSSID();
    String BSSIDstr();

    int8_t RSSI();

    static void _setStatus(wl_status_t status);
protected:
    static bool _useStaticIp;
    static bool _autoReconnect;

public: 
    bool beginSmartConfig();
    bool stopSmartConfig();
    bool smartConfigDone();

    static bool _smartConfigDone;
protected:
    static bool _smartConfigStarted;

};


#endif /* ESP32WIFISTA_H_ */
