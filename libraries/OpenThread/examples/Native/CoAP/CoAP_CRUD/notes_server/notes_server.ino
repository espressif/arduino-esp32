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
 * CoAP CRUD — notes server
 *
 * REST collection "notes" backed by OThreadCoAPResourceStore.
 * Supports GET (list/item), POST (create), PUT (update), DELETE.
 */

#include <Arduino.h>
#include "OThread.h"
#include "OThreadCoAP.h"

const char PSKD[] = "J01NME";
const uint32_t JOINER_WINDOW_SEC = 600;
const uint8_t CHANNEL = 15;
const uint16_t PAN_ID = 0xC0DE;
const uint8_t NETKEY[OT_NETWORK_KEY_SIZE] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99};

OThreadCoAPResourceStore Notes;

const uint32_t ATTACH_TIMEOUT_MS = 30000;
const uint32_t ATTACH_DOT_MS = 2000;

static void onNotesChanged(const char *basePath, void *ctx) {
  (void)ctx;
  Serial.printf("Store '%s' changed (%u items)\n", basePath, (unsigned)Notes.count());
}

static bool waitForAttach(uint32_t timeoutMs, uint32_t dotMs) {
  uint32_t start = millis();
  uint32_t lastDot = start;

  while (millis() - start < timeoutMs) {
    if (OThread.otGetDeviceRole() >= OT_ROLE_CHILD) {
      Serial.println();
      return true;
    }
    if (millis() - lastDot >= dotMs) {
      Serial.print('.');
      lastDot = millis();
    }
    delay(100);
  }
  Serial.println();
  return false;
}

static bool startNetwork() {
  Serial.println("Forming Thread network...");
  DataSet ds;
  ds.initNew();
  ds.setNetworkName("ESP_OT_CoAP_CRUD");
  ds.setChannel(CHANNEL);
  ds.setPanId(PAN_ID);
  ds.setNetworkKey(NETKEY);
  OThread.commitDataSet(ds);
  OThread.networkInterfaceUp();
  OThread.start();

  Serial.print("Waiting for attach");
  if (!waitForAttach(ATTACH_TIMEOUT_MS, ATTACH_DOT_MS)) {
    Serial.println("Attach timeout.");
    return false;
  }
  Serial.printf("Attached as %s.\n", OThread.otGetStringDeviceRole());

  Serial.println("Starting Commissioner...");
  if (OThread.startCommissioner() != OT_ERROR_NONE) {
    Serial.println("Commissioner start failed.");
    return false;
  }
  OThread.addJoiner(PSKD, JOINER_WINDOW_SEC);
  Serial.printf("Commissioner ready (PSKd \"%s\")\n", PSKD);
  return true;
}

void setup() {
  Serial.begin(115200);
  Serial.println("=== CoAP CRUD — notes server ===");

  OThread.begin(false);
  while (!startNetwork()) {
    Serial.println("Network failed, retrying in 2s...");
    OThread.stop();
    delay(2000);
  }

  Serial.println("Starting CoAP server...");
  if (!OThreadCoAPServer.begin()) {
    Serial.println("CoAP server failed.");
    while (1) {
      delay(1000);
    }
  }

  Notes.onChange(onNotesChanged);
  if (!Notes.attach(OThreadCoAPServer, "notes", 8)) {
    Serial.println("ResourceStore attach failed.");
    while (1) {
      delay(1000);
    }
  }

  Serial.println("Ready. REST endpoints:");
  Serial.println("  GET    notes       — list");
  Serial.println("  GET    notes/<id>  — read");
  Serial.println("  POST   notes       — create");
  Serial.println("  PUT    notes/<id>  — update");
  Serial.println("  DELETE notes/<id>  — delete");
  Serial.printf("Leader: %s\n", OThread.getLeaderRloc().toString().c_str());
}

void loop() {
  delay(10);
}
