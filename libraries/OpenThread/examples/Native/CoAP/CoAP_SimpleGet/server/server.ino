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
 * CoAP Simple GET — server side
 *
 * Forms a small Thread network, starts a CoAP server on port 5683, and
 * responds to GET /hello with a text payload.
 *
 * Pair with client/client.ino on a second board.
 *
 * Forms the Thread network with a fixed operational dataset (including
 * Extended PAN ID). Flash before client.ino.
 */

#include <Arduino.h>
#include "OThread.h"
#include "OThreadCoAP.h"

const uint8_t CHANNEL = 24;
const uint16_t PAN_ID = 0xC0A0;
const uint8_t EXTPANID[OT_EXT_PAN_ID_SIZE] = {0xC0, 0xA0, 0x00, 0x00, 0x47, 0x45, 0x54, 0x01};
const uint32_t ATTACH_TIMEOUT_MS = 20000;
const uint32_t ATTACH_DOT_MS = 2000;
const uint8_t NETKEY[OT_NETWORK_KEY_SIZE] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};

static void onHello(OThreadCoAPRequest &req, OThreadCoAPResponse &resp, void *ctx) {
  (void)ctx;
  if (req.method() != OT_COAP_REQ_GET) {
    resp.setCode(OT_COAP_RESP_METHOD_NA);
    resp.send();
    return;
  }
  resp.setCode(OT_COAP_RESP_OK);
  resp.setPayload("Hello from CoAP!");
  resp.send();
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

static bool formNetwork() {
  Serial.println("Forming Thread network...");
  DataSet ds;
  ds.initNew();
  ds.setNetworkName("ESP_OT_CoAP_Demo");
  ds.setChannel(CHANNEL);
  ds.setPanId(PAN_ID);
  ds.setExtendedPanId(EXTPANID);
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
  Serial.printf("Mesh-local EID: %s\n", OThread.getMeshLocalEid().toString().c_str());
  Serial.printf("Leader RLOC: %s\n", OThread.getLeaderRloc().toString().c_str());
  return true;
}

void setup() {
  Serial.begin(115200);

  Serial.println("=== CoAP Simple GET — server ===");
  OThread.begin(false);
  if (!formNetwork()) {
    Serial.println("Thread attach failed.");
    while (1) {
      delay(1000);
    }
  }

  Serial.println("Starting CoAP server...");
  // Optional: OThreadCoAPServer.setPort(5683);  // before begin() if not using default
  OThreadCoAPServer.on("hello", OT_COAP_METHOD_GET, onHello);
  if (!OThreadCoAPServer.begin()) {
    Serial.println("CoAP server start failed.");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("Ready. CoAP server listening on path 'hello'");
}

void loop() {
  // CoAP requests invoke onHello() automatically — no CoAP polling in loop().
  delay(10);
}
