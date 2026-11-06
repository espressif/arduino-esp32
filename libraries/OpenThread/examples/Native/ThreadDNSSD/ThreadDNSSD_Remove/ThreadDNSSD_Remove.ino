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
 * Thread DNS-SD — complete add / remove / end demonstration.
 *
 * Runs two add→announce→remove cycles, then OThreadDNSSD.end().
 * Watch OTBR CLI `srp server service` as the instance appears and goes.
 *
 * Set OT_NETKEY to your OTBR Network Key.
 */

#include <Arduino.h>
#include "OThread.h"
#include "OThreadDNSSD.h"

// Same Network Key as the OTBR Thread network (other dataset fields are learned on attach).
static const uint8_t OT_NETKEY[OT_NETWORK_KEY_SIZE] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};

static const char *kHostName = "sensor-rm";
static const uint16_t kServicePort = 12347;
static const uint8_t kAddRemoveCycles = 2;
static const uint32_t kHoldAdvertisedMs = 8000;  // stay registered before remove
static const uint32_t kHoldRemovedMs = 5000;     // stay removed before next add
static const uint32_t kRemoveSettleMs = 3000;    // allow SRP remove to finish

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

// Returning from setup() still runs loop(); halt so idle status is not printed after a failed demo.
static void halt(const char *msg) {
  Serial.println(msg);
  while (true) {
    delay(1000);
  }
}

static void printStatus(const char *tag) {
  Serial.printf(
    "  [%s] announceComplete=%d lastError=%d role=%s\r\n", tag, OThreadDNSSD.isAnnounceComplete(), (int)OThreadDNSSD.lastError(),
    OThread.otGetStringDeviceRole()
  );
}

static bool advertiseService() {
  Serial.printf("ADD: addService(\"ot\", \"udp\", %u)\r\n", (unsigned)kServicePort);
  if (!OThreadDNSSD.addService("ot", "udp", kServicePort)) {
    Serial.println("FAIL: addService");
    return false;
  }
  Serial.println("Waiting for SRP announce...");
  if (!OThreadDNSSD.waitForAnnounce(60000)) {
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
    return false;
  }
  printStatus("after add");
  Serial.println("  Tip: OTBR CLI `srp server service` should list this instance.");
  return true;
}

static bool removeServiceOnly() {
  Serial.println("REMOVE: removeService(\"ot\", \"udp\")");
  if (!OThreadDNSSD.removeService("ot", "udp")) {
    Serial.println("FAIL: removeService");
    return false;
  }
  Serial.printf("Waiting %lu ms for SRP remove to settle...\r\n", (unsigned long)kRemoveSettleMs);
  delay(kRemoveSettleMs);
  printStatus("after remove");
  Serial.println("  Tip: OTBR CLI `srp server service` should no longer list this instance.");
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("ThreadDNSSD_Remove — two add/remove cycles, then end()");

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

  Serial.printf("OThreadDNSSD.begin(\"%s\")\r\n", kHostName);
  if (!OThreadDNSSD.begin(kHostName)) {
    halt("FAIL: OThreadDNSSD.begin");
  }
  printStatus("after begin");

  for (uint8_t cycle = 1; cycle <= kAddRemoveCycles; ++cycle) {
    Serial.println();
    Serial.printf("======== add/remove cycle %u / %u ========\r\n", (unsigned)cycle, (unsigned)kAddRemoveCycles);

    if (!advertiseService()) {
      halt("FAIL: advertise cycle");
    }

    Serial.printf("Holding advertised for %lu ms...\r\n", (unsigned long)kHoldAdvertisedMs);
    delay(kHoldAdvertisedMs);
    printStatus("before remove");

    if (!removeServiceOnly()) {
      halt("FAIL: remove cycle");
    }

    if (cycle < kAddRemoveCycles) {
      Serial.printf("Holding removed for %lu ms before next add...\r\n", (unsigned long)kHoldRemovedMs);
      delay(kHoldRemovedMs);
      printStatus("before next add");
    }
  }

  Serial.println();
  Serial.println("======== final end() ========");
  Serial.println("Calling OThreadDNSSD.end() (unregister host + stop SRP client)...");
  OThreadDNSSD.end();
  printStatus("after end");
  Serial.println("PASS: two add/remove cycles + end() complete");
  Serial.println("  Tip: OTBR CLI `srp server service` / `srp server host` should be clear for this device.");
}

void loop() {
  delay(15000);
  Serial.printf("idle announceComplete=%d role=%s\r\n", OThreadDNSSD.isAnnounceComplete(), OThread.otGetStringDeviceRole());
}
