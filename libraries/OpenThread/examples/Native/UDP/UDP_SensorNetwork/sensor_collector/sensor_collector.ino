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
 * sensor_collector - Thread Leader + Commissioner + UDP sink.
 *
 * This node forms a Thread network and receives sensor frames from many
 * children over UDP. Each accepted frame is printed to Serial and ACKed
 * back to the sender.
 */

#include <Arduino.h>
#include "OThread.h"
#include "OThreadUDP.h"

// ---------------- Thread network parameters ----------------
const char PSKD[] = "J01NME";
const uint8_t OT_CHANNEL = 15;
const uint16_t OT_PAN_ID = 0xABCE;
const uint8_t OT_EXTPANID[OT_EXT_PAN_ID_SIZE] = {0xCE, 0x01, 0x00, 0x00, 0xDE, 0xAD, 0xBE, 0xEF};
const uint8_t OT_NETKEY[OT_NETWORK_KEY_SIZE] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0, 0x01};
const char OT_NETWORK_NAME[] = "ESP_OT_SENSOR_NET";
const uint32_t JOINER_WINDOW_SEC = 3600;  // Keep join window open for 1 hour.

// ---------------- UDP + application parameters ----------------
// Use an application port that does not collide with OpenThread internals.
// 61631 is reserved by Thread TMF CoAP; binding an application socket there may
// receive binary management traffic that looks like malformed sensor packets.
const uint16_t COLLECTOR_PORT = 5050;
const uint16_t MAX_SENSORS = 256;
const uint32_t REPORT_PERIOD_MS = 30000;
const uint32_t NODE_OFFLINE_MS = 95000;               // ~3 missed 30 s samples
const uint32_t NODE_EVICT_MS = 30UL * 60UL * 1000UL;  // free slots after 30 min

OThreadUDP OtUdp;

struct SensorRecord {
  bool used;
  bool online;
  char nodeId[16];
  uint32_t lastSeq;
  int32_t lastTempCenti;
  uint16_t lastBatteryMv;
  uint32_t packetCount;
  uint32_t duplicateCount;
  uint32_t lastSeenMs;
  IPAddress lastIp;
};

static SensorRecord s_records[MAX_SENSORS];
static bool s_commissionerStarted = false;
static uint32_t s_totalPackets = 0;
static uint32_t s_droppedPackets = 0;

static int findRecordById(const char *id) {
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (s_records[i].used && !strcmp(s_records[i].nodeId, id)) {
      return i;
    }
  }
  return -1;
}

static int allocateRecord(const char *id) {
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (!s_records[i].used) {
      s_records[i] = SensorRecord{};
      s_records[i].used = true;
      strncpy(s_records[i].nodeId, id, sizeof(s_records[i].nodeId) - 1);
      s_records[i].nodeId[sizeof(s_records[i].nodeId) - 1] = '\0';
      return i;
    }
  }
  return -1;
}

static bool startThreadNetwork() {
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

  uint8_t tries = 24;
  while (tries-- && OThread.otGetDeviceRole() < OT_ROLE_CHILD) {
    Serial.print(".");
    delay(2500);
  }
  Serial.println();
  return OThread.otGetDeviceRole() >= OT_ROLE_CHILD;
}

static bool startCommissioner() {
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
  Serial.printf("Commissioner ACTIVE. PSKd \"%s\" open for %lu s.\n", PSKD, (unsigned long)JOINER_WINDOW_SEC);
  return true;
}

static void sendAck(const IPAddress &dstIp, uint16_t dstPort, const char *nodeId, uint32_t seq) {
  char ack[48];
  snprintf(ack, sizeof(ack), "OK,%s,%lu", nodeId, (unsigned long)seq);
  if (OtUdp.beginPacket(dstIp, dstPort)) {
    OtUdp.write((const uint8_t *)ack, strlen(ack));
    OtUdp.endPacket();
  }
}

static bool reopenUdpSocket() {
  OtUdp.stop();
  delay(50);
  return OtUdp.begin(COLLECTOR_PORT);
}

static bool restartThreadNetwork() {
  Serial.println("Collector detached; restarting Thread on the persisted dataset...");
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
  return reopenUdpSocket();
}

static void printCompactReport() {
  uint16_t known = 0;
  uint16_t online = 0;
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (!s_records[i].used) {
      continue;
    }
    known++;
    if (s_records[i].online) {
      online++;
    }
  }
  Serial.printf(
    "[collector] role=%s nodes=%u online=%u packets=%lu dropped=%lu\n", OThread.otGetStringDeviceRole(), known, online, (unsigned long)s_totalPackets,
    (unsigned long)s_droppedPackets
  );
}

static void auditNodes() {
  uint32_t now = millis();
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (!s_records[i].used) {
      continue;
    }
    uint32_t age = now - s_records[i].lastSeenMs;
    if (s_records[i].online && age > NODE_OFFLINE_MS) {
      s_records[i].online = false;
      Serial.printf("node %s -> OFFLINE (silent %lus)\n", s_records[i].nodeId, (unsigned long)(age / 1000));
    }
    if (age > NODE_EVICT_MS) {
      Serial.printf("node %s evicted from table (silent %lus)\n", s_records[i].nodeId, (unsigned long)(age / 1000));
      s_records[i].used = false;
    }
  }
}

static void processPacket(const char *payload, const IPAddress &srcIp, uint16_t srcPort) {
  // Wire format:
  // id=<node>,seq=<u32>,temp_centi=<i32>,batt_mv=<u16>
  char id[16] = {0};
  unsigned long seqTmp = 0;
  long tempTmp = 0;
  unsigned int battTmp = 0;
  if (sscanf(payload, "id=%15[^,],seq=%lu,temp_centi=%ld,batt_mv=%u", id, &seqTmp, &tempTmp, &battTmp) != 4) {
    s_droppedPackets++;
    Serial.printf("DROP malformed from [%s]:%u -> '%s'\n", srcIp.toString().c_str(), srcPort, payload);
    return;
  }
  uint32_t seq = (uint32_t)seqTmp;
  int32_t tempCenti = (int32_t)tempTmp;
  uint16_t battMv = (uint16_t)battTmp;

  int idx = findRecordById(id);
  if (idx < 0) {
    idx = allocateRecord(id);
    if (idx < 0) {
      s_droppedPackets++;
      Serial.printf("DROP table full (MAX_SENSORS=%u) from [%s]\n", (unsigned)MAX_SENSORS, srcIp.toString().c_str());
      return;
    }
  }

  SensorRecord &r = s_records[idx];
  bool isDuplicate = false;
  bool isRestart = false;
  bool isStale = false;
  bool acceptReading = false;

  if (r.packetCount == 0) {
    acceptReading = true;
  } else if (seq > r.lastSeq) {
    acceptReading = true;
  } else if (seq == r.lastSeq) {
    isDuplicate = true;
  } else if (seq == 1 && r.lastSeq > 1) {
    // The same node restarted and began its application sequence again.
    // Treat this as a fresh stream instead of permanently rejecting it as old.
    isRestart = true;
    acceptReading = true;
    r.duplicateCount = 0;
  } else {
    // Old/out-of-order frame. Do not overwrite the latest reading, but ACK the
    // collector's authoritative sequence so the node can resynchronise.
    isStale = true;
    r.duplicateCount++;
  }

  if (acceptReading) {
    r.lastSeq = seq;
    r.lastTempCenti = tempCenti;
    r.lastBatteryMv = battMv;
  }
  r.packetCount++;
  r.lastSeenMs = millis();
  r.lastIp = srcIp;
  s_totalPackets++;

  if (!r.online) {
    r.online = true;
    Serial.printf("node %s -> ONLINE\n", r.nodeId);
  }

  Serial.printf(
    "RX node=%s seq=%lu temp=%.2fC batt=%umV from [%s]:%u%s%s%s\n", r.nodeId, (unsigned long)seq, (float)tempCenti / 100.0f, battMv, srcIp.toString().c_str(),
    srcPort, isDuplicate ? " (dup)" : "", isRestart ? " (node restarted)" : "", isStale ? " (stale ignored)" : ""
  );

  // ACK the collector's stored sequence, not necessarily the received one. This
  // makes the collector authoritative and lets a node roll forward/back to the
  // last sequence the collector accepted.
  sendAck(srcIp, srcPort, r.nodeId, r.lastSeq);
}

void setup() {
  Serial.begin(115200);

  Serial.println("=== sensor_collector: Leader + UDP collector ===");
  if (!startThreadNetwork()) {
    Serial.println("Could not attach to Thread network. Halting.");
    while (1) {
      delay(1000);
    }
  }

  Serial.printf("Attached as %s\n", OThread.otGetStringDeviceRole());
  Serial.printf("Mesh-Local EID: %s\n", OThread.getMeshLocalEid().toString().c_str());
  Serial.printf("Leader RLOC:    %s\n", OThread.getLeaderRloc().toString().c_str());

  s_commissionerStarted = startCommissioner();

  if (!OtUdp.begin(COLLECTOR_PORT)) {
    Serial.println("OtUdp.begin failed. Halting.");
    while (1) {
      delay(1000);
    }
  }
  Serial.printf("Collector listening on UDP port %u\n", COLLECTOR_PORT);
}

void loop() {
  if (!s_commissionerStarted && OThread.otGetDeviceRole() >= OT_ROLE_CHILD) {
    s_commissionerStarted = startCommissioner();
    if (!s_commissionerStarted) {
      delay(5000);
    }
  }

  while (int n = OtUdp.parsePacket()) {
    char buf[160];
    int got = OtUdp.read(buf, (n < (int)sizeof(buf) - 1) ? n : (int)sizeof(buf) - 1);
    buf[got] = '\0';
    processPacket(buf, OtUdp.remoteIP(), OtUdp.remotePort());
  }

  static uint32_t lastReport = 0;
  if (millis() - lastReport >= REPORT_PERIOD_MS) {
    lastReport = millis();
    auditNodes();
    printCompactReport();
  }

  static uint32_t lastRoleCheck = 0;
  if (millis() - lastRoleCheck >= 5000) {
    lastRoleCheck = millis();
    if (OThread.otGetDeviceRole() < OT_ROLE_CHILD && !restartThreadNetwork()) {
      Serial.println("Thread/UDP restart failed.");
    }
  }

  delay(20);
}
