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

    if(passphrase && strlen(passphrase) > 63) {
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

uint8_t WiFiMulti::run(uint32_t connectTimeout, bool scanHidden)
{
    int8_t scanResult;
    uint8_t status = WiFi.status();
    int8_t ci = 0;

    if(status == WL_CONNECTED) {
        for(uint32_t x = 0; x < APlist.size(); x++) {
            if(WiFi.SSID()==APlist[x].ssid) {
                return status;
            }
        }
	
	WiFi.persistent(false);
	WiFi.disconnect(false,false);
	WiFi.mode(WIFI_OFF);
	WiFi.mode(WIFI_STA);
        delay(200);
        status = WiFi.status();
    }

    scanResult = WiFi.scanNetworks(false, scanHidden);
    if(scanResult == WIFI_SCAN_RUNNING) {
        // scan is running
        return WL_NO_SSID_AVAIL;
    } else if(scanResult >= 0) {
        // scan done analyze

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

		    // SSID match or hidden SSID
                    if(ssid_scan == entry.ssid || (scanHidden && ssid_scan.length() == 0) ) { 
                        known = true;


			log_i("[WIFI] Candidate: %d  scan index: %d  APlist: %d  BSSID:[%02X:%02X:%02X:%02X:%02X:%02X] ssid_scan: %s  entry.ssid: %s\n",
			       ci, i, x,
			       BSSID_scan[0], BSSID_scan[1], BSSID_scan[2], BSSID_scan[3], BSSID_scan[4], BSSID_scan[5], 
			       ssid_scan, entry.ssid, rssi_scan);

			ci++;

			WiFi.begin(entry.ssid, entry.passphrase, chan_scan, BSSID_scan); 

			// If the ssid returned from the scan is empty, it is a hidden SSID
			// it appears that the WiFi.begin() function is asynchronous and takes
			// additional time to connect to a hidden SSID. Therefore a delay of 1000ms
			// is added for hidden SSIDs before calling WiFi.status()
			if( scanHidden && ssid_scan.length() == 0) {
			  log_i("[WIFI] Hidden SSID, adding a delay.");
			  delay(1000); 
			}

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
			// Connection was not successful
			// Disconnect and try the next AP
			if( status != WL_CONNECTED ) {
			  log_d("[WIFI] Disconnecting....\n");
			  WiFi.persistent(false);
			  WiFi.disconnect();
			  WiFi.mode(WIFI_OFF);
			  WiFi.mode(WIFI_STA);
			  delay(200);
			} else {
			  // connection was successful
			  // exit both loops w/return status
			  WiFi.scanDelete();
			  return status;
			}
		    }
		}
            }
        }
    }
    // clean up ram
    WiFi.scanDelete();

    return status;
}

