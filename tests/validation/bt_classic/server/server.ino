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
 * BT Classic SPP slave for multi-DUT validation (pairs with client/).
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
static String serverName;

static bool readNameFromHost() {
  Serial.println("[SERVER] Ready for name");
  Serial.println("[SERVER] Send name:");
  while (serverName.length() == 0) {
    if (Serial.available()) {
      serverName = Serial.readStringUntil('\n');
      serverName.trim();
    }
    delay(50);
  }
  Serial.printf("[SERVER] Name: %s\n", serverName.c_str());
  return true;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  if (!readNameFromHost()) {
    return;
  }

  if (!SerialBT.begin(serverName)) {
    Serial.println("[SERVER] ERROR: begin failed");
    return;
  }

  Serial.println("[SERVER] SPP listening");

  const unsigned long connectTimeoutMs = 90000;
  unsigned long start = millis();
  while (!SerialBT.hasClient() && (millis() - start) < connectTimeoutMs) {
    delay(100);
  }

  if (!SerialBT.hasClient()) {
    Serial.println("[SERVER] ERROR: client timeout");
    return;
  }

  Serial.println("[SERVER] Client connected");

  start = millis();
  while (!SerialBT.available() && (millis() - start) < 30000) {
    delay(50);
  }

  if (!SerialBT.available()) {
    Serial.println("[SERVER] ERROR: no data from client");
    return;
  }

  String msg = SerialBT.readStringUntil('\n');
  msg.trim();
  Serial.printf("[SERVER] Received: %s\n", msg.c_str());

  if (msg != "BT_CLASSIC_PING") {
    Serial.println("[SERVER] ERROR: unexpected payload");
    return;
  }

  SerialBT.println("BT_CLASSIC_PONG");
  Serial.println("[SERVER] Sent response");
  Serial.println("[SERVER] Test complete");
}

void loop() {}
