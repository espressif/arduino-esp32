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
 * Thread DNS-SD discover — resolve a host label (queryHost).
 *
 * Pair with ThreadDNSSD_Advertise on another board (hostname sensor-1).
 * Set OT_NETKEY to your OTBR Network Key.
 */

#include <Arduino.h>
#include "OThread.h"
#include "OThreadDNSSD.h"

// Same Network Key as the OTBR Thread network (other dataset fields are learned on attach).
static const uint8_t OT_NETKEY[OT_NETWORK_KEY_SIZE] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};

static const char *kPeerHost = "sensor-1";

static bool waitAttached(uint32_t timeoutMs) {
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    ot_device_role_t role = OThread.otGetDeviceRole();
    if (role == OT_ROLE_CHILD || role == OT_ROLE_ROUTER || role == OT_ROLE_LEADER) {
      return true;
    }
    delay(200);
  }
  return false;
}

// Returning from setup() still runs loop(); halt so queryHost is not called without begin().
static void halt(const char *msg) {
  Serial.println(msg);
  while (true) {
    delay(1000);
  }
}

static bool isEmptyV6(const IPAddress &addr) {
  for (int i = 0; i < 16; ++i) {
    if (addr[i] != 0) {
      return false;
    }
  }
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("ThreadDNSSD_QueryHost");

  OThread.begin(false);

  DataSet ds;
  ds.clear();
  ds.setNetworkKey(OT_NETKEY);
  OThread.commitDataSet(ds);
  OThread.networkInterfaceUp();
  OThread.start();

  Serial.println("Waiting to attach...");
  if (!waitAttached(60000)) {
    halt("FAIL: not attached (check Network Key vs OTBR)");
  }
  Serial.printf("Attached as %s\r\n", OThread.otGetStringDeviceRole());

  if (!OThreadDNSSD.begin("resolver")) {
    halt("FAIL: OThreadDNSSD.begin");
  }
  delay(2000);
}

void loop() {
  Serial.println();
  Serial.printf("queryHost(\"%s\")...\r\n", kPeerHost);
  IPAddress addr = OThreadDNSSD.queryHost(kPeerHost);
  if (isEmptyV6(addr)) {
    Serial.printf("FAIL: no address (lastError=%d) — is Advertise board registered?\r\n", (int)OThreadDNSSD.lastError());
  } else {
    Serial.printf("PASS: %s -> %s\r\n", kPeerHost, addr.toString().c_str());
  }
  delay(10000);
}
