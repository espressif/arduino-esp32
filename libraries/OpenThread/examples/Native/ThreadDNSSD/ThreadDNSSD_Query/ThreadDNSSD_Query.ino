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
 * Thread DNS-SD discover — browse for _ot._udp instances (queryService).
 *
 * Pair with ThreadDNSSD_Advertise (or Advertise_Callback) on another board
 * on the same OTBR Thread network.
 *
 * Set OT_NETKEY to your OTBR Network Key.
 */

#include <Arduino.h>
#include "OThread.h"
#include "OThreadDNSSD.h"

// Same Network Key as the OTBR Thread network (other dataset fields are learned on attach).
static const uint8_t OT_NETKEY[OT_NETWORK_KEY_SIZE] = {
  0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff
};

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

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("ThreadDNSSD_Query");

  OThread.begin(false);

  DataSet ds;
  ds.clear();
  ds.setNetworkKey(OT_NETKEY);
  OThread.commitDataSet(ds);
  OThread.networkInterfaceUp();
  OThread.start();

  Serial.println("Waiting to attach...");
  if (!waitAttached(60000)) {
    Serial.println("FAIL: not attached (check Network Key vs OTBR)");
    return;
  }
  Serial.printf("Attached as %s\r\n", OThread.otGetStringDeviceRole());

  // begin() enables SRP auto-start so DNS default server follows the BR.
  if (!OThreadDNSSD.begin("browser")) {
    Serial.println("FAIL: OThreadDNSSD.begin");
    return;
  }
  delay(2000);  // allow Network Data / DNS server selection
}

void loop() {
  Serial.println();
  Serial.println("queryService(\"ot\", \"udp\")...");
  int n = OThreadDNSSD.queryService("ot", "udp");
  Serial.printf("Found %d instance(s) (lastError=%d)\r\n", n, (int)OThreadDNSSD.lastError());

  for (int i = 0; i < n; ++i) {
    Serial.printf(
      "  [%d] instance=%s host=%s port=%u addr=%s\r\n", i, OThreadDNSSD.instanceName(i), OThreadDNSSD.hostname(i),
      (unsigned)OThreadDNSSD.port(i), OThreadDNSSD.address(i).toString().c_str()
    );
    for (int t = 0; t < OThreadDNSSD.numTxt(i); ++t) {
      Serial.printf("       TXT %s=%s\r\n", OThreadDNSSD.txtKey(i, t).c_str(), OThreadDNSSD.txt(i, t).c_str());
    }
  }

  delay(10000);
}
