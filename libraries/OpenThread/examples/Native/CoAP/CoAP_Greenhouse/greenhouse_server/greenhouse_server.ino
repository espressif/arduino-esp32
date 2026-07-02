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
 * CoAP Greenhouse — server
 *
 * Dual-transport IoT gateway on one device:
 *   Plain CoAP (5683)  — read-only telemetry (accepts CON and NON GET)
 *   CoAPS (5684)       — actuator commands (PUT valve/water, PUT fan/speed)
 *
 * Pair with greenhouse_client/greenhouse_client.ino.
 *
 * BUILD: CoAPS requires OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE=1
 */

#include <Arduino.h>
#include "OThread.h"
#include "OThreadCoAP.h"

const char PSKD[] = "J01NME";
const uint32_t JOINER_WINDOW_SEC = 600;
const uint8_t CHANNEL = 15;
const uint16_t PAN_ID = 0xBEE5;
const uint8_t NETKEY[OT_NETWORK_KEY_SIZE] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0, 0x05};

const uint32_t ATTACH_TIMEOUT_MS = 30000;
const uint32_t ATTACH_DOT_MS = 2000;

static const uint8_t COAP_PSK[] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0, 0x00};
static const char COAP_PSK_ID[] = "esp-coap-demo";

struct GreenhouseState {
  float tempC;
  float lightLux;
  uint8_t valvePercent;
  uint8_t fanSpeed;
};

static GreenhouseState s_state = {24.0f, 12000.0f, 0, 0};

static void logRequestKind(const char *path, OThreadCoAPRequest &req) {
  Serial.printf("[%s] %s from %s\n", req.isConfirmable() ? "CON" : "NON", path, req.remoteIP().toString().c_str());
}

static void onGreenhouseTemp(OThreadCoAPRequest &req, OThreadCoAPResponse &resp, void *ctx) {
  auto *state = static_cast<GreenhouseState *>(ctx);
  if (req.method() != OT_COAP_REQ_GET) {
    resp.setCode(OT_COAP_RESP_METHOD_NA);
    resp.send();
    return;
  }
  logRequestKind("GET greenhouse/temp", req);
  resp.setCode(OT_COAP_RESP_OK);
  String payload = String(state->tempC, 1);
  resp.setPayload(payload.c_str());
  resp.send();
}

static void onGreenhouseLight(OThreadCoAPRequest &req, OThreadCoAPResponse &resp, void *ctx) {
  auto *state = static_cast<GreenhouseState *>(ctx);
  if (req.method() != OT_COAP_REQ_GET) {
    resp.setCode(OT_COAP_RESP_METHOD_NA);
    resp.send();
    return;
  }
  logRequestKind("GET greenhouse/light", req);
  resp.setCode(OT_COAP_RESP_OK);
  String payload = String((int)state->lightLux);
  resp.setPayload(payload.c_str());
  resp.send();
}

static bool parsePercent(const String &payload, uint8_t *out) {
  int value = payload.toInt();
  if (value < 0 || value > 100) {
    return false;
  }
  *out = (uint8_t)value;
  return true;
}

static void onValveWater(OThreadCoAPRequest &req, OThreadCoAPResponse &resp, void *ctx) {
  auto *state = static_cast<GreenhouseState *>(ctx);
  if (req.method() != OT_COAP_REQ_PUT) {
    resp.setCode(OT_COAP_RESP_METHOD_NA);
    resp.send();
    return;
  }
  logRequestKind("CoAPS PUT valve/water", req);
  uint8_t percent = 0;
  if (!parsePercent(req.payloadString(), &percent)) {
    resp.setCode(OT_COAP_RESP_BAD_REQUEST);
    resp.setPayload("0-100");
    resp.send();
    return;
  }
  state->valvePercent = percent;
  Serial.printf("Valve -> %u%%\n", (unsigned)percent);
  resp.setCode(OT_COAP_RESP_CHANGED);
  String payload = String(percent);
  resp.setPayload(payload.c_str());
  resp.send();
}

static void onFanSpeed(OThreadCoAPRequest &req, OThreadCoAPResponse &resp, void *ctx) {
  auto *state = static_cast<GreenhouseState *>(ctx);
  if (req.method() != OT_COAP_REQ_PUT) {
    resp.setCode(OT_COAP_RESP_METHOD_NA);
    resp.send();
    return;
  }
  logRequestKind("CoAPS PUT fan/speed", req);
  uint8_t speed = 0;
  if (!parsePercent(req.payloadString(), &speed)) {
    resp.setCode(OT_COAP_RESP_BAD_REQUEST);
    resp.setPayload("0-100");
    resp.send();
    return;
  }
  state->fanSpeed = speed;
  Serial.printf("Fan -> %u%%\n", (unsigned)speed);
  resp.setCode(OT_COAP_RESP_CHANGED);
  String payload = String(speed);
  resp.setPayload(payload.c_str());
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
  ds.setNetworkName("ESP_OT_CoAP_Greenhouse");
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
  randomSeed(esp_random());

  Serial.println("=== CoAP Greenhouse — server ===");
  OThread.begin(false);
  while (!startNetwork()) {
    Serial.println("Network failed, retrying in 2s...");
    OThread.stop();
    delay(2000);
  }

  Serial.println("Starting CoAP servers...");
  OThreadCoAPServer.on("greenhouse/temp", OT_COAP_METHOD_GET, onGreenhouseTemp, &s_state);
  OThreadCoAPServer.on("greenhouse/light", OT_COAP_METHOD_GET, onGreenhouseLight, &s_state);
  if (!OThreadCoAPServer.begin()) {
    Serial.println("Plain CoAP server start failed.");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("Ready.");
  Serial.printf("Plain CoAP on port %u: GET greenhouse/temp, greenhouse/light\n", (unsigned)OT_COAP_DEFAULT_PORT);

  if (OThreadCoAP::secureApiEnabled()) {
    OThreadCoAPSecureServer.setPSK(COAP_PSK, sizeof(COAP_PSK), COAP_PSK_ID);
    OThreadCoAPSecureServer.on("valve/water", OT_COAP_METHOD_PUT, onValveWater, &s_state);
    OThreadCoAPSecureServer.on("fan/speed", OT_COAP_METHOD_PUT, onFanSpeed, &s_state);
    if (!OThreadCoAPSecureServer.begin()) {
      Serial.println("CoAPS server start failed.");
      while (1) {
        delay(1000);
      }
    }
    Serial.printf("CoAPS on port %u: PUT valve/water, fan/speed (PSK id \"%s\")\n", (unsigned)OT_COAP_SECURE_DEFAULT_PORT, COAP_PSK_ID);
  } else {
    Serial.println("CoAPS is not enabled in this build. Secure actuators will not run.");
    Serial.println("Plain CoAP telemetry continues.");
  }
  Serial.printf("Mesh-local: %s\n", OThread.getMeshLocalEid().toString().c_str());
}

void loop() {
  // Simulate slow environment drift; fan slightly cools the greenhouse.
  s_state.tempC += ((float)random(-5, 6)) / 10.0f;
  s_state.tempC -= (float)s_state.fanSpeed / 200.0f;
  if (s_state.tempC < 18.0f) {
    s_state.tempC = 18.0f;
  }
  if (s_state.tempC > 32.0f) {
    s_state.tempC = 32.0f;
  }

  s_state.lightLux += (float)random(-500, 501);
  if (s_state.lightLux < 2000.0f) {
    s_state.lightLux = 2000.0f;
  }
  if (s_state.lightLux > 20000.0f) {
    s_state.lightLux = 20000.0f;
  }

  delay(1000);
}
