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
 * CoAP Sensor — server
 *
 * Exposes GET sensor/temp with a text/plain temperature reading.
 *
 * Forms the Thread network with a fixed operational dataset (including
 * Extended PAN ID). Flash before sensor_client.ino.
 */

#include <Arduino.h>
#include "OThread.h"
#include "OThreadCoAP.h"

const uint8_t CHANNEL = 15;
const uint16_t PAN_ID = 0xBEEF;
const uint8_t EXTPANID[OT_EXT_PAN_ID_SIZE] = {0xBE, 0xEF, 0x00, 0x00, 0x53, 0xE5, 0xC0, 0x01};
const uint32_t ATTACH_TIMEOUT_MS = 20000;
const uint32_t ATTACH_DOT_MS = 2000;
const uint8_t NETKEY[OT_NETWORK_KEY_SIZE] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0, 0x00};

// Backing String for the serve()-bound resource. After begin(), only update it
// via OThreadCoAPServer.setResourceValue() — never assign it directly here, because the
// server reads it on the OpenThread task and a direct write from loop() would race.
static String temperature = "22.0";

static float readFakeSensorCelsius() {
  return 20.0f + (float)random(0, 800) / 100.0f;  // 20.00 .. 27.99 C
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
  ds.setNetworkName("ESP_OT_CoAP_Sensor");
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
  Serial.printf("Leader RLOC: %s\n", OThread.getLeaderRloc().toString().c_str());
  return true;
}

void setup() {
  Serial.begin(115200);
  randomSeed(esp_random());

  Serial.println("=== CoAP Sensor — server ===");
  OThread.begin(false);
  if (!startNetwork()) {
    Serial.println("Attach failed.");
    while (1) {
      delay(1000);
    }
  }

  OThreadCoAPServer.serve("sensor/temp", &temperature);
  if (!OThreadCoAPServer.begin()) {
    Serial.println("CoAP server failed.");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("Ready. Serving GET sensor/temp");
}

void loop() {
  // Build the reading locally and publish it through setResourceValue(), which
  // updates the bound String under the OpenThread lock (no race with the handler).
  String reading = String(readFakeSensorCelsius(), 2);
  OThreadCoAPServer.setResourceValue("sensor/temp", reading);
  delay(1000);
}
