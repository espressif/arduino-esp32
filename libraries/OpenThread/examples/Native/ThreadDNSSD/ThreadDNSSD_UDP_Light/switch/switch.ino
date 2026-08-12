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
 * Thread DNSSD UDP Light — switch
 *
 * Discovers _otlight._udp via OThreadDNSSD.queryService. BOOT sends TOGGLE
 * then STATUS to the resolved unicast address/port.
 *
 * See ../README.md for the full lab.
 */

#include <Arduino.h>
#include "OThread.h"
#include "OThreadDNSSD.h"
#include "OThreadUDP.h"

#ifndef USER_BUTTON
#define USER_BUTTON BOOT_PIN
#endif

static const uint8_t OT_NETKEY[OT_NETWORK_KEY_SIZE] = {
  0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff
};

static const uint16_t LIGHT_PORT_FALLBACK = 5051;
static const uint32_t ACK_TIMEOUT_MS = 2000;
static const uint32_t REDISCOVER_MS = 15000;

OThreadUDP OtUdp;

static IPAddress s_lightAddr(IPv6);
static uint16_t s_lightPort = LIGHT_PORT_FALLBACK;
static bool s_haveLight = false;
static bool s_ready = false;
static uint32_t s_lastDiscoverMs = 0;

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

static bool isEmptyV6(const IPAddress &addr) {
  for (int i = 0; i < 16; ++i) {
    if (addr[i] != 0) {
      return false;
    }
  }
  return true;
}

static bool discoverLight() {
  Serial.println("queryService(\"otlight\", \"udp\")...");
  int n = OThreadDNSSD.queryService("otlight", "udp");
  Serial.printf("Found %d instance(s) (lastError=%d)\r\n", n, (int)OThreadDNSSD.lastError());
  s_lastDiscoverMs = millis();
  if (n <= 0) {
    s_haveLight = false;
    return false;
  }
  s_lightAddr = OThreadDNSSD.address(0);
  s_lightPort = OThreadDNSSD.port(0);
  if (s_lightPort == 0) {
    s_lightPort = LIGHT_PORT_FALLBACK;
  }
  if (isEmptyV6(s_lightAddr)) {
    Serial.println("FAIL: empty address in browse result");
    s_haveLight = false;
    return false;
  }
  s_haveLight = true;
  Serial.printf(
    "Light: instance=%s host=%s [%s]:%u\r\n", OThreadDNSSD.instanceName(0), OThreadDNSSD.hostname(0),
    s_lightAddr.toString().c_str(), s_lightPort
  );
  return true;
}

static void drainRx() {
  while (OtUdp.parsePacket() > 0) {
    while (OtUdp.available()) {
      OtUdp.read();
    }
  }
}

static bool sendAndWait(const char *cmd, char *respOut, size_t respSize) {
  if (!s_haveLight) {
    return false;
  }
  drainRx();
  Serial.printf("TX [%s]:%u -> '%s'\r\n", s_lightAddr.toString().c_str(), s_lightPort, cmd);
  if (!OtUdp.beginPacket(s_lightAddr, s_lightPort)) {
    Serial.println("beginPacket failed");
    return false;
  }
  OtUdp.write((const uint8_t *)cmd, strlen(cmd));
  if (!OtUdp.endPacket()) {
    Serial.println("endPacket failed");
    return false;
  }

  uint32_t start = millis();
  while (millis() - start < ACK_TIMEOUT_MS) {
    int n = OtUdp.parsePacket();
    if (n > 0) {
      int got = OtUdp.read(respOut, (n < (int)respSize - 1) ? n : (int)respSize - 1);
      respOut[got] = '\0';
      Serial.printf("RX '%s'\r\n", respOut);
      return true;
    }
    delay(10);
  }
  Serial.println("No reply (timeout)");
  return false;
}

static void onBootPress() {
  if (!s_haveLight && !discoverLight()) {
    Serial.println("No light yet — is light announced?");
    return;
  }

  char resp[32];
  if (!sendAndWait("TOGGLE", resp, sizeof(resp))) {
    s_haveLight = false;
    (void)discoverLight();
    return;
  }
  if (!sendAndWait("STATUS", resp, sizeof(resp))) {
    Serial.println("TOGGLE ok but STATUS failed");
  }
}

static void checkButton() {
  static uint32_t lastPress = 0;
  if (millis() - lastPress < 400) {
    return;
  }
  if (digitalRead(USER_BUTTON) != LOW) {
    return;
  }
  lastPress = millis();
  onBootPress();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("ThreadDNSSD_UDP_Light / switch");
  pinMode(USER_BUTTON, INPUT_PULLUP);

  OThread.begin(false);
  DataSet ds;
  ds.clear();
  ds.setNetworkKey(OT_NETKEY);
  OThread.commitDataSet(ds);
  OThread.networkInterfaceUp();
  OThread.start();

  Serial.println("Waiting to attach...");
  if (!waitAttached(60000)) {
    Serial.println("FAIL: not attached (check Network Key vs OTBR)");
    return;
  }
  Serial.printf("Attached as %s\r\n", OThread.otGetStringDeviceRole());

  if (!OThreadDNSSD.begin("ot-switch")) {
    Serial.println("FAIL: OThreadDNSSD.begin");
    return;
  }
  delay(2000);

  if (!OtUdp.begin(LIGHT_PORT_FALLBACK)) {
    Serial.println("FAIL: UDP begin");
    return;
  }

  (void)discoverLight();
  Serial.println("Press BOOT to TOGGLE + STATUS");
  s_ready = true;
}

void loop() {
  if (!s_ready) {
    delay(1000);
    return;
  }

  checkButton();

  if (!s_haveLight && (millis() - s_lastDiscoverMs >= REDISCOVER_MS)) {
    (void)discoverLight();
  }

  delay(10);
}
