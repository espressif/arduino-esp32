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
 * CoAP Simple GET — client side
 *
 * Joins the Leader started by server.ino, then performs one GET on path
 * "hello" and prints the response.
 *
 * The client only needs the network key to discover and attach to the Leader.
 * Channel, PAN ID, and Extended PAN ID are learned from the server at attach
 * time (same pattern as SimpleThreadNetwork/RouterNode).
 */

#include <Arduino.h>
#include "OThread.h"
#include "OThreadCoAP.h"

// Must match server.ino
const uint8_t NETKEY[OT_NETWORK_KEY_SIZE] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};

const uint32_t ATTACH_TIMEOUT_MS = 20000;
const uint32_t ATTACH_DOT_MS = 2000;

OThreadCoAPClient CoapClient;
static IPAddress serverIp;

static bool waitForAttach(uint32_t timeoutMs, uint32_t dotMs) {
  uint32_t start = millis();
  uint32_t lastDot = start;

  while (millis() - start < timeoutMs) {
    ot_device_role_t role = OThread.otGetDeviceRole();
    if (role == OT_ROLE_CHILD || role == OT_ROLE_ROUTER) {
      Serial.println();
      return true;
    }
    if (role == OT_ROLE_LEADER) {
      Serial.println();
      Serial.println("Started as Leader — is server.ino running first?");
      return false;
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

static bool joinNetwork() {
  Serial.println("Joining Thread network...");
  DataSet ds;
  ds.clear();
  ds.setNetworkKey(NETKEY);
  OThread.commitDataSet(ds);
  OThread.networkInterfaceUp();
  OThread.start();

  Serial.print("Waiting for attach");
  if (!waitForAttach(ATTACH_TIMEOUT_MS, ATTACH_DOT_MS)) {
    Serial.println("Attach timeout.");
    return false;
  }
  serverIp = OThread.getLeaderRloc();
  Serial.printf("Attached as %s. Server (Leader RLOC): %s\n", OThread.otGetStringDeviceRole(), serverIp.toString().c_str());
  return true;
}

void setup() {
  Serial.begin(115200);

  Serial.println("=== CoAP Simple GET — client ===");
  OThread.begin(false);
  CoapClient.setTimeout(3000);
  CoapClient.setConfirmable(true);  // CLI default for GET when expecting payload

  if (!joinNetwork()) {
    Serial.println("Thread attach failed.");
    while (1) {
      delay(1000);
    }
  }

  delay(500);  // let server CoAP settle

  Serial.println("Ready. Sending GET hello...");
  int code = CoapClient.GET(serverIp, "hello");
  Serial.printf("GET hello -> code %d (%s)\n", code, OThreadCoAP::responseCodeToString(code));
  if (code >= 0) {
    Serial.printf("Payload: %s\n", CoapClient.getString().c_str());
  } else {
    Serial.printf("Error: %s\n", OThreadCoAP::errorToString(code));
  }
}

void loop() {
  // One-shot demo
  delay(1000);
}
