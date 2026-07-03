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
 * CoAP Sensor — client
 *
 * Joins the sensor network and polls GET sensor/temp every 10 seconds.
 *
 * The client only needs the network key to discover and attach to the Leader.
 * Channel, PAN ID, and Extended PAN ID are learned from the server at attach
 * time (same pattern as SimpleThreadNetwork/RouterNode).
 */

#include <Arduino.h>
#include "OThread.h"
#include "OThreadCoAP.h"

// Must match sensor_server.ino
const uint8_t NETKEY[OT_NETWORK_KEY_SIZE] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0, 0x00};

const uint32_t ATTACH_TIMEOUT_MS = 20000;
const uint32_t ATTACH_DOT_MS = 2000;
const uint32_t POLL_MS = 10000;

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
      Serial.println("Started as Leader — is sensor_server running first?");
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
  Serial.printf("Attached as %s. Sensor server: %s\n", OThread.otGetStringDeviceRole(), serverIp.toString().c_str());
  return true;
}

void setup() {
  Serial.begin(115200);
  Serial.println("=== CoAP Sensor — client ===");

  OThread.begin(false);
  CoapClient.setTimeout(3000);
  // Non-confirmable GET (CLI: default without "con") — no ACK handshake.
  CoapClient.setConfirmable(false);

  if (!joinNetwork()) {
    Serial.println("Attach failed.");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("Ready. Polling GET sensor/temp every 10 s");
}

void loop() {
  if (OThread.otGetDeviceRole() < OT_ROLE_CHILD) {
    delay(1000);
    return;
  }

  int code = CoapClient.GET(serverIp, "sensor/temp");
  if (code >= 0) {
    Serial.printf("Temperature: %s C (code %d)\n", CoapClient.getString().c_str(), code);
  } else {
    Serial.printf("GET failed: %s\n", OThreadCoAP::errorToString(code));
  }

  delay(POLL_MS);
}
