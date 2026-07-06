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
 */

#include <Arduino.h>
#include "OThread.h"
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
    Serial.println("discovery failed (interface down, busy, or timeout)");
  } else if (n == OT_DISCOVER_RUNNING) {
    Serial.println("unexpected: discovery still running");
  } else if (n == 0) {
    Serial.println("no Thread networks found");
  } else {
    Serial.print(n);
    Serial.println(" network(s) found");
    Serial.println("Nr | J | Network Name     | Extended PAN     | PAN  | MAC Address      | CH | dBm | LQI");
    for (int i = 0; i < n; ++i) {
      printNetwork(OThreadScan.getResult(i), i);
      delay(10);
    }
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
