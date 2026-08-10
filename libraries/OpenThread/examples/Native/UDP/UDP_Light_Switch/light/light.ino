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
 * UDP Light - server side of a UDP-based Thread IoT light demo.
 *
 *   1. Brings up the Thread stack and commits a local Operational DataSet.
 *   2. Becomes the network Leader.
 *   3. Petitions the Commissioner role and authorizes any joiner that uses
 *      the shared PSKd. Companion switch sketches can then attach
 *      without knowing the network key.
 *   4. Joins the realm-local multicast group ff03::abcd and listens on
 *      UDP port LIGHT_PORT for "ON" / "OFF" / "TOGGLE" commands coming
 *      from any number of switches on the mesh.
 *   5. For every recognized command it updates the on-board RGB LED
 *      ("lamp") and sends an "ACK ON" / "ACK OFF" packet back to the
 *      sender, giving the switch a confirmation of the new lamp state.
 *
 * Pair this sketch with one or more switch.ino nodes.
 */

#include <Arduino.h>
#include "OThread.h"
#include "OThreadUDP.h"

// -------------------- Thread network configuration --------------------
// Pre-Shared Key for Device: every joiner must present this string.
const char PSKD[] = "J01NME";
// How long the joiner entry stays alive on the commissioner. Switches that
// power up within this window can be commissioned without further action.
const uint32_t JOINER_WINDOW_SEC = 600;  // 10 min

// Hard-coded operational dataset so subsequent power cycles always join the
// same Thread network and switches can find the light again.
const uint8_t OT_CHANNEL = 15;
const uint16_t OT_PAN_ID = 0xABCD;
const uint8_t OT_EXTPANID[OT_EXT_PAN_ID_SIZE] = {0xDE, 0xAD, 0x00, 0xBE, 0xEF, 0x00, 0xCA, 0xFE};
const uint8_t OT_NETKEY[OT_NETWORK_KEY_SIZE] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
const char OT_NETWORK_NAME[] = "ESP_OT_UDP_IoT";

// -------------------- UDP IoT configuration ----------------------------
// Realm-local multicast group that the light subscribes to. Switches send
// commands to this group + LIGHT_PORT so that they do not need to know
// the light's individual IPv6 address. ff03::/16 is mesh-wide on Thread.
const uint8_t LIGHT_GROUP_BYTES[16] = {0xff, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xab, 0xcd};
const IPAddress LIGHT_GROUP(IPv6, LIGHT_GROUP_BYTES);
// Use an application port that does not collide with OpenThread internals.
// 5683/5684 are CoAP/CoAPs ports and 61631 is Thread TMF CoAP.
const uint16_t LIGHT_PORT = 5051;

OThreadUDP OtUdp;

// -------------------- Lamp state ---------------------------------------
bool lampOn = false;
bool commissionerStarted = false;
bool receivedSwitchPacket = false;

// Track the current intensity so we can fade smoothly between transitions
// instead of snapping the LED on and off.
static uint8_t s_currentLevel = 0;

static void showConnectionLed(bool connected) {
  s_currentLevel = 0;
  if (connected) {
    rgbLedWrite(RGB_BUILTIN, 0, 64, 0);  // GREEN = connected, waiting for first switch command
  } else {
    rgbLedWrite(RGB_BUILTIN, 64, 0, 0);  // RED = not connected / setup failed
  }
}

static void fadeTo(uint8_t target) {
  if (s_currentLevel == target) {
    rgbLedWrite(RGB_BUILTIN, target, target, target);
    return;
  }
  int8_t step = (target > s_currentLevel) ? +1 : -1;
  while (s_currentLevel != target) {
    s_currentLevel = (uint8_t)((int)s_currentLevel + step);
    rgbLedWrite(RGB_BUILTIN, s_currentLevel, s_currentLevel, s_currentLevel);
    delay(2);
  }
}

static void applyLamp(bool on) {
  lampOn = on;
  if (on) {
    fadeTo(248);
  } else {
    fadeTo(0);
  }
}

static void replyAck(IPAddress to, uint16_t port) {
  const char *resp = lampOn ? "ACK ON" : "ACK OFF";
  OtUdp.beginPacket(to, port);
  OtUdp.write((const uint8_t *)resp, strlen(resp));
  OtUdp.endPacket();
}

static bool openUdpSocket() {
  OtUdp.stop();
  delay(50);
  // beginMulticast() subscribes to LIGHT_GROUP and also accepts unicast packets
  // to our addresses on LIGHT_PORT.
  return OtUdp.beginMulticast(LIGHT_GROUP, LIGHT_PORT);
}

static void networkSetup() {
  Serial.println("=== light: Bringing up Thread network ===");
  OThread.begin(false);

  if (OThread.hasActiveDataset()) {
    Serial.println("Active dataset found in NVS; resuming existing network.");
  } else {
    Serial.println("No active dataset; provisioning a new network.");
    DataSet ds;
    ds.initNew();
    ds.setNetworkName(OT_NETWORK_NAME);
    ds.setChannel(OT_CHANNEL);
    ds.setPanId(OT_PAN_ID);
    ds.setExtendedPanId(OT_EXTPANID);
    ds.setNetworkKey(OT_NETKEY);
    OThread.commitDataSet(ds);
  }

  OThread.networkInterfaceUp();
  OThread.start();

  Serial.println("Thread started, waiting for attached role...");
  uint8_t tries = 24;  // 24 x 2.5 s = 1 min
  while (tries && OThread.otGetDeviceRole() < OT_ROLE_CHILD) {
    Serial.print(".");
    delay(2500);
    tries--;
  }
  Serial.println();
  if (!tries) {
    Serial.println("Could not attach to Thread network. Halting.");
    showConnectionLed(false);
    while (1) {
      delay(1000);
    }
  }
  Serial.printf("Attached as %s.\n", OThread.otGetStringDeviceRole());
}

static bool commissionerSetup() {
  Serial.println("Petitioning Commissioner...");
  otError err = OThread.startCommissioner();
  if (err != OT_ERROR_NONE) {
    Serial.printf("startCommissioner failed: %d\n", err);
    return false;
  }
  err = OThread.addJoiner(PSKD, JOINER_WINDOW_SEC);
  if (err != OT_ERROR_NONE) {
    Serial.printf("addJoiner failed: %d\n", err);
    OThread.stopCommissioner();
    return false;
  }
  Serial.printf("Commissioner ACTIVE - PSKd \"%s\" accepted for %lu s.\n", PSKD, (unsigned long)JOINER_WINDOW_SEC);
  Serial.println("Flash switch sketches now.");
  return true;
}

void setup() {
  Serial.begin(115200);
  showConnectionLed(false);

  networkSetup();
  commissionerStarted = commissionerSetup();

  if (!openUdpSocket()) {
    Serial.println("UDP beginMulticast failed!");
    while (1) {
      delay(1000);
    }
  }
  Serial.printf("Listening on [%s]:%u (and unicast)\n", LIGHT_GROUP.toString().c_str(), LIGHT_PORT);
  Serial.printf("Mesh-Local EID: %s\n", OThread.getMeshLocalEid().toString().c_str());

  // Ready and waiting for the first switch command.
  showConnectionLed(true);
}

static bool restartThreadNetwork() {
  Serial.println("Light detached; restarting Thread on the persisted dataset...");
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

  if (OThread.otGetDeviceRole() < OT_ROLE_CHILD) {
    return false;
  }
  commissionerStarted = false;
  if (!openUdpSocket()) {
    return false;
  }
  lampOn = false;
  receivedSwitchPacket = false;
  showConnectionLed(true);
  return true;
}

void loop() {
  // If the commissioner could not be started during setup() because we were
  // not yet attached, keep retrying until it succeeds.
  if (!commissionerStarted && OThread.otGetDeviceRole() >= OT_ROLE_CHILD) {
    commissionerStarted = commissionerSetup();
    if (!commissionerStarted) {
      delay(5000);
    }
  }

  // Drain pending UDP commands.
  while (int n = OtUdp.parsePacket()) {
    char buf[32];
    int got = OtUdp.read(buf, (n < (int)sizeof(buf) - 1) ? n : (int)sizeof(buf) - 1);
    buf[got] = '\0';
    IPAddress src = OtUdp.remoteIP();
    uint16_t sp = OtUdp.remotePort();

    Serial.printf("RX [%s]:%u -> '%s'\n", src.toString().c_str(), sp, buf);

    if (!strcmp(buf, "ON")) {
      receivedSwitchPacket = true;
      applyLamp(true);
    } else if (!strcmp(buf, "OFF")) {
      receivedSwitchPacket = true;
      applyLamp(false);
    } else if (!strcmp(buf, "TOGGLE")) {
      receivedSwitchPacket = true;
      applyLamp(!lampOn);
    } else if (!strcmp(buf, "STATUS")) {
      // Pure query: do not touch the lamp, just report state.
      if (!receivedSwitchPacket) {
        receivedSwitchPacket = true;
        applyLamp(lampOn);
      }
    } else {
      Serial.printf("Ignoring unknown command '%s'\n", buf);
      continue;  // no ACK for unknown commands
    }
    replyAck(src, sp);
  }

  // Periodic status print (every 10 s)
  static uint32_t lastPrint = 0;
  if (millis() - lastPrint > 10000) {
    lastPrint = millis();
    Serial.printf("Role=%s lamp=%s\n", OThread.otGetStringDeviceRole(), lampOn ? "ON" : "OFF");
  }

  static uint32_t lastRoleCheck = 0;
  if (millis() - lastRoleCheck >= 5000) {
    lastRoleCheck = millis();
    if (OThread.otGetDeviceRole() < OT_ROLE_CHILD) {
      if (!restartThreadNetwork()) {
        showConnectionLed(false);
        Serial.println("Thread/UDP restart failed.");
      }
    }
  }

  delay(20);
}
