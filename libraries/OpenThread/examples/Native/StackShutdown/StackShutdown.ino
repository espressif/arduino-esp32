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
 * Native StackShutdown — UDP + CoAP teardown, then stack restart
 *
 * Forms a small Thread network, binds OThreadUDP on port 5050, starts
 * OThreadCoAPServer on path "hello", waits RUN_MS, then stops application
 * layers in the documented order before OThread.end(). Calls setup() again
 * to bring the stack back up without resetting the chip.
 *
 * Compare with CLI-only ../../CLI/StackShutdown/.
 */

#include <Arduino.h>
#include "OThread.h"
#include "OThreadUDP.h"
#include "OThreadCoAP.h"

static const uint32_t RUN_MS = 30000;
static const uint16_t UDP_PORT = 5050;

static OThreadUDP OtUdp;

static void onHello(OThreadCoAPRequest &req, OThreadCoAPResponse &resp, void *ctx) {
  (void)ctx;
  if (req.method() != OT_COAP_REQ_GET) {
    resp.setCode(OT_COAP_RESP_METHOD_NA);
    resp.send();
    return;
  }
  resp.setCode(OT_COAP_RESP_OK);
  resp.setPayload("shutdown demo");
  resp.send();
}

static bool waitForAttach(uint32_t timeoutMs) {
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    if (OThread.otGetDeviceRole() >= OT_ROLE_CHILD) {
      return true;
    }
    delay(100);
  }
  return false;
}

static bool formNetwork() {
  DataSet ds;
  ds.initNew();
  ds.setNetworkName("ESP_OT_Shutdown");
  ds.setChannel(15);
  ds.setPanId(0x5D00);
  OThread.commitDataSet(ds);
  OThread.networkInterfaceUp();
  OThread.start();
  return waitForAttach(20000);
}

static void gracefulShutdown() {
  Serial.println("Shutting down (CoAP -> UDP -> stack)...");

  OThreadCoAPServer.stop();
  OtUdp.stop();

  OThread.stop();
  OThread.networkInterfaceDown();
  OThread.end();

  Serial.println("OpenThread ended. Running setup() to start again.");
  setup();  // Run setup() again to start the stack again
}

void setup() {
  Serial.begin(115200);
  Serial.println("=== Native StackShutdown (UDP + CoAP) ===");

  OThread.begin(false);
  if (!formNetwork()) {
    Serial.println("Attach failed.");
    while (1) {
      delay(1000);
    }
  }
  Serial.printf("Attached as %s\n", OThread.otGetStringDeviceRole());

  if (!OtUdp.begin(UDP_PORT)) {
    Serial.println("UDP bind failed.");
    while (1) {
      delay(1000);
    }
  }
  Serial.printf("UDP listening on port %u\n", (unsigned)UDP_PORT);

  OThreadCoAPServer.on("hello", OT_COAP_METHOD_GET, onHello);
  if (!OThreadCoAPServer.begin()) {
    Serial.println("CoAP server start failed.");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("CoAP server on path 'hello'.");
  Serial.printf("Waiting %lu ms before graceful shutdown...\n", (unsigned long)RUN_MS);
}

void loop() {
  static bool done = false;
  static uint32_t startMs = 0;

  if (startMs == 0) {
    startMs = millis();
  }
  if (!done && millis() - startMs >= RUN_MS) {
    gracefulShutdown();
    done = true;
  }
  delay(100);
}
