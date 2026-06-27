// Copyright 2026 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/*
 * UDP Switch - client side of a UDP-based Thread IoT light demo.
 *
 *   1. Brings up the Thread stack but does NOT commit a local DataSet.
 *   2. Pre-sets the radio channel as a Joiner hint (faster commissioning).
 *   3. Runs the Joiner state machine and obtains the operational dataset
 *      from the light's Commissioner (PSKd "J01NME").
 *   4. Once joined, enables Thread and opens a UDP socket on LIGHT_PORT.
 *   5. On every BOOT-button press it sends "TOGGLE" to the realm-local
 *      light group ff03::abcd and waits up to ACK_TIMEOUT_MS for the
 *      light's "ACK ON" / "ACK OFF" confirmation.
 *
 * Multiple switch nodes can coexist on the same mesh and all of them
 * can drive the same light.
 */

#include <Arduino.h>
#include "OThread.h"
#include "OThreadUDP.h"

#ifndef USER_BUTTON
#define USER_BUTTON BOOT_PIN  // BOOT button GPIO provided by Arduino.h
#endif

// Must match the light's PSKD and channel.
const char PSKD[] = "J01NME";
const uint8_t CHANNEL = 15;

const uint8_t LIGHT_GROUP_BYTES[16] = {0xff, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xab, 0xcd};
const IPAddress LIGHT_GROUP(IPv6, LIGHT_GROUP_BYTES);
// Use an application port that does not collide with OpenThread internals.
// 5683/5684 are CoAP/CoAPs ports and 61631 is Thread TMF CoAP.
const uint16_t LIGHT_PORT = 5051;
const uint32_t ACK_TIMEOUT_MS = 1000;
const uint32_t JOIN_TIMEOUT_MS = 60000;
const uint32_t RESUME_ATTACH_TIMEOUT_MS = 30000;
const uint8_t REATTACH_AFTER_MISSED = 3;
const uint32_t DETACHED_REATTACH_MS = 15000;

OThreadUDP OtUdp;
static uint8_t s_consecutiveNoAck = 0;
static uint32_t s_lastAttachedMs = 0;

static void showConnectionLed(bool connected) {
  if (connected) {
    rgbLedWrite(RGB_BUILTIN, 0, 64, 0);  // GREEN = attached and ready
  } else {
    rgbLedWrite(RGB_BUILTIN, 64, 0, 0);  // RED = not connected / setup failed
  }
}

static bool isOwnAddress(const IPAddress &ip) {
  const size_t addressCount = OThread.getUnicastAddressCount();
  for (size_t i = 0; i < addressCount; i++) {
    if (OThread.getUnicastAddress(i) == ip) {
      return true;
    }
  }
  return false;
}

static bool waitForAttach(uint32_t timeoutMs, uint16_t stepMs) {
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    if (OThread.otGetDeviceRole() >= OT_ROLE_CHILD) {
      Serial.println();
      Serial.printf("Attached as %s.\n", OThread.otGetStringDeviceRole());
      s_lastAttachedMs = millis();
      return true;
    }
    Serial.print('.');
    delay(stepMs);
  }
  Serial.println();
  return false;
}

static bool resumeStoredDataset() {
  if (!OThread.hasActiveDataset()) {
    Serial.println("No stored Thread dataset; commissioning is required.");
    return false;
  }

  Serial.println("Stored Thread dataset found; resuming network without commissioning.");
  OThread.networkInterfaceUp();
  OThread.start();
  if (waitForAttach(RESUME_ATTACH_TIMEOUT_MS, 500)) {
    return true;
  }

  Serial.println("Stored dataset did not attach. Falling back to Joiner commissioning.");
  OThread.stop();
  delay(200);
  return false;
}

static bool runJoinerCommissioning() {
  // Optional but recommended: skip the channel scan during Joiner discovery.
  OThread.setChannel(CHANNEL);
  OThread.networkInterfaceUp();

  Serial.print("EUI-64 = ");
  Serial.println(OThread.getEui64());
  Serial.printf("Joining with PSKd \"%s\" (timeout %lu ms)...\n", PSKD, (unsigned long)JOIN_TIMEOUT_MS);

  uint8_t tries = 5;
  while (tries--) {
    otError err = OThread.startJoiner(PSKD, JOIN_TIMEOUT_MS);
    if (err == OT_ERROR_NONE) {
      Serial.println("Joiner: SUCCESS");
      OThread.start();
      if (waitForAttach(60000, 2500)) {
        return true;
      }
      Serial.println("Joined but did not attach. Retrying...");
      OThread.stop();
      delay(200);
      OThread.networkInterfaceUp();
      continue;
    }
    switch (err) {
      case OT_ERROR_SECURITY:         Serial.println("Joiner: PSKd mismatch."); break;
      case OT_ERROR_NOT_FOUND:        Serial.println("Joiner: no joinable network. Is the light running and accepting joiners?"); break;
      case OT_ERROR_RESPONSE_TIMEOUT: Serial.println("Joiner: commissioner did not respond in time."); break;
      default:                        Serial.printf("Joiner: error %d.\n", err); break;
    }
    delay(2000);
  }
  return false;
}

static bool joinThreadNetwork() {
  OThread.begin(false);
  if (resumeStoredDataset()) {
    return true;
  }
  return runJoinerCommissioning();
}

static void drainRx() {
  // Discard any stray packets sitting in the RX queue before a new TX so
  // the ACK we are about to wait for is not confused with old traffic.
  while (OtUdp.parsePacket() > 0) {
    while (OtUdp.available()) {
      OtUdp.read();
    }
  }
}

static bool reopenUdpSocket() {
  OtUdp.stop();
  delay(50);
  if (!OtUdp.begin(LIGHT_PORT)) {
    Serial.println("UDP begin failed after re-open.");
    return false;
  }
  return true;
}

static void forceReattach() {
  Serial.println("Light unreachable; forcing Thread re-attach...");
  showConnectionLed(false);
  OtUdp.stop();
  OThread.stop();
  delay(200);
  OThread.start();

  uint8_t tries = 20;
  while (tries-- && OThread.otGetDeviceRole() < OT_ROLE_CHILD) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println();

  if (OThread.otGetDeviceRole() >= OT_ROLE_CHILD) {
    s_lastAttachedMs = millis();
    Serial.printf("Reattached as %s.\n", OThread.otGetStringDeviceRole());
  }
  showConnectionLed(OThread.otGetDeviceRole() >= OT_ROLE_CHILD && reopenUdpSocket());
}

static bool sendCommand(const char *cmd) {
  drainRx();

  Serial.printf("TX [%s]:%u -> '%s'\n", LIGHT_GROUP.toString().c_str(), LIGHT_PORT, cmd);
  if (!OtUdp.beginPacket(LIGHT_GROUP, LIGHT_PORT)) {
    Serial.println("beginPacket failed");
    return false;
  }
  OtUdp.write((const uint8_t *)cmd, strlen(cmd));
  if (!OtUdp.endPacket()) {
    Serial.println("endPacket failed");
    return false;
  }

  // Wait for the light's ACK packet (unicast back to us). Do not retransmit
  // TOGGLE automatically: if the command arrived but the ACK was lost, a retry
  // would toggle the lamp a second time.
  uint32_t start = millis();
  while (millis() - start < ACK_TIMEOUT_MS) {
    int n = OtUdp.parsePacket();
    if (n > 0) {
      char buf[32];
      int got = OtUdp.read(buf, (n < (int)sizeof(buf) - 1) ? n : (int)sizeof(buf) - 1);
      buf[got] = '\0';
      IPAddress remote = OtUdp.remoteIP();
      Serial.printf("ACK from [%s]:%u <- '%s'\n", remote.toString().c_str(), OtUdp.remotePort(), buf);
      if (isOwnAddress(remote)) {
        Serial.println("Ignoring datagram from own address.");
        continue;
      }
      if (!strcmp(buf, "ACK ON") || !strcmp(buf, "ACK OFF")) {
        return true;
      }
    }
    delay(10);
  }
  Serial.println("No ACK within timeout.");
  return false;
}

static void checkButton() {
  static uint32_t lastPress = 0;
  if (millis() - lastPress < 500) {
    return;  // debounce
  }
  if (digitalRead(USER_BUTTON) != LOW) {
    return;
  }
  lastPress = millis();

  bool ok = sendCommand("TOGGLE");
  if (ok) {
    s_consecutiveNoAck = 0;
  } else {
    if (++s_consecutiveNoAck >= REATTACH_AFTER_MISSED) {
      forceReattach();
      s_consecutiveNoAck = 0;
    }
  }
}

void setup() {
  Serial.begin(115200);
  showConnectionLed(false);
  pinMode(USER_BUTTON, INPUT_PULLUP);

  Serial.println("=== switch: Thread Joiner + UDP client ===");
  if (!joinThreadNetwork()) {
    Serial.println("Could not join Thread network. Halting.");
    while (1) {
      delay(1000);
    }
  }

  // Bind UDP on the same port the light replies to. We do NOT subscribe
  // to ff03::abcd here because we are not a recipient of multicast
  // commands - the light's ACK comes back as unicast on this port.
  if (!OtUdp.begin(LIGHT_PORT)) {
    Serial.println("UDP begin failed!");
    while (1) {
      delay(1000);
    }
  }

  Serial.printf("UDP bound on port %u\n", LIGHT_PORT);
  Serial.printf("Mesh-Local EID: %s\n", OThread.getMeshLocalEid().toString().c_str());
  Serial.println("Press BOOT to toggle the light.");

  showConnectionLed(true);
}

void loop() {
  if (OThread.otGetDeviceRole() < OT_ROLE_CHILD) {
    showConnectionLed(false);
    Serial.printf("Role=%s, waiting to reattach...\n", OThread.otGetStringDeviceRole());
    if (millis() - s_lastAttachedMs > DETACHED_REATTACH_MS) {
      forceReattach();
    }
    delay(1500);
    return;
  }
  s_lastAttachedMs = millis();
  showConnectionLed((bool)OtUdp);

  checkButton();
  delay(10);
}
