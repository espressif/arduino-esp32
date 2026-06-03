/*
 * Copyright 2026 Espressif Systems (Shanghai) PTE LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * BT Classic SPP master for multi-DUT validation (pairs with server/).
 */

#include <Arduino.h>
#include "BluetoothSerial.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled
#endif

#if !defined(CONFIG_BT_SPP_ENABLED)
#error BT Classic SPP is only available on ESP32
#endif

static BluetoothSerial SerialBT;
static String slaveName;
static const char *kMasterName = "BT_CLASSIC_MASTER";
static const char *kPing = "BT_CLASSIC_PING";
static const char *kPong = "BT_CLASSIC_PONG";

static bool readSlaveNameFromHost() {
  Serial.println("[CLIENT] Ready for slave name");
  Serial.println("[CLIENT] Send slave name:");
  while (slaveName.length() == 0) {
    if (Serial.available()) {
      slaveName = Serial.readStringUntil('\n');
      slaveName.trim();
    }
    delay(50);
  }
  Serial.printf("[CLIENT] Slave name: %s\n", slaveName.c_str());
  return true;
}

static bool waitForConnection() {
  Serial.printf("[CLIENT] Connecting to %s\n", slaveName.c_str());

  if (SerialBT.connect(slaveName)) {
    return true;
  }

  const unsigned long deadline = millis() + 90000;
  while (millis() < deadline) {
    if (SerialBT.connected(10000)) {
      return true;
    }
    Serial.println("[CLIENT] Waiting for slave...");
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  if (!readSlaveNameFromHost()) {
    return;
  }

  if (!SerialBT.begin(kMasterName, true)) {
    Serial.println("[CLIENT] ERROR: begin failed");
    return;
  }

  if (!waitForConnection()) {
    Serial.println("[CLIENT] ERROR: connect failed");
    return;
  }

  Serial.println("[CLIENT] Connected");

  SerialBT.println(kPing);

  const unsigned long deadline = millis() + 30000;
  while (!SerialBT.available() && millis() < deadline) {
    delay(50);
  }

  if (!SerialBT.available()) {
    Serial.println("[CLIENT] ERROR: no response from server");
    return;
  }

  String response = SerialBT.readStringUntil('\n');
  response.trim();
  Serial.printf("[CLIENT] Received: %s\n", response.c_str());

  if (response != kPong) {
    Serial.println("[CLIENT] ERROR: unexpected response");
    return;
  }

  Serial.println("[CLIENT] Test complete");
}

void loop() {}
