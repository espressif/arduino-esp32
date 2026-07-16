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
 * Native Thread discovery — streaming callbacks.
 *
 * Matches the OpenThread / Matter model: each Discovery Response is delivered
 * through onResult(); onComplete() fires when the scan ends.
 * Call scanDelete() from loop() after scanComplete() — not from callbacks.
 */

#include <Arduino.h>
#include "OThread.h"
#include "OThreadScan.h"

static volatile int networksSeen = 0;

static void onDiscoverResult(const OThreadNetworkInfo &info, void *) {
  networksSeen++;
  Serial.printf(
    "  found: %s | extPan=%s | pan=%04x | ch=%u | %d dBm | joinable=%s\r\n", info.networkName, info.extendedPanIdStr().c_str(),
    info.panId, info.channel, info.rssi, info.joinable ? "yes" : "no"
  );
}

static void onDiscoverComplete(int16_t resultCount, otError error, void *) {
  if (error != OT_ERROR_NONE) {
    Serial.printf("discovery complete: error %d\r\n", error);
  } else if (resultCount == OT_DISCOVER_FAILED) {
    Serial.println("discovery complete: failed");
  } else {
    Serial.printf("discovery complete: %d network(s) (%d reported live)\r\n", resultCount, networksSeen);
  }
  networksSeen = 0;
}

void setup() {
  Serial.begin(115200);

  OThread.begin(false);
  OThread.networkInterfaceUp();

  OThreadScan.onResult(onDiscoverResult);
  OThreadScan.onComplete(onDiscoverComplete);
  OThreadScan.setScanTimeout(30000);

  Serial.println("Setup done — callback discovery");
}

void loop() {
  Serial.println("Thread discovery start (callbacks)");
  networksSeen = 0;

  int16_t rc = OThreadScan.discoverNetworks(true);
  if (rc == OT_DISCOVER_RUNNING) {
    while (OThreadScan.scanComplete() == OT_DISCOVER_RUNNING) {
      delay(50);
    }
    OThreadScan.scanDelete();
  } else if (rc >= 0) {
    Serial.printf("discovery finished immediately with %d network(s)\r\n", rc);
    OThreadScan.scanDelete();
  } else {
    Serial.println("discovery failed to start");
  }

  delay(10000);
}
