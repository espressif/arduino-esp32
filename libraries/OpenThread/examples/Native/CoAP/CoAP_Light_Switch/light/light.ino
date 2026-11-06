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
 * CoAP Light — server
 *
 * Thread Leader + Commissioner + CoAP resource "Lamp".
 * PUT payload "0" or "1" turns the RGB LED off/on.
 * GET returns current state as "0" or "1".
 *
 * Pair with switch/switch.ino (Joiner + CoAP client).
 */

#include <Arduino.h>
#include "OThread.h"
#include "OThreadCoAP.h"

const char PSKD[] = "J01NME";
const uint32_t JOINER_WINDOW_SEC = 600;
const uint8_t CHANNEL = 15;
const uint16_t PAN_ID = 0xABCD;
const uint8_t EXTPANID[OT_EXT_PAN_ID_SIZE] = {0xDE, 0xAD, 0x00, 0xBE, 0xEF, 0x00, 0xCA, 0xFE};
const uint8_t NETKEY[OT_NETWORK_KEY_SIZE] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
const char NETWORK_NAME[] = "ESP_OT_CoAP_Lamp";

const uint32_t ATTACH_TIMEOUT_MS = 30000;
const uint32_t ATTACH_DOT_MS = 2000;

// Realm-local multicast — switches can PUT here without knowing unicast address
const uint8_t LAMP_GROUP_BYTES[16] = {0xff, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xab, 0xcd};
const IPAddress LAMP_GROUP(IPv6, LAMP_GROUP_BYTES);

// lampTarget is the commanded state, written by the CoAP handler on the
// OpenThread task. lampShown is what the LED currently displays, owned by loop()
// on the Arduino task. The handler only flips the target (cheap); the slow LED
// ramp runs in loop(), so the OpenThread stack is never blocked by the animation.
static volatile bool lampTarget = false;
static bool lampShown = false;
static bool commissionerStarted = false;

// Runs from loop() on the Arduino task, where delay() is safe. NEVER call this
// from a CoAP handler — it would stall the OpenThread task for the whole ramp.
static void animateLamp(bool on) {
  if (on) {
    for (int16_t level = 16; level < 248; level += 8) {
      rgbLedWrite(RGB_BUILTIN, level, level, level);
      delay(5);
    }
    rgbLedWrite(RGB_BUILTIN, 255, 255, 255);
  } else {
    for (int16_t level = 248; level > 16; level -= 8) {
      rgbLedWrite(RGB_BUILTIN, level, level, level);
      delay(5);
    }
    rgbLedWrite(RGB_BUILTIN, 0, 0, 0);
  }
  lampShown = on;
}

static void onLamp(OThreadCoAPRequest &req, OThreadCoAPResponse &resp, void *ctx) {
  (void)ctx;
  // Runs on the OpenThread task (CLI callback pattern). Keep it short: it only
  // records the requested state and returns — the LED animation happens in
  // loop(). Never call delay() or long work here, or the whole stack stalls.

  // A multicast command (the switch PUTs to the group address) is fire-and-forget:
  // act on it but do NOT reply, or every lamp in the group would answer the same
  // datagram at once (RFC 7252 §8.2). resp.send() is simply skipped for these;
  // the switch uses sendNonBlocking() and expects no response. Unicast requests
  // still get a normal reply below.
  const bool replyAllowed = !req.isMulticast();

  if (req.method() == OT_COAP_REQ_GET) {
    if (replyAllowed) {
      resp.setCode(OT_COAP_RESP_OK);
      resp.setPayload(lampTarget ? "1" : "0");
      resp.send();
    }
    return;
  }

  if (req.method() == OT_COAP_REQ_PUT) {
    String body = req.payloadString();
    char cmd = body.length() ? body.charAt(body.length() - 1) : '\0';
    if (cmd != '0' && cmd != '1') {
      if (replyAllowed) {
        resp.setCode(OT_COAP_RESP_BAD_REQUEST);
        resp.send();
      }
      return;
    }
    lampTarget = (cmd == '1');  // loop() picks this up and drives the LED
    Serial.printf(
      "CoAP PUT from %s -> %s%s\n", req.remoteIP().toString().c_str(), lampTarget ? "ON" : "OFF", req.isMulticast() ? " (multicast, no reply)" : ""
    );
    if (replyAllowed) {
      resp.setCode(OT_COAP_RESP_CHANGED);
      resp.setPayload(lampTarget ? "1" : "0");
      resp.send();
    }
    return;
  }

  if (replyAllowed) {
    resp.setCode(OT_COAP_RESP_METHOD_NA);
    resp.send();
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

static bool startNetwork() {
  Serial.println("Forming Thread network...");
  DataSet ds;
  ds.initNew();
  ds.setNetworkName(NETWORK_NAME);
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

  if (!commissionerStarted) {
    Serial.println("Starting Commissioner...");
    if (OThread.startCommissioner() == OT_ERROR_NONE) {
      OThread.addJoiner(PSKD, JOINER_WINDOW_SEC);
      commissionerStarted = true;
      Serial.printf("Commissioner ready (PSKd \"%s\")\n", PSKD);
    } else {
      Serial.println("Commissioner start failed — joiners cannot attach until a Commissioner is active.");
    }
  }

  rgbLedWrite(RGB_BUILTIN, 0, 64, 0);
  return true;
}

void setup() {
  Serial.begin(115200);
  rgbLedWrite(RGB_BUILTIN, 64, 0, 0);

  Serial.println("=== CoAP Light (server) ===");
  OThread.begin(false);

  while (!startNetwork()) {
    Serial.println("Network setup failed, retrying in 2s...");
    OThread.stop();
    delay(2000);
  }

  Serial.println("Starting CoAP server...");
  OThreadCoAPServer.on("Lamp", OT_COAP_METHOD_GET | OT_COAP_METHOD_PUT, onLamp);
  if (!OThreadCoAPServer.begin()) {
    Serial.println("CoAP server failed.");
    while (1) {
      delay(1000);
    }
  }

  // Subscribe the interface to the lamp group; without this the node never
  // receives datagrams addressed to the custom realm-local group ff03::abcd.
  if (!OThreadCoAPServer.joinMulticastGroup(LAMP_GROUP)) {
    Serial.println("Failed to join multicast group.");
    while (1) {
      delay(1000);
    }
  }

  Serial.printf("Ready. CoAP resource 'Lamp' (multicast group %s)\n", LAMP_GROUP.toString().c_str());
  rgbLedWrite(RGB_BUILTIN, 0, 0, 0);  // start with the lamp off
  lampTarget = false;
  lampShown = false;
}

void loop() {
  // onLamp() runs on the OpenThread task and only sets lampTarget. Do the slow
  // LED animation here (Arduino task, delay() is safe) and only when it changed.
  if (lampTarget != lampShown) {
    animateLamp(lampTarget);
  }
  delay(10);
}
