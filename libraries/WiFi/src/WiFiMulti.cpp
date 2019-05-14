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
    APlistClean();
}

bool WiFiMulti::addAP(const char* ssid, const char *passphrase)
{
    return APlistAdd(ssid, passphrase);
}

uint8_t WiFiMulti::run(uint32_t connectTimeout)
{

    int8_t scanResult;
    uint8_t status = WiFi.status();
    if(status == WL_CONNECTED) {
        if (_bTestConnection && !_bWFMInit){
            if (testConnection()) {
                _bWFMInit = true;
                return status;
            }
        } else if (!_bStrict) {
            return status;
        } else {
            for(uint32_t x = 0; x < APlist.size(); x++) {
                if(WiFi.SSID()==APlist[x].ssid){
                    return status;
                }
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
    } else if (scanResult >= 0) {
        // scan done analyze
        int32_t bestIndex = 0;
        WifiAPlist_t bestNetwork { NULL, NULL, NULL };
        int bestNetworkDb = INT_MIN;
        int bestNetworkSec;
        uint8_t bestBSSID[6];
        int32_t bestChannel = 0;

        log_i("[WIFI] scan done");

        if(scanResult == 0) {
            log_e("[WIFI] no networks found");
        } else {
            log_i("[WIFI] %d networks found", scanResult);

            int8_t failCount = 0;
            int8_t foundCount = 0;
            for(int8_t i = 0; i < scanResult; ++i) {

                String ssid_scan;
                int32_t rssi_scan;
                uint8_t sec_scan;
                uint8_t* BSSID_scan;
                int32_t chan_scan;

                WiFi.getNetworkInfo(i, ssid_scan, sec_scan, rssi_scan, BSSID_scan, chan_scan);

                if (_bAllowOpenAP && sec_scan == WIFI_AUTH_OPEN){
                    bool found = false;
                    for(uint32_t o = 0; o < APlist.size(); o++) {
                        WifiAPlist_t check = APlist[o];
                        if (ssid_scan == check.ssid){
                            found = true;
                        }
                    }
                    if (!found){
                        log_i("[WIFI] added %s to APList", ssid_scan);
                        APlistAdd(ssid_scan.c_str());
                        // failCount++;
                        // foundCount++;
                    }
                }

                bool known = false;
                for(uint32_t x = 0; x < APlist.size(); x++) {
                    WifiAPlist_t entry = APlist[x];
                    
                    if(ssid_scan == entry.ssid) { // It's on the list
                    log_i("known ssid: %s, failed: %i", entry.ssid, entry.fail);
                        foundCount++;
                        if (!entry.fail){
                            known = true;
                            log_i("rssi_scan: %d, bestNetworkDb: %d", rssi_scan, bestNetworkDb);
                            if(rssi_scan > bestNetworkDb) { // best network
                                if(_bAllowOpenAP || (sec_scan == WIFI_AUTH_OPEN || entry.passphrase)) { // check for passphrase if not open wlan
                                    log_i("best network is now: %s", ssid_scan);
                                    bestIndex = x;
                                    bestNetworkSec = sec_scan;
                                    bestNetworkDb = rssi_scan;
                                    bestChannel = chan_scan;
                                    memcpy((void*) &bestNetwork, (void*) &entry, sizeof(bestNetwork));
                                    memcpy((void*) &bestBSSID, (void*) BSSID_scan, sizeof(bestBSSID));
                                }
                            }
                            break;
                        } else {
                            failCount++;
                        }
                    }
                }

                if(known) {
                    log_d(" --->   %d: [%d][%02X:%02X:%02X:%02X:%02X:%02X] %s (%d) %c", i, chan_scan, BSSID_scan[0], BSSID_scan[1], BSSID_scan[2], BSSID_scan[3], BSSID_scan[4], BSSID_scan[5], ssid_scan.c_str(), rssi_scan, (sec_scan == WIFI_AUTH_OPEN) ? ' ' : '*');
                } else {
                    log_d("       %d: [%d][%02X:%02X:%02X:%02X:%02X:%02X] %s (%d) %c", i, chan_scan, BSSID_scan[0], BSSID_scan[1], BSSID_scan[2], BSSID_scan[3], BSSID_scan[4], BSSID_scan[5], ssid_scan.c_str(), rssi_scan, (sec_scan == WIFI_AUTH_OPEN) ? ' ' : '*');
                }
            }
            log_i("foundCount = %d, failCount = %d", foundCount, failCount);
            
            if (foundCount == failCount){
                failCount = 0;
                resetFails();
            }
            foundCount = 0;
        }
        
        // clean up ram
        WiFi.scanDelete();

        if(bestNetwork.ssid) {
            log_i("[WIFI] Connecting BSSID: %02X:%02X:%02X:%02X:%02X:%02X SSID: %s Channel: %d (%d)", bestBSSID[0], bestBSSID[1], bestBSSID[2], bestBSSID[3], bestBSSID[4], bestBSSID[5], bestNetwork.ssid, bestChannel, bestNetworkDb);

            WiFi.begin(bestNetwork.ssid, (_bAllowOpenAP && bestNetworkSec == WIFI_AUTH_OPEN) ? NULL:bestNetwork.passphrase, bestChannel, bestBSSID);
            status = WiFi.status();
            _bWFMInit = true;
            
            auto startTime = millis();
            // wait for connection, fail, or timeout
            while(status != WL_CONNECTED && status != WL_NO_SSID_AVAIL && status != WL_CONNECT_FAILED && (millis() - startTime) <= connectTimeout) {
                delay(100);
                status = WiFi.status();
            }
            
            switch(status) {
            case 3:
                log_i("[WIFI] Connecting done.");
                log_d("[WIFI] SSID: %s", WiFi.SSID().c_str());
                log_d("[WIFI] IP: %s", WiFi.localIP().toString().c_str());
                log_d("[WIFI] MAC: %s", WiFi.BSSIDstr().c_str());
                log_d("[WIFI] Channel: %d", WiFi.channel());
                
                if (_bTestConnection){
                    // We connected to an AP but if it's a captive portal we're not going anywhere.  Test it.
                    if (testConnection()){
                        resetFails();
                    } else {
                        markAsFailed(bestIndex);
                        WiFi.disconnect(false,false);
                        delay(100);
                        status = WiFi.status();
                    }
                } else {
                    resetFails();
                }

                break;
            case 1:
                log_e("[WIFI] Connecting Failed AP not found.");
                markAsFailed(bestIndex);
                break;
            case 4:
                log_e("[WIFI] Connecting Failed.");
                markAsFailed(bestIndex);
                break;
            default:
                log_e("[WIFI] Connecting Failed (%d).", status);
                markAsFailed(bestIndex);
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

void WiFiMulti::markAsFailed(int32_t i) {
    APlist[i].fail = true;
    log_i("marked %s as failed",APlist[i].ssid);
}


void WiFiMulti::resetFails(){
    log_i("resetting failure flags");
    for(uint32_t i = 0; i < APlist.size(); i++) {
        APlist[i].fail = false;
    }
}

void WiFiMulti::setStrictMode(bool bStrict) {
    _bStrict = bStrict;
}

void WiFiMulti::setAllowOpenAP(bool bAllowOpenAP) {
    _bAllowOpenAP = bAllowOpenAP;
}

void WiFiMulti::setTestConnection(bool bTestConnection){
    _bTestConnection = bTestConnection;
}
void WiFiMulti::setTestPhrase(const char* testPhrase){
    _testPhrase = testPhrase;
}
void WiFiMulti::setTestURL(String testURL){
    _testURL = testURL;
}

bool WiFiMulti::testConnection(){
    //parse url
    int8_t split = _testURL.indexOf('/',7);
    String host = _testURL.substring(7, split);
    String url = _testURL.substring(split,_testURL.length());

    log_i("Testing connection to %s.  Test phrase is \"%s\"",_testURL.c_str(), _testPhrase.c_str());
    // Use WiFiClient class to create TCP connections
    WiFiClient client;
    const int httpPort = 80;
    if (!client.connect(host.c_str(), httpPort)) {
        log_e("Connection failed");
        return false;
    } else {
        log_i("Connected to test host");
    }

    // This will send the request to the server
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "Connection: close\r\n\r\n");
    unsigned long timeout = millis();
    while (client.available() == 0) {
        if (millis() - timeout > 5000) {
            log_e(">>>Client timeout!");
            client.stop();
            return false;
        }
    }
    bool bSuccess = false;
    // Read all the lines of the reply from server and print them to Serial
    while(client.available()) {
        // String line = client.readStringUntil('\r');
        // Serial.print(line);
        bSuccess = client.find(_testPhrase.c_str());
        if (bSuccess){
             log_i("Success. Found test phrase");
        } else {
            log_e("Failed.  Can't find test phrase");
        }
        return bSuccess;
    }

}

// ##################################################################################

bool WiFiMulti::APlistAdd(const char* ssid, const char *passphrase)
{

    WifiAPlist_t newAP;

    if(!ssid || *ssid == 0x00 || strlen(ssid) > 31) {
        // fail SSID to long or missing!
        log_e("[WIFI][APlistAdd] no ssid or ssid too long");
        return false;
    }

    if(passphrase && strlen(passphrase) > 63) {
        // fail passphrase to long!
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
        
    newAP.fail = false;

    APlist.push_back(newAP);
    log_i("[WIFI][APlistAdd] added SSID: %s", newAP.ssid);
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
        //why doesn't this work???
        //free(entry.fail);
    }
    APlist.clear();
}

