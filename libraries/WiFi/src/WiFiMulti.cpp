/**
 *
 * @file ESP8266WiFiMulti.cpp
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
    APlistClean();
}

bool WiFiMulti::addAP(const char* ssid, const char *passphrase)
{
    return APlistAdd(ssid, passphrase);
}

uint8_t WiFiMulti::run(void)
{

    int8_t scanResult;
    uint8_t status = WiFi.status();
    if(status != WL_CONNECTED || status == WL_NO_SSID_AVAIL || status == WL_IDLE_STATUS || status == WL_CONNECT_FAILED) {

        scanResult = WiFi.scanNetworks();
        if(scanResult == WIFI_SCAN_RUNNING) {
            // scan is running
            return WL_NO_SSID_AVAIL;
        } else if(scanResult > 0) {
            // scan done analyze
            WifiAPlist_t bestNetwork { NULL, NULL };
            int bestNetworkDb = INT_MIN;
            uint8_t bestBSSID[6];
            int32_t bestChannel = 0;

            DEBUG_WIFI_MULTI("[WIFI] scan done\n");
            delay(0);

            if(scanResult <= 0) {
                DEBUG_WIFI_MULTI("[WIFI] no networks found\n");
            } else {
                DEBUG_WIFI_MULTI("[WIFI] %d networks found\n", scanResult);
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
                        DEBUG_WIFI_MULTI(" ---> ");
                    } else {
                        DEBUG_WIFI_MULTI("      ");
                    }

                    DEBUG_WIFI_MULTI(" %d: [%d][%02X:%02X:%02X:%02X:%02X:%02X] %s (%d) %c\n", i, chan_scan, BSSID_scan[0], BSSID_scan[1], BSSID_scan[2], BSSID_scan[3], BSSID_scan[4], BSSID_scan[5], ssid_scan.c_str(), rssi_scan, (sec_scan == WIFI_AUTH_OPEN) ? ' ' : '*');
                    delay(0);
                }
            }

            // clean up ram
            WiFi.scanDelete();

            DEBUG_WIFI_MULTI("\n\n");
            delay(0);

            if(bestNetwork.ssid) {
                DEBUG_WIFI_MULTI("[WIFI] Connecting BSSID: %02X:%02X:%02X:%02X:%02X:%02X SSID: %s Channal: %d (%d)\n", bestBSSID[0], bestBSSID[1], bestBSSID[2], bestBSSID[3], bestBSSID[4], bestBSSID[5], bestNetwork.ssid, bestChannel, bestNetworkDb);

                WiFi.begin(bestNetwork.ssid, bestNetwork.passphrase, bestChannel, bestBSSID);
                status = WiFi.status();

                // wait for connection or fail
                while(status != WL_CONNECTED && status != WL_NO_SSID_AVAIL && status != WL_CONNECT_FAILED) {
                    delay(10);
                    status = WiFi.status();
                }

                IPAddress ip;
                uint8_t * mac;
                switch(status) {
                case 3:
                    ip = WiFi.localIP();
                    mac = WiFi.BSSID();
                    DEBUG_WIFI_MULTI("[WIFI] Connecting done.\n");
                    DEBUG_WIFI_MULTI("[WIFI] SSID: %s\n", WiFi.SSID());
                    DEBUG_WIFI_MULTI("[WIFI] IP: %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
                    DEBUG_WIFI_MULTI("[WIFI] MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
                    DEBUG_WIFI_MULTI("[WIFI] Channel: %d\n", WiFi.channel());
                    break;
                case 1:
                    DEBUG_WIFI_MULTI("[WIFI] Connecting Failed AP not found.\n");
                    break;
                case 4:
                    DEBUG_WIFI_MULTI("[WIFI] Connecting Failed.\n");
                    break;
                default:
                    DEBUG_WIFI_MULTI("[WIFI] Connecting Failed (%d).\n", status);
                    break;
                }
            } else {
                DEBUG_WIFI_MULTI("[WIFI] no matching wifi found!\n");
            }
        } else {
            // start scan
            DEBUG_WIFI_MULTI("[WIFI] delete old wifi config...\n");
            WiFi.disconnect();

            DEBUG_WIFI_MULTI("[WIFI] start scan\n");
            // scan wifi async mode
            WiFi.scanNetworks(true);
        }
    }
    return status;
}

// ##################################################################################

bool WiFiMulti::APlistAdd(const char* ssid, const char *passphrase)
{

    WifiAPlist_t newAP;

    if(!ssid || *ssid == 0x00 || strlen(ssid) > 31) {
        // fail SSID to long or missing!
        DEBUG_WIFI_MULTI("[WIFI][APlistAdd] no ssid or ssid to long\n");
        return false;
    }

    if(passphrase && strlen(passphrase) > 63) {
        // fail passphrase to long!
        DEBUG_WIFI_MULTI("[WIFI][APlistAdd] passphrase to long\n");
        return false;
    }

    newAP.ssid = strdup(ssid);

    if(!newAP.ssid) {
        DEBUG_WIFI_MULTI("[WIFI][APlistAdd] fail newAP.ssid == 0\n");
        return false;
    }

    if(passphrase && *passphrase != 0x00) {
        newAP.passphrase = strdup(passphrase);
        if(!newAP.passphrase) {
            DEBUG_WIFI_MULTI("[WIFI][APlistAdd] fail newAP.passphrase == 0\n");
            free(newAP.ssid);
            return false;
        }
    }

    APlist.push_back(newAP);
    DEBUG_WIFI_MULTI("[WIFI][APlistAdd] add SSID: %s\n", newAP.ssid);
    return true;
}

void WiFiMulti::APlistClean(void)
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

