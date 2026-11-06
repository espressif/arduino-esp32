// Copyright 2026 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/*
 * Native Thread discovery — blocking (WiFiScan-style).
 *
 * Uses OThreadScan.discoverNetworks(), which wraps otThreadDiscover() /
 * OpenThread CLI "discover". Results include Thread identity and 802.15.4
 * link fields in OThreadNetworkInfo.
 *
 * Start a Leader on another board first (SimpleThreadNetwork/LeaderNode).
 *
 * This sketch sets OT_DISCOVER_MAX_RESULTS to 32 (default in the library is 16).
 */

#include <Arduino.h>
#include "OThread.h"

// This sketch stores up to 32 unique networks (library default is 16).
// Define OT_DISCOVER_MAX_RESULTS before including OThreadScan.h.
#define OT_DISCOVER_MAX_RESULTS 32
#include "OThreadScan.h"

static void printNetwork(const OThreadNetworkInfo &net, int index) {
  Serial.printf("%2d", index + 1);
  Serial.print(" | ");
  Serial.print(net.joinable ? "1" : "0");
  Serial.print(" | ");
  Serial.printf("%-16.16s", net.networkName);
  Serial.print(" | ");
  Serial.print(net.extendedPanIdStr());
  Serial.print(" | ");
  Serial.printf("%04x", net.panId);
  Serial.print(" | ");
  Serial.print(net.extAddressStr());
  Serial.print(" | ");
  Serial.printf("%2u", net.channel);
  Serial.print(" | ");
  Serial.printf("%3d", net.rssi);
  Serial.print(" | ");
  Serial.printf("%3u", net.lqi);
  Serial.println();
}

static void discoverBlocking() {
  Serial.println("Thread discovery start");
  int n = OThreadScan.discoverNetworks();
  Serial.println("Thread discovery done");

  if (n == OT_DISCOVER_FAILED) {
    Serial.println("discovery failed (interface down, lock failure, or timeout)");
  } else if (n == OT_DISCOVER_RUNNING) {
    Serial.println("discovery already in progress (OT_DISCOVER_RUNNING)");
  } else {
    if (n == 0) {
      Serial.println("no Thread networks found");
    } else {
      Serial.print(n);
      Serial.println(" network(s) found");
      Serial.println("Nr | J | Network Name     | Extended PAN     | PAN  | MAC Address      | CH | dBm | LQI");
      for (int i = 0; i < n; ++i) {
        printNetwork(OThreadScan.getResult(i), i);
        delay(10);
      }
    }
    // Release reserved capacity after every completed scan (including 0 results).
    OThreadScan.scanDelete();
  }

  Serial.println("-------------------------------------");
}

void setup() {
  Serial.begin(115200);

  OThread.begin(false);
  OThread.networkInterfaceUp();

  Serial.println("Setup done — IPv6 interface up, Thread start not required");
}

void loop() {
  discoverBlocking();
  delay(10000);
}
