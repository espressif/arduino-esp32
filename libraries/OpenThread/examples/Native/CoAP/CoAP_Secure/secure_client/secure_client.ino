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
 * CoAP Secure — client
 *
 * Joins via Commissioner, opens DTLS to the secure server, GET /status.
 */

#include <Arduino.h>
#include "OThread.h"
#include "OThreadCoAP.h"

const char PSKD[] = "J01NME";
const uint8_t CHANNEL_HINT = 15;
const uint32_t JOIN_TIMEOUT_MS = 60000;
const uint32_t ATTACH_TIMEOUT_MS = 30000;
const uint32_t ATTACH_DOT_MS = 2000;

static const uint8_t COAP_PSK[] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0, 0x00};
static const char COAP_PSK_ID[] = "esp-coap-demo";

OThreadCoAPSecureClient SecureClient;
static IPAddress serverIp;

static void onSecureEvent(int event, void *ctx) {
  (void)ctx;
  switch (event) {
    case OT_COAP_SECURE_CONNECTED:          Serial.println("DTLS connected"); break;
    case OT_COAP_SECURE_DISCONNECTED_PEER:  Serial.println("DTLS closed by peer"); break;
    case OT_COAP_SECURE_DISCONNECTED_ERROR: Serial.println("DTLS error"); break;
    default:                                Serial.printf("DTLS event %d\n", event); break;
  }
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

static bool joinNetwork() {
  Serial.println("Joining Thread network (Joiner)...");
  OThread.setChannel(CHANNEL_HINT);
  OThread.networkInterfaceUp();

  Serial.printf("Commissioning with PSKd \"%s\"...\n", PSKD);
  otError err = OThread.startJoiner(PSKD, JOIN_TIMEOUT_MS);
  if (err != OT_ERROR_NONE) {
    Serial.printf("Joiner failed: %d\n", err);
    return false;
  }
  OThread.start();

  Serial.print("Waiting for attach");
  if (!waitForAttach(ATTACH_TIMEOUT_MS, ATTACH_DOT_MS)) {
    Serial.println("Attach timeout.");
    return false;
  }
  serverIp = OThread.getLeaderRloc();
  Serial.printf("Attached as %s. Server: %s\n", OThread.otGetStringDeviceRole(), serverIp.toString().c_str());
  return true;
}

void setup() {
  Serial.begin(115200);
  Serial.println("=== CoAP Secure — client ===");

  if (!OThreadCoAP::secureApiEnabled()) {
    Serial.println("CoAPS is not enabled in this build. This sketch will not run.");
    while (1) {
      delay(1000);
    }
  }

  OThread.begin(false);
  SecureClient.setPSK(COAP_PSK, sizeof(COAP_PSK), COAP_PSK_ID);
  SecureClient.setConnectTimeout(10000);
  SecureClient.setTimeout(5000);
  SecureClient.setConfirmable(true);
  SecureClient.onConnectEvent(onSecureEvent);

  while (!joinNetwork()) {
    Serial.println("Join failed, retry in 3s...");
    OThread.stop();
    delay(3000);
  }

  delay(1000);

  Serial.println("Connecting DTLS...");
  if (!SecureClient.connect(serverIp)) {
    Serial.println("DTLS connect failed.");
    return;
  }

  int code = SecureClient.GET("status");
  Serial.printf("GET status -> %d (%s)\n", code, code >= 0 ? OThreadCoAP::responseCodeToString(code) : OThreadCoAP::errorToString(code));
  if (code >= 0) {
    Serial.printf("Payload: %s\n", SecureClient.getString().c_str());
  }

  SecureClient.disconnect();
}

void loop() {
  delay(1000);
}
