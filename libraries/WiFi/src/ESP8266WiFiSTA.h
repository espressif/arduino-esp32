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

#ifndef ESP8266WIFISTA_H_
#define ESP8266WIFISTA_H_


#include "ESP8266WiFiType.h"
#include "ESP8266WiFiGeneric.h"
#include "user_interface.h"


class ESP8266WiFiSTAClass {
        // ----------------------------------------------------------------------------------------------
        // ---------------------------------------- STA function ----------------------------------------
        // ----------------------------------------------------------------------------------------------

    public:

        wl_status_t begin(const char* ssid, const char *passphrase = NULL, int32_t channel = 0, const uint8_t* bssid = NULL, bool connect = true);
        wl_status_t begin(char* ssid, char *passphrase = NULL, int32_t channel = 0, const uint8_t* bssid = NULL, bool connect = true);
        wl_status_t begin(const String& ssid, const String& passphrase = emptyString, int32_t channel = 0, const uint8_t* bssid = NULL, bool connect = true);
        wl_status_t begin();

        //The argument order for ESP is not the same as for Arduino. However, there is compatibility code under the hood
        //to detect Arduino arg order, and handle it correctly. Be aware that the Arduino default value handling doesn't
        //work here (see Arduino docs for gway/subnet defaults). In other words: at least 3 args must always be given.
        bool config(IPAddress local_ip, IPAddress gateway, IPAddress subnet, IPAddress dns1 = (uint32_t)0x00000000, IPAddress dns2 = (uint32_t)0x00000000);

        bool reconnect();
        bool disconnect(bool wifioff = false);

        bool isConnected();

        bool setAutoConnect(bool autoConnect);
        bool getAutoConnect();

        bool setAutoReconnect(bool autoReconnect);
        bool getAutoReconnect();

        int8_t waitForConnectResult(unsigned long timeoutLength = 60000);

        // STA network info
        IPAddress localIP();

        uint8_t * macAddress(uint8_t* mac);
        String macAddress();

        IPAddress subnetMask();
        IPAddress gatewayIP();
        IPAddress dnsIP(uint8_t dns_no = 0);

        String hostname();
        bool hostname(const String& aHostname) { return hostname(aHostname.c_str()); }
        bool hostname(const char* aHostname);

        // STA WiFi info
        wl_status_t status();
        String SSID() const;
        String psk() const;

        uint8_t * BSSID();
        String BSSIDstr();

        int32_t RSSI();

        static void enableInsecureWEP (bool enable = true) { _useInsecureWEP = enable; }

    protected:

        static bool _useStaticIp;
        static bool _useInsecureWEP;

    // ----------------------------------------------------------------------------------------------
    // ------------------------------------ STA remote configure  -----------------------------------
    // ----------------------------------------------------------------------------------------------

    public:

        bool beginWPSConfig(void);
        bool beginSmartConfig();
        bool stopSmartConfig();
        bool smartConfigDone();

    protected:

        static bool _smartConfigStarted;
        static bool _smartConfigDone;

        static void _smartConfigCallback(uint32_t status, void* result);

};


#endif /* ESP8266WIFISTA_H_ */
