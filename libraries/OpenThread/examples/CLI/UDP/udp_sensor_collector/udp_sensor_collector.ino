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
 * udp_sensor_collector (CLI) - Leader + UDP telemetry sink.
 *
 * This is the central node of a many-to-one telemetry demo. It:
 *   1. Forms a Thread network and attaches (Leader in a fresh partition),
 *      entirely through OpenThread CLI commands (no Native OThreadUDP object).
 *   2. Opens a CLI UDP socket and subscribes to a realm-local multicast group.
 *   3. Receives sensor frames that the CLI prints asynchronously on its
 *      output stream, parses them, and unicasts an ACK back to each sender.
 *
 * Everything here is driven by the OpenThread CLI through the OThreadCLI
 * Arduino Stream, so the sketch can run on builds where the Native UDP API
 * is not available. Pair it with udp_sensor_node.ino.
 */

#include <Arduino.h>
#include "OThreadCLI.h"       // OThreadCLI object (CLI as an Arduino Stream)
#include "OThreadCLI_Util.h"  // otExecCommand() / ot_cmd_return_t helpers

// CLI dataset values. Keep these aligned with udp_sensor_node.
// Thread network name is limited to 16 bytes (OT_NETWORK_NAME_MAX_SIZE); a
// longer name makes "dataset networkname" fail with InvalidArgs.
#define OT_NETWORK_NAME "ESP_OT_SENSORS"
#define OT_CHANNEL      "15"
#define OT_PANID        "0xabcf"
#define OT_EXTPANID     "ce010000deadc0de"
#define OT_NETWORK_KEY  "102030405060708090a0b0c0d0e0f002"
// Fixed mesh-local prefix so a re-provisioned/rebooted leader forms the exact
// same network instead of the random prefix "dataset init new" would generate.
#define OT_MESHLOCAL_PREFIX "fdde:ad00:beef:0::"

const uint16_t COLLECTOR_PORT = 61631;        // UDP port we bind/listen on
const char COLLECTOR_GROUP[] = "ff03::abcd";  // realm-local multicast group sensors send to
const int MAX_SENSORS = 256;                  // capacity of the in-RAM node table
const uint32_t REPORT_PERIOD_MS = 30000;      // how often the fleet summary is printed

// Liveness tracking. A node that goes silent (lost power, crashed, moved out of
// range) must eventually be recognized as gone, otherwise the collector keeps
// reporting it forever. NODE_OFFLINE_MS should be a small multiple of the
// sensor's sample period so a couple of missed reports flips it to OFFLINE;
// after NODE_EVICT_MS with no contact the table slot is freed entirely.
const uint32_t NODE_OFFLINE_MS = 95000;               // ~3 missed 30 s samples
const uint32_t NODE_EVICT_MS = 30UL * 60UL * 1000UL;  // drop dead nodes after 30 min

// One entry per unique sensor node id seen on the network. The table is a
// simple fixed array (no dynamic allocation) so memory use is bounded and
// predictable even with a few hundred nodes.
struct SensorRecord {
  bool used;
  bool online;  // false once the node has been silent past NODE_OFFLINE_MS
  char nodeId[16];
  uint32_t lastSeq;
  int32_t lastTempCenti;
  uint16_t lastBatteryMv;
  uint32_t packetCount;
  uint32_t duplicateCount;
  uint32_t lastSeenMs;
  char lastIp[40];
};

static SensorRecord s_records[MAX_SENSORS];
static uint32_t s_totalPackets = 0;
static uint32_t s_droppedPackets = 0;
static char s_cliLine[256];

// Run a single CLI command and check its result. otExecCommand() sends the
// command and waits for the terminating "Done"/"Error ..." line; ot_cmd_return_t
// carries the parsed error code/message so we can log a useful failure.
static bool otCliCmd(const char *cmd) {
  ot_cmd_return_t rc;
  // otExecCommand() uses Stream::readBytesUntil(), which depends on the current
  // Stream timeout. RX parsing tweaks the timeout frequently, so force a
  // known-safe value for CLI control commands.
  OThreadCLI.setTimeout(1000);
  bool ok = otExecCommand(cmd, nullptr, &rc);
  if (!ok) {
    Serial.printf("CLI ERROR: '%s' -> %d %s\n", cmd, rc.errorCode, rc.errorMessage.c_str());
  }
  return ok;
}

// Read one line of asynchronous CLI output. OThreadCLI is an Arduino Stream,
// so unsolicited output (e.g. incoming UDP datagrams) can be read with the
// usual Stream API. Returns true only when a non-empty line was captured.
static bool readCliLine(char *line, size_t lineLen, uint32_t timeoutMs) {
  if (lineLen < 2) {
    return false;
  }
  OThreadCLI.setTimeout(timeoutMs);
  size_t n = OThreadCLI.readBytesUntil('\n', line, lineLen - 1);
  if (!n) {
    line[0] = '\0';
    return false;
  }
  line[n] = '\0';
  for (size_t i = 0; i < n; i++) {
    if (line[i] == '\r' || line[i] == '\n') {
      line[i] = '\0';
      break;
    }
  }
  return strlen(line) > 0;
}

// The CLI prints a received datagram as:
//   "<N> bytes from <srcIp> <srcPort> <payload>"
// e.g. "21 bytes from fdde:ad00:beef:0:1234:5678:9abc:def0 49152 id=node-1,seq=5,..."
// Pull out the source IP, source port, and the payload text. Returns false for
// any line that is not in this exact UDP-receive shape.
static bool parseUdpLine(const char *line, char *srcIp, size_t srcIpLen, uint16_t &srcPort, char *payload, size_t payloadLen) {
  unsigned int bytes = 0;
  unsigned int port = 0;
  char ip[40] = {0};
  char msg[160] = {0};
  int m = sscanf(line, "%u bytes from %39s %u %159[^\r\n]", &bytes, ip, &port, msg);
  if (m != 4) {
    return false;
  }
  strncpy(srcIp, ip, srcIpLen - 1);
  srcIp[srcIpLen - 1] = '\0';
  srcPort = (uint16_t)port;
  strncpy(payload, msg, payloadLen - 1);
  payload[payloadLen - 1] = '\0';
  return true;
}

// Linear lookup of an existing node record by its string id (-1 if not found).
static int findRecordById(const char *id) {
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (s_records[i].used && !strcmp(s_records[i].nodeId, id)) {
      return i;
    }
  }
  return -1;
}

// Claim the first free slot for a newly seen node id (-1 if the table is full).
static int allocateRecord(const char *id) {
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (!s_records[i].used) {
      memset(&s_records[i], 0, sizeof(s_records[i]));
      s_records[i].used = true;
      strncpy(s_records[i].nodeId, id, sizeof(s_records[i].nodeId) - 1);
      return i;
    }
  }
  return -1;
}

// Bring up Thread purely through the CLI.
//
// Reboot recovery (issue: children must rejoin after the leader resets):
// OpenThread persists the committed dataset in NVS. If one already exists we
// RESUME it - just bring the interface up and start Thread - which restores the
// SAME network (same mesh-local prefix and key/frame-counter state). A child
// that lost its parent then re-attaches to the returning leader within seconds.
//
// Only on the very first boot (no stored dataset) do we provision one, and we
// PIN the mesh-local prefix and active timestamp. Plain "dataset init new"
// randomizes both, so without pinning every reboot would look like a brand-new
// network and force a slow, churny re-attach across the whole fleet.
//
// NOTE: because the dataset is persisted, changing the OT_* constants above has
// no effect until the stored dataset is cleared (CLI "factoryreset" or an erase
// of flash). The dataset MUST match udp_sensor_node.
static bool setupThreadByCli() {
  bool datasetPresent = OThread.hasActiveDataset();
  if (datasetPresent) {
    Serial.println("Active dataset found in NVS; resuming existing network.");
    if (!otCliCmd("ifconfig up") || !otCliCmd("thread start")) {
      return false;
    }
  } else {
    Serial.println("No active dataset; provisioning a new deterministic network.");
    const char *cmds[] = {
      "thread stop",
      "ifconfig down",
      "dataset clear",
      "dataset init new",
      "dataset networkname " OT_NETWORK_NAME,
      "dataset channel " OT_CHANNEL,
      "dataset panid " OT_PANID,
      "dataset extpanid " OT_EXTPANID,
      "dataset networkkey " OT_NETWORK_KEY,
      // Pin the fields "dataset init new" would otherwise randomize so the
      // network is identical every time it is (re)created.
      "dataset meshlocalprefix " OT_MESHLOCAL_PREFIX,
      "dataset activetimestamp 1",
      "dataset commit active",
      "ifconfig up",
      "thread start",
    };
    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
      if (!otCliCmd(cmds[i])) {
        return false;
      }
    }
  }

  // Attaching takes a few seconds. Poll the device role until it is at least
  // a Child (Child/Router/Leader all count as "attached"); give up after ~60s.
  uint8_t tries = 30;
  while (tries-- && OThread.otGetDeviceRole() < OT_ROLE_CHILD) {
    Serial.print(".");
    delay(2000);
  }
  Serial.println();
  return OThread.otGetDeviceRole() >= OT_ROLE_CHILD;
}

// Open the CLI UDP socket and start listening. "ipmaddr add" subscribes the
// node to the realm-local multicast group the sensors target, and "udp bind ::
// <port>" binds the socket to our listen port on every address. After this the
// CLI will asynchronously print each datagram it receives.
static bool setupUdpCli() {
  // COLLECTOR_GROUP/COLLECTOR_PORT are runtime variables, so build the CLI
  // command strings with snprintf (string-literal concatenation only works
  // with #define macros, not with const char[] variables).
  char mcastCmd[64];
  char bindCmd[48];
  snprintf(mcastCmd, sizeof(mcastCmd), "ipmaddr add %s", COLLECTOR_GROUP);
  snprintf(bindCmd, sizeof(bindCmd), "udp bind :: %u", COLLECTOR_PORT);
  const char *cmds[] = {
    "udp close",  // ensure a clean socket state before (re)opening
    "udp open",
    mcastCmd,  // subscribe to the multicast group sensors publish to
    bindCmd,   // bind to COLLECTOR_PORT so incoming frames are delivered
  };
  for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
    if (!otCliCmd(cmds[i])) {
      return false;
    }
  }
  return true;
}

// Unicast an "OK,<nodeId>,<seq>" acknowledgment straight back to the sender's
// address/port. The node uses this ACK to confirm delivery (and, for sleepy
// nodes, as a cue that it can go back to sleep). We push the command directly
// on the CLI stream rather than otExecCommand() because we do not need to block
// on the "Done" response here.
static void sendAck(const char *dstIp, uint16_t dstPort, const char *nodeId, uint32_t seq) {
  char ack[48];
  char cmd[128];
  snprintf(ack, sizeof(ack), "OK,%s,%lu", nodeId, (unsigned long)seq);
  snprintf(cmd, sizeof(cmd), "udp send %s %u %s", dstIp, dstPort, ack);
  OThreadCLI.println(cmd);
}

// Age out nodes we have stopped hearing from. A node still in range but silent
// past NODE_OFFLINE_MS is flipped to OFFLINE (and reported once), and a node
// gone longer than NODE_EVICT_MS is removed from the table so its slot can be
// reused and it stops being counted. This is what lets the collector "sense" a
// sensor that lost power instead of claiming it is still present.
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
    "[collector-cli] role=%s nodes=%u online=%u packets=%lu dropped=%lu\n", OThread.otGetStringDeviceRole(), known, online, (unsigned long)s_totalPackets,
    (unsigned long)s_droppedPackets
  );
}

// Decode one sensor frame, update (or create) its node record, and ACK it.
// The frame format is the plain-text key/value string the node builds:
//   "id=<str>,seq=<n>,temp_centi=<int>,batt_mv=<uint>"
static void processPacket(const char *payload, const char *srcIp, uint16_t srcPort) {
  char id[16] = {0};
  unsigned long seqTmp = 0;
  long tempTmp = 0;
  unsigned int battTmp = 0;
  if (sscanf(payload, "id=%15[^,],seq=%lu,temp_centi=%ld,batt_mv=%u", id, &seqTmp, &tempTmp, &battTmp) != 4) {
    s_droppedPackets++;
    Serial.printf("DROP malformed from [%s]:%u -> '%s'\n", srcIp, srcPort, payload);
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
      Serial.printf("DROP table full (MAX_SENSORS=%u)\n", MAX_SENSORS);
      return;
    }
  }

  // A retransmitted frame (node resent because it missed our ACK) will carry a
  // sequence number we have already recorded. Count it but do not overwrite the
  // stored reading, so duplicates never corrupt the latest values.
  SensorRecord &r = s_records[idx];
  bool isRebootSeq = (r.packetCount > 0 && seq < r.lastSeq && seq <= 3);
  bool isDuplicate = (r.packetCount > 0 && !isRebootSeq && seq <= r.lastSeq);
  if (isDuplicate) {
    r.duplicateCount++;
  } else {
    r.lastSeq = seq;
    r.lastTempCenti = tempCenti;
    r.lastBatteryMv = battMv;
  }
  r.packetCount++;
  r.lastSeenMs = millis();
  strncpy(r.lastIp, srcIp, sizeof(r.lastIp) - 1);
  r.lastIp[sizeof(r.lastIp) - 1] = '\0';
  s_totalPackets++;

  // Report the moment a node (re)appears: either brand new, or one we had
  // marked OFFLINE that has come back (e.g. after the node or collector reboot).
  if (!r.online) {
    r.online = true;
    Serial.printf("node %s -> ONLINE\n", r.nodeId);
  }

  Serial.printf(
    "RX node=%s seq=%lu temp=%.2fC batt=%umV from [%s]:%u%s\n", r.nodeId, (unsigned long)seq, (float)tempCenti / 100.0f, battMv, srcIp, srcPort,
    isDuplicate ? " (dup)" : ""
  );

  sendAck(srcIp, srcPort, r.nodeId, seq);
}

void setup() {
  Serial.begin(115200);

  Serial.println("=== udp_sensor_collector (CLI): Leader + UDP sink ===");
  // begin(false): start the OpenThread stack but do NOT auto-start a network -
  // we form the network ourselves through the CLI below. begin() returns only
  // after stack init has finished (and reloaded any persisted dataset from NVS),
  // so OThread.hasActiveDataset() is reliable without any extra wait here.
  OThread.begin(false);
  OThreadCLI.begin();
  OThreadCLI.setTimeout(250);

  if (!setupThreadByCli()) {
    Serial.println("Thread setup failed. Halting.");
    while (1) {
      delay(1000);
    }
  }

  Serial.printf("Attached as %s\n", OThread.otGetStringDeviceRole());
  // Network we ended up on: this is either the dataset we just provisioned or the
  // one resumed from NVS on a subsequent boot.
  Serial.printf("Network:        name=%s panid=0x%04x\n", OThread.getNetworkName().c_str(), OThread.getPanId());
  Serial.printf("Mesh-Local EID: %s\n", OThread.getMeshLocalEid().toString().c_str());
  Serial.printf("Leader RLOC:    %s\n", OThread.getLeaderRloc().toString().c_str());

  if (!setupUdpCli()) {
    Serial.println("UDP CLI setup failed. Halting.");
    while (1) {
      delay(1000);
    }
  }
  Serial.printf("Collector listening on UDP %s:%u\n", COLLECTOR_GROUP, COLLECTOR_PORT);
}

void loop() {
  // Drain whatever the CLI has emitted since last loop. Each readable line is
  // either CLI bookkeeping ("Done"/"Error", skipped) or an incoming datagram.
  while (readCliLine(s_cliLine, sizeof(s_cliLine), 20)) {
    if (!strncmp(s_cliLine, "Done", 4) || !strncmp(s_cliLine, "Error", 5)) {
      continue;
    }
    char srcIp[40] = {0};
    uint16_t srcPort = 0;
    char payload[160] = {0};
    if (parseUdpLine(s_cliLine, srcIp, sizeof(srcIp), srcPort, payload, sizeof(payload))) {
      processPacket(payload, srcIp, srcPort);
    }
  }

  // Periodically age the table and print a summary. After a collector reboot
  // the table starts empty and is rebuilt automatically from the frames sensors
  // keep multicasting, so no persisted state is needed to recover.
  static uint32_t lastReport = 0;
  if (millis() - lastReport >= REPORT_PERIOD_MS) {
    lastReport = millis();
    auditNodes();
    printCompactReport();
  }

  // Thread role watchdog: as the network's only infrastructure node the
  // collector should stay Leader/Router. If it ever drops to detached/child
  // (e.g. a transient fault), bring Thread back up so sensors have a parent.
  // setupThreadByCli() resumes the persisted dataset, so this restores the same
  // network rather than creating a new one.
  static uint32_t lastRoleCheck = 0;
  if (millis() - lastRoleCheck >= 5000) {
    lastRoleCheck = millis();
    if (OThread.otGetDeviceRole() < OT_ROLE_CHILD) {
      Serial.println("Collector detached; restarting Thread...");
      if (!setupThreadByCli()) {
        Serial.println("Thread restart failed.");
      } else if (!setupUdpCli()) {
        Serial.println("UDP CLI setup failed after Thread restart.");
      }
    }
  }

  delay(20);
}
