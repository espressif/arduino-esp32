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
 * Native Thread discovery — async polling (WiFiScanAsync-style).
 *
 *   OThreadScan.discoverNetworks(true)  — returns OT_DISCOVER_RUNNING immediately
 *   OThreadScan.scanComplete()          — polled from loop()
 *   OThreadScan.scanDelete()            — frees results after printing
 */

#include <Arduino.h>
#include "OThread.h"
#include "OThreadScan.h"

static bool discoverPending = false;

static void startAsyncDiscover() {
  Serial.println("Thread discovery async start");
  OThreadScan.setScanTimeout(30000);
  int16_t rc = OThreadScan.discoverNetworks(true);
  if (rc == OT_DISCOVER_RUNNING) {
    discoverPending = true;
  } else if (rc == OT_DISCOVER_FAILED) {
    Serial.println("discovery failed to start");
    discoverPending = false;
  } else {
    Serial.printf("discovery returned immediately with %d network(s)\r\n", rc);
    discoverPending = false;
  }
}

static void printResults(int count) {
  if (count == 0) {
    Serial.println("no Thread networks found");
  } else {
    Serial.printf("%d network(s):\r\n", count);
    for (int i = 0; i < count; ++i) {
      const OThreadNetworkInfo &net = OThreadScan.getResult(i);
      Serial.printf(
        "  [%d] %s | extPan=%s | pan=%04x | %s | ch=%u | %d dBm | lqi=%u | joinable=%s\r\n", i, net.networkName,
        net.extendedPanIdStr().c_str(), net.panId, net.extAddressStr().c_str(), net.channel, net.rssi, net.lqi,
        net.joinable ? "yes" : "no"
      );
    }
  }
  // Release reserved capacity after every completed scan (including 0 results).
  OThreadScan.scanDelete();
}

static void handleDiscoverComplete(int16_t status) {
  if (status == OT_DISCOVER_FAILED) {
    Serial.println("async discovery failed — restarting");
  } else {
    Serial.println("async discovery done");
    printResults(status);
  }
  discoverPending = false;
}

void setup() {
  Serial.begin(115200);

  OThread.begin(false);
  OThread.networkInterfaceUp();

  Serial.println("Setup done — starting async discovery");
  startAsyncDiscover();
}

void loop() {
  if (discoverPending) {
    int16_t status = OThreadScan.scanComplete();
    if (status < 0) {
      if (status == OT_DISCOVER_FAILED) {
        handleDiscoverComplete(status);
        delay(2000);
        startAsyncDiscover();
      }
    } else {
      handleDiscoverComplete(status);
      delay(2000);
      startAsyncDiscover();
    }
  } else {
    startAsyncDiscover();
  }

  delay(250);
  Serial.println("loop running...");
}
