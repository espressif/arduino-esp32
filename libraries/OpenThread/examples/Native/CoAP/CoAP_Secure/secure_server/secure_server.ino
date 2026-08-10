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
 * CoAP Secure — server
 *
 * CoAPS server on port 5684 with PSK authentication.
 * Serves GET /status with a short text payload.
 *
 * Pair with secure_client/secure_client.ino.
 *
 * BUILD: requires OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE=1
 */

#include <Arduino.h>
#include "OThread.h"
#include "OThreadCoAP.h"

const char PSKD[] = "J01NME";
const uint32_t JOINER_WINDOW_SEC = 600;
const uint8_t CHANNEL = 15;
const uint16_t PAN_ID = 0x5EC0;
const uint8_t NETKEY[OT_NETWORK_KEY_SIZE] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};

const uint32_t ATTACH_TIMEOUT_MS = 30000;
const uint32_t ATTACH_DOT_MS = 2000;

// Shared PSK for demo — same bytes on client and server
static const uint8_t COAP_PSK[] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0, 0x00};
static const char COAP_PSK_ID[] = "esp-coap-demo";

static void onStatus(OThreadCoAPRequest &req, OThreadCoAPResponse &resp, void *ctx) {
  (void)ctx;
  if (req.method() != OT_COAP_REQ_GET) {
    resp.setCode(OT_COAP_RESP_METHOD_NA);
    resp.send();
    return;
  }
  Serial.printf("CoAPS GET from %s\n", req.remoteIP().toString().c_str());
  resp.setCode(OT_COAP_RESP_OK);
  resp.setPayload("secure-ok");
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

static bool startNetwork() {
  Serial.println("Forming Thread network...");
  DataSet ds;
  ds.initNew();
  ds.setNetworkName("ESP_OT_CoAP_Secure");
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
  if (OThread.startCommissioner() == OT_ERROR_NONE) {
    OThread.addJoiner(PSKD, JOINER_WINDOW_SEC);
    Serial.printf("Commissioner ready (PSKd \"%s\")\n", PSKD);
  } else {
    Serial.println("Commissioner start failed — joiners cannot attach until a Commissioner is active.");
  }
  return true;
}

void setup() {
  Serial.begin(115200);
  Serial.println("=== CoAP Secure — server ===");

  if (!OThreadCoAP::secureApiEnabled()) {
    Serial.println("CoAPS is not enabled in this build. This sketch will not run.");
    while (1) {
      delay(1000);
    }
  }

  OThread.begin(false);
  while (!startNetwork()) {
    Serial.println("Network failed, retrying in 2s...");
    OThread.stop();
    delay(2000);
  }

  Serial.println("Starting CoAPS server...");
  OThreadCoAPSecureServer.setPSK(COAP_PSK, sizeof(COAP_PSK), COAP_PSK_ID);
  OThreadCoAPSecureServer.on("status", OT_COAP_METHOD_GET, onStatus);

  if (!OThreadCoAPSecureServer.begin()) {
    Serial.println("CoAPS server start failed.");
    while (1) {
      delay(1000);
    }
  }

  Serial.printf("Ready. CoAPS listening on port %u (PSK id \"%s\")\n", (unsigned)OT_COAP_SECURE_DEFAULT_PORT, COAP_PSK_ID);
  Serial.printf("Mesh-local: %s\n", OThread.getMeshLocalEid().toString().c_str());
}

void loop() {
  delay(10);
}
