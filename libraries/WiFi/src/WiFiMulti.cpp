/**
 *
 * @file WiFiMulti.cpp
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

#include "WiFiMulti.h"
#include <limits.h>
#include <string.h>
#include <esp32-hal.h>

WiFiMulti::WiFiMulti()
{
}

WiFiMulti::~WiFiMulti()
{
    for(uint32_t i = 0; i < APlist.size(); i++) {
        WifiAPlist_t entry = APlist[i];
        if(entry.ssid) {
            free(entry.ssid);
        }
        if(entry.passphrase) {
            free(entry.passphrase);
        }
    }
    APlist.clear();
}

bool WiFiMulti::addAP(const char* ssid, const char *passphrase)
{
    WifiAPlist_t newAP;

    if(!ssid || *ssid == 0x00 || strlen(ssid) > 31) {
        // fail SSID too long or missing!
        log_e("[WIFI][APlistAdd] no ssid or ssid too long");
        return false;
    }

    if(passphrase && strlen(passphrase) > 64) {
        // fail passphrase too long!
        log_e("[WIFI][APlistAdd] passphrase too long");
        return false;
    }

    newAP.ssid = strdup(ssid);

    if(!newAP.ssid) {
        log_e("[WIFI][APlistAdd] fail newAP.ssid == 0");
        return false;
    }

    if(passphrase && *passphrase != 0x00) {
        newAP.passphrase = strdup(passphrase);
        if(!newAP.passphrase) {
            log_e("[WIFI][APlistAdd] fail newAP.passphrase == 0");
            free(newAP.ssid);
            return false;
        }
    } else {
        newAP.passphrase = NULL;
    }

    APlist.push_back(newAP);
    log_i("[WIFI][APlistAdd] add SSID: %s", newAP.ssid);
    return true;
}

uint8_t WiFiMulti::run(uint32_t connectTimeout)
{
    int8_t scanResult;
    uint8_t status = WiFi.status();
    if(status == WL_CONNECTED) {
        for(uint32_t x = 0; x < APlist.size(); x++) {
            if(WiFi.SSID()==APlist[x].ssid) {
                return status;
            }
        }
        WiFi.disconnect(false,false);
        delay(10);
        status = WiFi.status();
    }

    scanResult = WiFi.scanNetworks();
    if(scanResult == WIFI_SCAN_RUNNING) {
        // scan is running
        return WL_NO_SSID_AVAIL;
    } else if(scanResult >= 0) {
        // scan done analyze
        WifiAPlist_t bestNetwork { NULL, NULL };
        int bestNetworkDb = INT_MIN;
        uint8_t bestBSSID[6];
        int32_t bestChannel = 0;

        log_i("[WIFI] scan done");

        if(scanResult == 0) {
            log_e("[WIFI] no networks found");
        } else {
            log_i("[WIFI] %d networks found", scanResult);
            for(int8_t i = 0; i < scanResult; ++i) {

                String ssid_scan;
                int32_t rssi_scan;
                uint8_t sec_scan;
                uint8_t* BSSID_scan;
                int32_t chan_scan;

                WiFi.getNetworkInfo(i, ssid_scan, sec_scan, rssi_scan, BSSID_scan, chan_scan);

                bool known = false;
                for(uint32_t x = 0; x < APlist.size(); x++) {
                    WifiAPlist_t entry = APlist[x];

                    if(ssid_scan == entry.ssid) { // SSID match
                        known = true;
                        if(rssi_scan > bestNetworkDb) { // best network
                            if(sec_scan == WIFI_AUTH_OPEN || entry.passphrase) { // check for passphrase if not open wlan
                                bestNetworkDb = rssi_scan;
                                bestChannel = chan_scan;
                                memcpy((void*) &bestNetwork, (void*) &entry, sizeof(bestNetwork));
                                memcpy((void*) &bestBSSID, (void*) BSSID_scan, sizeof(bestBSSID));
                            }
                        }
                        break;
                    }
                }

                if(known) {
                    log_d(" --->   %d: [%d][%02X:%02X:%02X:%02X:%02X:%02X] %s (%d) %c", i, chan_scan, BSSID_scan[0], BSSID_scan[1], BSSID_scan[2], BSSID_scan[3], BSSID_scan[4], BSSID_scan[5], ssid_scan.c_str(), rssi_scan, (sec_scan == WIFI_AUTH_OPEN) ? ' ' : '*');
                } else {
                    log_d("       %d: [%d][%02X:%02X:%02X:%02X:%02X:%02X] %s (%d) %c", i, chan_scan, BSSID_scan[0], BSSID_scan[1], BSSID_scan[2], BSSID_scan[3], BSSID_scan[4], BSSID_scan[5], ssid_scan.c_str(), rssi_scan, (sec_scan == WIFI_AUTH_OPEN) ? ' ' : '*');
                }
            }
        }

        // clean up ram
        WiFi.scanDelete();

        if(bestNetwork.ssid) {
            log_i("[WIFI] Connecting BSSID: %02X:%02X:%02X:%02X:%02X:%02X SSID: %s Channel: %d (%d)", bestBSSID[0], bestBSSID[1], bestBSSID[2], bestBSSID[3], bestBSSID[4], bestBSSID[5], bestNetwork.ssid, bestChannel, bestNetworkDb);

            WiFi.begin(bestNetwork.ssid, bestNetwork.passphrase, bestChannel, bestBSSID);
            status = WiFi.status();

            auto startTime = millis();
            // wait for connection, fail, or timeout
            while(status != WL_CONNECTED && status != WL_NO_SSID_AVAIL && status != WL_CONNECT_FAILED && (millis() - startTime) <= connectTimeout) {
                delay(10);
                status = WiFi.status();
            }

            switch(status) {
            case WL_CONNECTED:
                log_i("[WIFI] Connecting done.");
                log_d("[WIFI] SSID: %s", WiFi.SSID().c_str());
                log_d("[WIFI] IP: %s", WiFi.localIP().toString().c_str());
                log_d("[WIFI] MAC: %s", WiFi.BSSIDstr().c_str());
                log_d("[WIFI] Channel: %d", WiFi.channel());
                break;
            case WL_NO_SSID_AVAIL:
                log_e("[WIFI] Connecting Failed AP not found.");
                break;
            case WL_CONNECT_FAILED:
                log_e("[WIFI] Connecting Failed.");
                break;
            default:
                log_e("[WIFI] Connecting Failed (%d).", status);
                break;
            }
        } else {
            log_e("[WIFI] no matching wifi found!");
        }
    } else {
        // start scan
        log_d("[WIFI] delete old wifi config...");
        WiFi.disconnect();

        log_d("[WIFI] start scan");
        // scan wifi async mode
        WiFi.scanNetworks(true);
    }

    return status;
}
