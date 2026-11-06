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
 * sensor_node - Thread Joiner + sleepy sensor end-device + UDP client.
 *
 * The node joins through the collector's Commissioner, optionally enables
 * Sleepy End Device (SED) behavior, then periodically sends a compact sensor
 * frame over UDP to the Thread Leader.
 */

#include <Arduino.h>
#include "OThread.h"
#include "OThreadUDP.h"

// Must match collector side.
const char PSKD[] = "J01NME";
const uint8_t CHANNEL_HINT = 15;
// Keep this away from Thread reserved ports. 61631 is used by Thread TMF CoAP
// and can make application sockets receive binary management traffic.
const uint16_t COLLECTOR_PORT = 5050;
const uint32_t JOIN_TIMEOUT_MS = 60000;
const uint32_t RESUME_ATTACH_TIMEOUT_MS = 30000;

// App behavior.
const uint32_t SAMPLE_PERIOD_MS = 30000;  // Read + send every 30 s.
const uint32_t ACK_TIMEOUT_MS = 1200;
const uint8_t TX_RETRIES = 2;
const uint8_t REATTACH_AFTER_MISSED = 3;
const uint32_t DETACHED_REATTACH_MS = 15000;
const bool ENABLE_SLEEPY_END_DEV = true;

// Sleepy End Device settings.
const uint32_t SED_POLL_PERIOD_MS = 1000;
const uint32_t CHILD_TIMEOUT_SEC = 300;

OThreadUDP OtUdp;
static IPAddress s_collectorIp;
static char s_nodeId[16] = {0};
static uint32_t s_seq = 0;  // Last sequence confirmed by the collector.
static uint8_t s_consecutiveNoAck = 0;
static uint32_t s_lastAttachedMs = 0;

static void makeNodeId() {
  uint8_t eui[8] = {0};
  if (OThread.getEui64(eui)) {
    snprintf(s_nodeId, sizeof(s_nodeId), "%02X%02X%02X%02X", eui[4], eui[5], eui[6], eui[7]);
    return;
  }
  unsigned long macLow = (unsigned long)(ESP.getEfuseMac() & 0xFFFFFFFFULL);
  snprintf(s_nodeId, sizeof(s_nodeId), "%08lX", macLow);
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
  OThread.setChannel(CHANNEL_HINT);
  OThread.networkInterfaceUp();

  Serial.printf("Joiner EUI-64: %s\n", OThread.getEui64().c_str());
  Serial.printf("Joining with PSKd \"%s\"...\n", PSKD);
  for (uint8_t tries = 0; tries < 6; tries++) {
    otError err = OThread.startJoiner(PSKD, JOIN_TIMEOUT_MS);
    if (err == OT_ERROR_NONE) {
      OThread.start();
      if (waitForAttach(10000, 500)) {
        return true;
      }
      Serial.println("Joined but did not attach. Retrying...");
      OThread.stop();
      delay(200);
      OThread.networkInterfaceUp();
      continue;
    }
    Serial.printf("Join attempt %u failed: %d\n", (unsigned int)(tries + 1), err);
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

static void configureSleepyEndDevice() {
  if (!ENABLE_SLEEPY_END_DEV) {
    return;
  }
  otInstance *inst = OThread.getInstance();
  if (!inst) {
    Serial.println("OpenThread instance not available");
    return;
  }
  if (!esp_openthread_lock_acquire(portMAX_DELAY)) {
    Serial.println("Failed to acquire OpenThread lock");
    return;
  }
  otError e1 = otLinkSetPollPeriod(inst, SED_POLL_PERIOD_MS);
  otThreadSetChildTimeout(inst, CHILD_TIMEOUT_SEC);
  otError e2 = otLinkSetRxOnWhenIdle(inst, false);
  esp_openthread_lock_release();

  Serial.printf(
    "SED cfg poll=%lums childTimeout=%lus rxOnIdle=0 (errs poll=%d rx=%d)\n", (unsigned long)SED_POLL_PERIOD_MS, (unsigned long)CHILD_TIMEOUT_SEC, e1, e2
  );
}

static void readFakeSensor(int32_t &tempCenti, uint16_t &battMv) {
  // Replace this with your real sensor and battery ADC reads.
  tempCenti = 2200 + (int32_t)random(-450, 550);  // 17.5C .. 27.5C
  battMv = 3600 + (uint16_t)random(0, 400);       // 3.60V .. 4.00V
}

static long parseAckSeq(const char *payload) {
  char ackNode[16] = {0};
  unsigned long ackSeq = 0;
  if (sscanf(payload, "OK,%15[^,],%lu", ackNode, &ackSeq) != 2) {
    return -1;
  }
  if (strcmp(ackNode, s_nodeId)) {
    return -1;
  }
  return (long)ackSeq;
}

static void reopenUdpSocket() {
  OtUdp.stop();
  delay(50);
  if (!OtUdp.begin(COLLECTOR_PORT)) {
    Serial.println("OtUdp.begin failed after re-open.");
  }
}

static void forceReattach() {
  Serial.println("Collector unreachable; forcing Thread re-attach...");
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
    s_collectorIp = OThread.getLeaderRloc();
    Serial.printf("Reattached as %s. Collector (Leader RLOC): %s\n", OThread.otGetStringDeviceRole(), s_collectorIp.toString().c_str());
  }
  reopenUdpSocket();
}

static long sendFrameAndWaitAck(uint32_t seq, int32_t tempCenti, uint16_t battMv) {
  char frame[120];
  snprintf(frame, sizeof(frame), "id=%s,seq=%lu,temp_centi=%ld,batt_mv=%u", s_nodeId, (unsigned long)seq, (long)tempCenti, battMv);

  long bestAcked = -1;
  for (uint8_t attempt = 0; attempt <= TX_RETRIES; attempt++) {
    // Drain stale packets before sending this attempt.
    while (OtUdp.parsePacket() > 0) {
      while (OtUdp.available()) {
        OtUdp.read();
      }
    }

    if (!OtUdp.beginPacket(s_collectorIp, COLLECTOR_PORT)) {
      Serial.println("beginPacket failed");
      continue;
    }
    OtUdp.write((const uint8_t *)frame, strlen(frame));
    if (!OtUdp.endPacket()) {
      Serial.println("endPacket failed");
      continue;
    }

    Serial.printf("TX try=%u [%s]:%u -> '%s'\n", (unsigned int)(attempt + 1), s_collectorIp.toString().c_str(), COLLECTOR_PORT, frame);

    uint32_t started = millis();
    while (millis() - started < ACK_TIMEOUT_MS) {
      int n = OtUdp.parsePacket();
      if (n > 0) {
        char ack[64];
        int got = OtUdp.read(ack, (n < (int)sizeof(ack) - 1) ? n : (int)sizeof(ack) - 1);
        ack[got] = '\0';
        Serial.printf("ACK [%s]:%u <- '%s'\n", OtUdp.remoteIP().toString().c_str(), OtUdp.remotePort(), ack);
        if (OtUdp.remoteIP() != s_collectorIp) {
          Serial.println("Ignoring ACK from unexpected source.");
          continue;
        }
        long ackedSeq = parseAckSeq(ack);
        if (ackedSeq < 0) {
          continue;
        }
        if (ackedSeq == (long)seq) {
          return ackedSeq;
        }
        if (ackedSeq > bestAcked) {
          bestAcked = ackedSeq;
        }
      }
      delay(20);
    }
  }
  Serial.println("ACK timeout");
  return bestAcked;
}

void setup() {
  Serial.begin(115200);

  randomSeed(esp_random());

  Serial.println("=== sensor_node: Joiner + sleepy UDP sensor ===");
  if (!joinThreadNetwork()) {
    Serial.println("Unable to join Thread network. Halting.");
    while (1) {
      delay(1000);
    }
  }

  configureSleepyEndDevice();
  makeNodeId();

  s_collectorIp = OThread.getLeaderRloc();
  s_lastAttachedMs = millis();
  Serial.printf("Role: %s\n", OThread.otGetStringDeviceRole());
  Serial.printf("Node ID: %s\n", s_nodeId);
  Serial.printf("Collector (Leader RLOC): %s\n", s_collectorIp.toString().c_str());

  // Use a fixed source port for easier troubleshooting.
  if (!OtUdp.begin(COLLECTOR_PORT)) {
    Serial.println("OtUdp.begin failed. Halting.");
    while (1) {
      delay(1000);
    }
  }
}

void loop() {
  if (OThread.otGetDeviceRole() < OT_ROLE_CHILD) {
    Serial.printf("Role=%s, waiting to reattach...\n", OThread.otGetStringDeviceRole());
    if (millis() - s_lastAttachedMs > DETACHED_REATTACH_MS) {
      forceReattach();
    }
    delay(1500);
    return;
  }
  s_lastAttachedMs = millis();

  int32_t tempCenti;
  uint16_t battMv;
  readFakeSensor(tempCenti, battMv);

  uint32_t seqToSend = s_seq + 1;
  long ackedSeq = sendFrameAndWaitAck(seqToSend, tempCenti, battMv);
  bool ok = (ackedSeq >= 0);
  if (ok) {
    s_seq = (uint32_t)ackedSeq;
  }
  Serial.printf(
    "sample=%lu temp=%.2fC batt=%umV status=%s\n", (unsigned long)seqToSend, (float)tempCenti / 100.0f, battMv,
    ok ? ((ackedSeq == (long)seqToSend) ? "ACKED" : "RESYNC") : "NO_ACK"
  );

  if (ok) {
    s_consecutiveNoAck = 0;
  } else {
    s_consecutiveNoAck++;
    Serial.printf("Holding sequence at %lu after %u missed ACK(s).\n", (unsigned long)s_seq, (unsigned int)s_consecutiveNoAck);
    if (s_consecutiveNoAck >= REATTACH_AFTER_MISSED) {
      forceReattach();
      s_consecutiveNoAck = 0;
    }
  }

  // For SED this is application idle time; the stack keeps low-power polling.
  delay(SAMPLE_PERIOD_MS);
}
