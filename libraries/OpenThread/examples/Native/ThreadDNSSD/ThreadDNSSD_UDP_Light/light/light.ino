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
 * Thread DNSSD UDP Light — server
 *
 * Joins OTBR with Network Key, advertises _otlight._udp via OThreadDNSSD,
 * and serves ON/OFF/TOGGLE/STATUS on UDP port LIGHT_PORT (board LED).
 *
 * See ../README.md for the full light + switch + WiFi web lab.
 */

#include <Arduino.h>
#include "OThread.h"
#include "OThreadDNSSD.h"
#include "OThreadUDP.h"

static const uint8_t OT_NETKEY[OT_NETWORK_KEY_SIZE] = {
  0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff
};

static const char *kHostName = "ot-light";
static const uint16_t LIGHT_PORT = 5051;

OThreadUDP OtUdp;

static bool lampOn = false;
static uint8_t s_currentLevel = 0;
static bool s_ready = false;
static bool s_announcedLogged = false;

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
  fadeTo(on ? 248 : 0);
}

static void reply(IPAddress to, uint16_t port, const char *resp) {
  OtUdp.beginPacket(to, port);
  OtUdp.write((const uint8_t *)resp, strlen(resp));
  OtUdp.endPacket();
}

static bool waitAttached(uint32_t timeoutMs) {
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    ot_device_role_t role = OThread.otGetDeviceRole();
    if (role == OT_ROLE_CHILD || role == OT_ROLE_ROUTER || role == OT_ROLE_LEADER) {
      return true;
    }
    delay(200);
  }
  return false;
}

// Returning from setup() still runs loop(); halt on attach / begin / UDP bind only.
static void halt(const char *msg) {
  Serial.println(msg);
  while (true) {
    delay(1000);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("ThreadDNSSD_UDP_Light / light");
  rgbLedWrite(RGB_BUILTIN, 64, 0, 0);

  OThread.begin(false);
  DataSet ds;
  ds.clear();
  ds.setNetworkKey(OT_NETKEY);
  OThread.commitDataSet(ds);
  OThread.networkInterfaceUp();
  OThread.start();

  Serial.println("Waiting to attach...");
  if (!waitAttached(60000)) {
    halt("FAIL: not attached (check Network Key vs OTBR)");
  }
  Serial.printf("Attached as %s\r\n", OThread.otGetStringDeviceRole());

  if (!OThreadDNSSD.begin(kHostName)) {
    halt("FAIL: OThreadDNSSD.begin");
  }
  if (!OThreadDNSSD.addService("otlight", "udp", LIGHT_PORT)) {
    halt("FAIL: addService");
  }
  (void)OThreadDNSSD.addServiceTxt("otlight", "udp", "cmds", "on,off,toggle,status");

  Serial.println("Waiting for SRP announce...");
  if (OThreadDNSSD.waitForAnnounce(60000)) {
    Serial.printf("PASS: announced as %s _otlight._udp:%u\r\n", OThreadDNSSD.hostname(), LIGHT_PORT);
    s_announcedLogged = true;
  } else {
    Serial.printf(
      "FAIL: announce timeout (lastError=%d) — starting UDP anyway; polling isAnnounceComplete()\r\n",
      (int)OThreadDNSSD.lastError()
    );
  }

  if (!OtUdp.begin(LIGHT_PORT)) {
    halt("FAIL: UDP begin");
  }
  Serial.printf("UDP listening on port %u (MLEID %s)\r\n", LIGHT_PORT, OThread.getMeshLocalEid().toString().c_str());
  applyLamp(false);
  s_ready = true;
}

void loop() {
  if (!s_ready) {
    delay(1000);
    return;
  }

  while (int n = OtUdp.parsePacket()) {
    char buf[32];
    int got = OtUdp.read(buf, (n < (int)sizeof(buf) - 1) ? n : (int)sizeof(buf) - 1);
    buf[got] = '\0';
    IPAddress src = OtUdp.remoteIP();
    uint16_t sp = OtUdp.remotePort();
    Serial.printf("RX [%s]:%u <- '%s'\r\n", src.toString().c_str(), sp, buf);

    if (!strcmp(buf, "ON")) {
      applyLamp(true);
      reply(src, sp, "ACK ON");
    } else if (!strcmp(buf, "OFF")) {
      applyLamp(false);
      reply(src, sp, "ACK OFF");
    } else if (!strcmp(buf, "TOGGLE")) {
      applyLamp(!lampOn);
      reply(src, sp, lampOn ? "ACK ON" : "ACK OFF");
    } else if (!strcmp(buf, "STATUS")) {
      reply(src, sp, lampOn ? "STATE ON" : "STATE OFF");
    } else {
      Serial.println("Ignoring unknown command");
    }
  }

  static uint32_t lastPrint = 0;
  if (!s_announcedLogged && OThreadDNSSD.isAnnounceComplete()) {
    s_announcedLogged = true;
    Serial.printf("PASS: announced as %s _otlight._udp:%u\r\n", OThreadDNSSD.hostname(), LIGHT_PORT);
  }
  if (millis() - lastPrint > 10000) {
    lastPrint = millis();
    Serial.printf(
      "role=%s announce=%d lamp=%s\r\n", OThread.otGetStringDeviceRole(), OThreadDNSSD.isAnnounceComplete(),
      lampOn ? "ON" : "OFF"
    );
  }
  delay(20);
}
