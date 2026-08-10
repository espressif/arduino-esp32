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
 * Thread DNS-SD advertise — simple blocking waitForAnnounce.
 *
 * Set OT_NETKEY to your OTBR Network Key.
 * For event-driven watching and re-advertise after OTBR restart, see
 * ThreadDNSSD_Advertise_Callback.
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
  Serial.println("ThreadDNSSD_Advertise");

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

  if (!OThreadDNSSD.begin("sensor-1")) {
    Serial.println("FAIL: OThreadDNSSD.begin");
    return;
  }
  if (!OThreadDNSSD.addService("ot", "udp", 12345)) {
    Serial.println("FAIL: addService");
    return;
  }
  if (!OThreadDNSSD.addServiceTxt("ot", "udp", "path", "/status")) {
    Serial.println("FAIL: addServiceTxt");
    return;
  }

  Serial.println("Waiting for SRP announce (need OTBR SRP server)...");
  if (OThreadDNSSD.waitForAnnounce(60000)) {
    Serial.printf("PASS: announced OK as %s\r\n", OThreadDNSSD.hostname());
  } else {
    otError err = OThreadDNSSD.lastError();
    if (err == OT_ERROR_DUPLICATED || err == OT_ERROR_SECURITY) {
      Serial.printf(
        "FAIL: name conflict for '%s' (lastError=%d). "
        "Pick a unique hostname, keep NVS on reflash, or clear OTBR soft state.\r\n",
        OThreadDNSSD.hostname(), (int)err
      );
    } else {
      Serial.printf("FAIL: announce timeout/error (lastError=%d)\r\n", (int)err);
    }
  }
}

void loop() {
  delay(5000);
  // Live SRP client state: may become 1 later if OpenThread retries successfully
  // (e.g. after the OTBR clears a prior name conflict).
  Serial.printf("announceComplete=%d role=%s\r\n", OThreadDNSSD.isAnnounceComplete(), OThread.otGetStringDeviceRole());
}
