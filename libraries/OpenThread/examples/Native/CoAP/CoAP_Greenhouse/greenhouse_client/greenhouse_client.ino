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
 * CoAP Greenhouse — client
 *
 * Dual-transport IoT controller on one device:
 *   Plain CoAP  — NON GET telemetry (greenhouse/temp, greenhouse/light)
 *   CoAPS       — CON PUT actuators (valve/water, fan/speed)
 *
 * Simple automation: if temp > 26 C, increase fan; if light < 8000 lux, open valve.
 *
 * Pair with greenhouse_server/greenhouse_server.ino.
 *
 * BUILD: CoAPS requires OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE=1
 */

#include <Arduino.h>
#include "OThread.h"
#include "OThreadCoAP.h"

const char PSKD[] = "J01NME";
const uint8_t CHANNEL_HINT = 15;
const uint32_t JOIN_TIMEOUT_MS = 60000;
const uint32_t ATTACH_TIMEOUT_MS = 30000;
const uint32_t ATTACH_DOT_MS = 2000;
const uint32_t TELEMETRY_MS = 8000;
const uint32_t CONTROL_MS = 24000;
const float TEMP_FAN_ON_C = 26.0f;
const float LIGHT_VALVE_LUX = 8000.0f;

static const uint8_t COAP_PSK[] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0, 0x00};
static const char COAP_PSK_ID[] = "esp-coap-demo";

OThreadCoAPClient PlainClient;
OThreadCoAPSecureClient SecureClient;

static IPAddress serverIp;
static uint32_t lastTelemetryMs = 0;
static uint32_t lastControlMs = 0;
static float lastTempC = 0.0f;
static float lastLightLux = 0.0f;

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
  Serial.printf("Attached as %s. Greenhouse server: %s\n", OThread.otGetStringDeviceRole(), serverIp.toString().c_str());
  return true;
}

static bool pollTelemetry() {
  PlainClient.setConfirmable(false);

  int code = PlainClient.GET(serverIp, "greenhouse/temp");
  if (code < 0) {
    Serial.printf("NON GET greenhouse/temp failed: %s\n", OThreadCoAP::errorToString(code));
    return false;
  }
  lastTempC = PlainClient.getString().toFloat();
  Serial.printf("[plain NON] temp=%.1f C\n", lastTempC);

  code = PlainClient.GET(serverIp, "greenhouse/light");
  if (code < 0) {
    Serial.printf("NON GET greenhouse/light failed: %s\n", OThreadCoAP::errorToString(code));
    return false;
  }
  lastLightLux = PlainClient.getString().toFloat();
  Serial.printf("[plain NON] light=%.0f lux\n", lastLightLux);
  return true;
}

static bool sendSecurePut(const char *path, uint8_t percent) {
  char payload[8];
  snprintf(payload, sizeof(payload), "%u", (unsigned)percent);
  int code = SecureClient.PUT(path, payload);
  if (code < 0) {
    Serial.printf("CON PUT %s failed: %s\n", path, OThreadCoAP::errorToString(code));
    return false;
  }
  Serial.printf("[CoAPS CON] PUT %s=%u -> %s\n", path, (unsigned)percent, OThreadCoAP::responseCodeToString(code));
  return true;
}

static bool runControlLoop() {
  if (!OThreadCoAP::secureApiEnabled()) {
    return false;
  }

  uint8_t fanSpeed = (lastTempC > TEMP_FAN_ON_C) ? 75 : 15;
  uint8_t valveOpen = (lastLightLux < LIGHT_VALVE_LUX) ? 60 : 10;

  Serial.printf("Control: temp=%.1f fan=%u%% light=%.0f valve=%u%%\n", lastTempC, (unsigned)fanSpeed, lastLightLux, (unsigned)valveOpen);

  if (!SecureClient.connect(serverIp)) {
    Serial.println("DTLS connect failed.");
    return false;
  }

  SecureClient.setConfirmable(true);
  bool ok = sendSecurePut("fan/speed", fanSpeed);
  ok = sendSecurePut("valve/water", valveOpen) && ok;

  SecureClient.disconnect();
  return ok;
}

void setup() {
  Serial.begin(115200);
  Serial.println("=== CoAP Greenhouse — client ===");

  OThread.begin(false);
  PlainClient.setTimeout(3000);
  PlainClient.setConfirmable(false);

  if (OThreadCoAP::secureApiEnabled()) {
    SecureClient.setPSK(COAP_PSK, sizeof(COAP_PSK), COAP_PSK_ID);
    SecureClient.setConnectTimeout(10000);
    SecureClient.setTimeout(5000);
    SecureClient.setConfirmable(true);
  } else {
    Serial.println("CoAPS is not enabled in this build. Secure control will not run.");
  }

  while (!joinNetwork()) {
    Serial.println("Join failed, retry in 3s...");
    OThread.stop();
    delay(3000);
  }

  Serial.println("Ready. Polling telemetry and running control loop.");
  lastTelemetryMs = millis();
  lastControlMs = millis();
}

void loop() {
  if (OThread.otGetDeviceRole() < OT_ROLE_CHILD) {
    delay(1000);
    return;
  }

  uint32_t now = millis();

  if (now - lastTelemetryMs >= TELEMETRY_MS) {
    pollTelemetry();
    lastTelemetryMs = now;
  }

  if (now - lastControlMs >= CONTROL_MS) {
    if (lastTempC > 0.0f && OThreadCoAP::secureApiEnabled()) {
      runControlLoop();
    }
    lastControlMs = now;
  }

  delay(100);
}
