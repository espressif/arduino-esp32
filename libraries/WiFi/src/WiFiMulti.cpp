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
#if SOC_WIFI_SUPPORTED
#include <limits.h>
#include <string.h>
#include <esp32-hal.h>

WiFiMulti::WiFiMulti() {
  ipv6_support = false;
}

void WiFiMulti::APlistClean(void) {
  for (auto entry : APlist) {
    if (entry.ssid) {
      free(entry.ssid);
    }
    if (entry.passphrase) {
      free(entry.passphrase);
    }
  }
  APlist.clear();
}

WiFiMulti::~WiFiMulti() {
  APlistClean();
}

bool WiFiMulti::addAP(const char* ssid, const char* passphrase) {
  WifiAPlist_t newAP;

  if (!ssid || *ssid == '\0' || strlen(ssid) > 31) {
    // fail SSID too long or missing!
    log_e("[WIFI][APlistAdd] no ssid or ssid too long");
    return false;
  }

  if (passphrase && strlen(passphrase) > 63) {
    // fail passphrase too long!
    log_e("[WIFI][APlistAdd] passphrase too long");
    return false;
  }

  newAP.ssid = strdup(ssid);

  if (!newAP.ssid) {
    log_e("[WIFI][APlistAdd] fail newAP.ssid == 0");
    return false;
  }

  if (passphrase && *passphrase != '\0') {
    newAP.passphrase = strdup(passphrase);
    if (!newAP.passphrase) {
      log_e("[WIFI][APlistAdd] fail newAP.passphrase == 0");
      free(newAP.ssid);
      return false;
    }
  } else {
    newAP.passphrase = NULL;
  }
  newAP.hasFailed = false;
  APlist.push_back(newAP);
  log_i("[WIFI][APlistAdd] add SSID: %s", newAP.ssid);
  return true;
}

uint8_t WiFiMulti::run(uint32_t connectTimeout, bool scanHidden) {
  int8_t scanResult;
  unsigned long startTime;
  uint8_t status = WiFi.status();
  if (status == WL_CONNECTED) {
    if (!_bWFMInit && _connectionTestCBFunc != NULL) {
      if (_connectionTestCBFunc() == true) {
        _bWFMInit = true;
        return status;
      }
    } else {
      if (!_bStrict) {
        return status;
      } else {
        for (auto ap : APlist) {
          if (WiFi.SSID() == ap.ssid) {
            return status;
          }
        }
      }
    }
    WiFi.disconnect(false, false);
    delay(10);
    status = WiFi.status();
  }

  scanResult = WiFi.scanNetworks(false, scanHidden);
  if (scanResult == WIFI_SCAN_RUNNING) {
    // scan is running
    return WL_NO_SSID_AVAIL;
  } else if (scanResult >= 0) {
    // scan done analyze
    int32_t bestIndex = -1;
    WifiAPlist_t bestNetwork{ NULL, NULL, false };
    int bestNetworkDb = INT_MIN;
    int bestNetworkSec = WIFI_AUTH_MAX;
    uint8_t bestBSSID[6];
    int32_t bestChannel = 0;

    log_i("[WIFI] scan done");

    if (scanResult == 0) {
      log_e("[WIFI] no networks found");
    } else {
      log_i("[WIFI] %d networks found", scanResult);

      int8_t failCount = 0;
      int8_t foundCount = 0;
      for (int8_t i = 0; i < scanResult; ++i) {

        String ssid_scan;
        int32_t rssi_scan;
        uint8_t sec_scan;
        uint8_t* BSSID_scan;
        int32_t chan_scan;
        bool hidden_scan;

        WiFi.getNetworkInfo(i, ssid_scan, sec_scan, rssi_scan, BSSID_scan, chan_scan);
        hidden_scan = (ssid_scan.length() == 0) && scanHidden;
        // add any Open WiFi AP to the list, if allowed with setAllowOpenAP(true)
        if (_bAllowOpenAP && sec_scan == WIFI_AUTH_OPEN) {
          bool found = false;
          for (auto check : APlist) {
            if (ssid_scan == check.ssid) {
              found = true;
              break;
            }
          }
          // If we didn't find it, add this Open WiFi AP to the list
          if (!found) {
            log_i("[WIFI][APlistAdd] adding Open WiFi SSID: %s", ssid_scan.c_str());
            addAP(ssid_scan.c_str());
          }
        }

        if (hidden_scan) {
          log_v("hidden ssid on channel %d found, trying to connect with known credentials...", chan_scan);
        }

        bool known = false;
        for (uint32_t x = 0; x < APlist.size(); x++) {
          WifiAPlist_t entry = APlist[x];

          if (ssid_scan == entry.ssid || hidden_scan) {  // SSID match or hidden network found
            if (!hidden_scan) {
              log_v("known ssid: %s, has failed: %s", entry.ssid, entry.hasFailed ? "yes" : "no");
              foundCount++;
            }
            if (!entry.hasFailed) {
              if (hidden_scan) {
                WiFi.begin(entry.ssid, entry.passphrase, chan_scan, BSSID_scan);

                // If the ssid returned from the scan is empty, it is a hidden SSID
                // it appears that the WiFi.begin() function is asynchronous and takes
                // additional time to connect to a hidden SSID. Therefore a delay of 1000ms
                // is added for hidden SSIDs before calling WiFi.status()
                delay(1000);

                status = WiFi.status();
                startTime = millis();

                while (status != WL_CONNECTED && (millis() - startTime) <= connectTimeout) {
                  delay(10);
                  status = WiFi.status();
                }

                WiFi.disconnect();
                delay(10);

                if (status == WL_CONNECTED) {
                  log_v("hidden ssid %s found", entry.ssid);
                  ssid_scan = entry.ssid;
                  foundCount++;
                } else {
                  continue;
                }
              }
              known = true;
              log_v("rssi_scan: %d, bestNetworkDb: %d", rssi_scan, bestNetworkDb);
              if (rssi_scan > bestNetworkDb) {                                            // best network
                if (_bAllowOpenAP || (sec_scan == WIFI_AUTH_OPEN || entry.passphrase)) {  // check for passphrase if not open wlan
                  log_v("best network is now: %s", ssid_scan);
                  bestIndex = x;
                  bestNetworkSec = sec_scan;
                  bestNetworkDb = rssi_scan;
                  bestChannel = chan_scan;
                  memcpy((void*)&bestNetwork, (void*)&entry, sizeof(bestNetwork));
                  memcpy((void*)&bestBSSID, (void*)BSSID_scan, sizeof(bestBSSID));
                }
              }
              break;
            } else {
              failCount++;
            }
          }
        }

        if (known) {
          log_d(" --->   %d: [%d][%02X:%02X:%02X:%02X:%02X:%02X] %s (%d) (%c) (%s)",
                i,
                chan_scan,
                BSSID_scan[0], BSSID_scan[1], BSSID_scan[2], BSSID_scan[3], BSSID_scan[4], BSSID_scan[5],
                ssid_scan.c_str(),
                rssi_scan,
                (sec_scan == WIFI_AUTH_OPEN) ? ' ' : '*',
                (hidden_scan) ? "hidden" : "visible");
        } else {
          log_d("        %d: [%d][%02X:%02X:%02X:%02X:%02X:%02X] %s (%d) (%c) (%s)",
                i,
                chan_scan,
                BSSID_scan[0], BSSID_scan[1], BSSID_scan[2], BSSID_scan[3], BSSID_scan[4], BSSID_scan[5],
                ssid_scan.c_str(),
                rssi_scan,
                (sec_scan == WIFI_AUTH_OPEN) ? ' ' : '*',
                (hidden_scan) ? "hidden" : "visible");
        }
      }
      log_v("foundCount = %d, failCount = %d", foundCount, failCount);
      // if all the APs in the list have failed, reset the failure flags
      if (foundCount == failCount) {
        resetFails();  // keeps trying the APs in the list
      }
    }
    // clean up ram
    WiFi.scanDelete();

    if (bestIndex >= 0) {
      log_i("[WIFI] Connecting BSSID: %02X:%02X:%02X:%02X:%02X:%02X SSID: %s Channel: %d (%d)", bestBSSID[0], bestBSSID[1], bestBSSID[2], bestBSSID[3], bestBSSID[4], bestBSSID[5], bestNetwork.ssid, bestChannel, bestNetworkDb);

      if (ipv6_support == true) {
        WiFi.enableIPv6();
      }
      WiFi.disconnect();
      delay(10);
      WiFi.begin(bestNetwork.ssid, (_bAllowOpenAP && bestNetworkSec == WIFI_AUTH_OPEN) ? NULL : bestNetwork.passphrase, bestChannel, bestBSSID);
      status = WiFi.status();
      _bWFMInit = true;

      startTime = millis();
      // wait for connection, fail, or timeout
      while (status != WL_CONNECTED && (millis() - startTime) <= connectTimeout) {  // && status != WL_NO_SSID_AVAIL && status != WL_CONNECT_FAILED
        delay(10);
        status = WiFi.status();
      }

      switch (status) {
        case WL_CONNECTED:
          log_i("[WIFI] Connecting done.");
          log_d("[WIFI] SSID: %s", WiFi.SSID().c_str());
          log_d("[WIFI] IP: %s", WiFi.localIP().toString().c_str());
          log_d("[WIFI] MAC: %s", WiFi.BSSIDstr().c_str());
          log_d("[WIFI] Channel: %d", WiFi.channel());

          if (_connectionTestCBFunc != NULL) {
            // We connected to an AP but if it's a captive portal we're not going anywhere.  Test it.
            if (_connectionTestCBFunc()) {
              resetFails();
            } else {
              markAsFailed(bestIndex);
              WiFi.disconnect();
              delay(10);
              status = WiFi.status();
            }
          } else {
            resetFails();
          }
          break;
        case WL_NO_SSID_AVAIL:
          log_e("[WIFI] Connecting Failed AP not found.");
          markAsFailed(bestIndex);
          break;
        case WL_CONNECT_FAILED:
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

void WiFiMulti::enableIPv6(bool state) {
  ipv6_support = state;
}

void WiFiMulti::markAsFailed(int32_t i) {
  APlist[i].hasFailed = true;
  log_d("[WIFI] Marked SSID %s as failed", APlist[i].ssid);
}

void WiFiMulti::resetFails() {
  for (uint32_t i = 0; i < APlist.size(); i++) {
    APlist[i].hasFailed = false;
  }
  log_d("[WIFI] Resetting failure flags");
}

void WiFiMulti::setStrictMode(bool bStrict) {
  _bStrict = bStrict;
}

void WiFiMulti::setAllowOpenAP(bool bAllowOpenAP) {
  _bAllowOpenAP = bAllowOpenAP;
}

void WiFiMulti::setConnectionTestCallbackFunc(ConnectionTestCB_t cbFunc) {
  _connectionTestCBFunc = cbFunc;
}

#endif /* SOC_WIFI_SUPPORTED */
