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
 * CoAP Switch — client
 *
 * Joins the light's Thread network via Commissioner (PSKd J01NME), then toggles
 * the remote lamp with CoAP PUT on multicast path "Lamp".
 */

#include <Arduino.h>
#include "OThread.h"
#include "OThreadCoAP.h"

#ifndef USER_BUTTON
#define USER_BUTTON BOOT_PIN  // BOOT button GPIO provided by Arduino.h
#endif

const char PSKD[] = "J01NME";
const uint8_t CHANNEL_HINT = 15;
const uint32_t JOIN_TIMEOUT_MS = 60000;
const uint32_t ATTACH_TIMEOUT_MS = 30000;
const uint32_t ATTACH_DOT_MS = 2000;

const uint8_t LAMP_GROUP_BYTES[16] = {0xff, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xab, 0xcd};
const IPAddress LAMP_GROUP(IPv6, LAMP_GROUP_BYTES);

OThreadCoAPClient CoapClient;
static bool lampOn = false;

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

static bool joinViaCommissioner() {
  Serial.println("Joining Thread network (Joiner)...");
  OThread.setChannel(CHANNEL_HINT);
  OThread.networkInterfaceUp();

  Serial.printf("Commissioning with PSKd \"%s\"...\n", PSKD);
  otError err = OThread.startJoiner(PSKD, JOIN_TIMEOUT_MS);
  if (err != OT_ERROR_NONE) {
    Serial.printf("startJoiner failed: %d\n", err);
    return false;
  }
  OThread.start();

  Serial.print("Waiting for attach");
  if (!waitForAttach(ATTACH_TIMEOUT_MS, ATTACH_DOT_MS)) {
    Serial.println("Attach timeout.");
    return false;
  }
  Serial.printf("Attached as %s.\n", OThread.otGetStringDeviceRole());
  rgbLedWrite(RGB_BUILTIN, 0, 0, 64);
  return true;
}

static bool sendLampState(bool on) {
  const char *payload = on ? "1" : "0";
  // Multicast command: send a single non-confirmable PUT and return immediately.
  // The lamp(s) act on it but do not reply (RFC 7252 §8.2), so there is nothing
  // to wait for. The boolean only reports whether the datagram was queued for TX.
  bool queued = CoapClient.sendNonBlocking(LAMP_GROUP, OT_COAP_REQ_PUT, "Lamp", payload);
  Serial.printf("PUT Lamp=%s -> %s (multicast NON, fire-and-forget)\n", payload, queued ? "sent" : "send failed");
  return queued;
}

static void checkButton() {
  static uint32_t lastPress = 0;
  const uint32_t debounceMs = 500;

  if (digitalRead(USER_BUTTON) == HIGH) {
    return;
  }
  if (millis() - lastPress < debounceMs) {
    return;
  }
  lastPress = millis();

  lampOn = !lampOn;
  if (!sendLampState(lampOn)) {
    // Fire-and-forget multicast: a failure here means the datagram could not be
    // queued (e.g. not attached yet), not that a lamp was unreachable.
    rgbLedWrite(RGB_BUILTIN, 255, 0, 0);
    Serial.println("CoAP send failed — is the node attached?");
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(USER_BUTTON, INPUT_PULLUP);
  rgbLedWrite(RGB_BUILTIN, 64, 0, 0);

  Serial.println("=== CoAP Switch (client) ===");
  OThread.begin(false);
  // LAMP_GROUP is a multicast address. RFC 7252 §8.1 forbids confirmable
  // multicast and §8.2 discourages responding to it, so this sketch uses
  // sendNonBlocking() (a single NON request, no waiting). The lamps act on the
  // command without replying. To target one lamp and confirm it, use a blocking
  // unicast call instead, e.g. CoapClient.PUT(lampUnicastAddr, "Lamp", payload).

  while (!joinViaCommissioner()) {
    Serial.println("Join failed, retrying in 3s...");
    OThread.stop();
    delay(3000);
  }
  Serial.println("Ready. Press the button to toggle the lamp.");
}

void loop() {
  if (OThread.otGetDeviceRole() < OT_ROLE_CHILD) {
    rgbLedWrite(RGB_BUILTIN, 64, 0, 0);
    delay(1000);
    return;
  }
  checkButton();
  delay(20);
}
