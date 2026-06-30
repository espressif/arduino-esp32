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
 * udp_sensor_node (CLI) - Sleepy UDP sensor child.
 *
 * One of potentially hundreds of leaf nodes in a many-to-one telemetry demo.
 * It:
 *   1. Joins the same Thread network as udp_sensor_collector using a hard-coded
 *      operational dataset, configured entirely through OpenThread CLI commands.
 *   2. Optionally becomes a Sleepy End Device (SED) - "mode n" + a poll period +
 *      child timeout - so the radio can sleep between polls to save power.
 *   3. Periodically samples a (fake) sensor, multicasts the reading over UDP via
 *      the CLI, and waits for the collector's unicast ACK before sleeping again.
 *
 * All transport goes through the OThreadCLI Arduino Stream, so no Native
 * OThreadUDP object is used. Pair it with udp_sensor_collector.ino.
 */

#include <Arduino.h>
#include <ctype.h>            // toupper() used when normalizing the node id
#include "OThreadCLI.h"       // OThreadCLI object (CLI as an Arduino Stream)
#include "OThreadCLI_Util.h"  // otExecCommand()/otGetRespCmd()/ot_cmd_return_t helpers
#include "esp_system.h"       // esp_random() for seeding the fake sensor

// CLI dataset values. Must match collector.
// Thread network name is limited to 16 bytes (OT_NETWORK_NAME_MAX_SIZE); a
// longer name makes "dataset networkname" fail with InvalidArgs. Must match collector.
#define OT_NETWORK_NAME "ESP_OT_SENSORS"
#define OT_CHANNEL      "15"
#define OT_PANID        "0xabcf"
#define OT_EXTPANID     "ce010000deadc0de"
#define OT_NETWORK_KEY  "102030405060708090a0b0c0d0e0f002"

// Destination/listen UDP port. MUST match the collector and MUST NOT be one of
// Thread's reserved UDP ports (61631 = TMF CoAP, 5683/5684 = CoAP/CoAPs,
// 19788 = MLE); binding those makes the app socket receive Thread's internal
// management traffic, which shows up at the collector as "DROP malformed".
const uint16_t COLLECTOR_PORT = 5050;         // destination/listen UDP port
const char COLLECTOR_GROUP[] = "ff03::abcd";  // multicast group the collector subscribes to
const uint32_t SAMPLE_PERIOD_MS = 30000;      // time between sensor samples
const uint32_t ACK_TIMEOUT_MS = 1200;         // how long to wait for the collector's ACK
const uint8_t TX_RETRIES = 2;                 // immediate resends of the SAME seq before giving up on this sample

// Recovery: if this many samples in a row get no ACK, assume the collector (our
// parent/leader) rebooted or vanished and force a clean Thread re-attach instead
// of waiting out the child timeout. Keep it >1 so an occasional lost ACK on a
// healthy network does not trigger needless re-attaches.
const uint8_t REATTACH_AFTER_MISSED = 3;

// Sleepy End Device setup via CLI.
const bool USE_SLEEPY_MODE = true;     // set false to stay a normal (always-on) child
const uint32_t SED_POLL_MS = 1000;     // data poll interval while sleeping (lower = lower latency, higher power)
const uint32_t CHILD_TIMEOUT_S = 300;  // parent keeps us as a child if we poll within this window

// Sleepy behavior in this sketch (mode n / pollperiod / childtimeout) is only
// meaningful when the build enables the required OT/802.15.4 low-power stack
// options. This is typically configured in Arduino-as-IDF-component projects.
#if defined(CONFIG_OPENTHREAD_MTD) && defined(CONFIG_IEEE802154_SLEEP_ENABLE) && defined(CONFIG_PM_ENABLE)
static constexpr bool kBuildHasSleepySupport = (CONFIG_OPENTHREAD_MTD && CONFIG_IEEE802154_SLEEP_ENABLE && CONFIG_PM_ENABLE);
#else
static constexpr bool kBuildHasSleepySupport = false;
#endif

static char s_nodeId[16] = {0};
static uint32_t s_seq = 0;
static char s_cliLine[256];
static uint8_t s_consecutiveNoAck = 0;  // back-to-back samples with no ACK

// Run a single CLI command and check its result. otExecCommand() sends the
// command and blocks until the terminating "Done"/"Error ..." line; the
// ot_cmd_return_t carries the parsed error code/message for logging.
static bool otCliCmd(const char *cmd) {
  ot_cmd_return_t rc;
  // otExecCommand() uses Stream::readBytesUntil(), which depends on the current
  // Stream timeout. The sketch adjusts the timeout frequently while parsing ACKs,
  // so force a known-safe value for CLI control commands.
  OThreadCLI.setTimeout(1000);
  bool ok = otExecCommand(cmd, nullptr, &rc);
  if (!ok) {
    Serial.printf("CLI ERROR: '%s' -> %d %s\n", cmd, rc.errorCode, rc.errorMessage.c_str());
  }
  return ok;
}

// Read one line of asynchronous CLI output via the Stream API (used to catch
// the ACK datagram the collector prints). Returns true only for a non-empty line.
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

// Parse the CLI's UDP-receive line ("<N> bytes from <ip> <port> <payload>")
// into its source IP, source port and payload. Returns false otherwise.
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

// Build a short, stable, unique node id by asking OpenThread for the factory
// IEEE EUI-64 through the CLI "eui64" command. otGetRespCmd() runs the command
// and copies its response (the 16 hex-digit EUI-64 plus an EOL) into resp. We
// trim the trailing newline and keep the last 8 hex digits for a compact id.
// Using the CLI avoids depending on any Native API that may be absent in some
// core releases. Returns true if a usable EUI-64 was obtained.
static bool makeNodeIdFromCli() {
  char resp[64] = {0};
  if (!otGetRespCmd("eui64", resp)) {
    return false;
  }
  // otGetRespCmd appends an OS end-of-line; strip any trailing CR/LF/space.
  size_t len = strlen(resp);
  while (len > 0 && (resp[len - 1] == '\r' || resp[len - 1] == '\n' || resp[len - 1] == ' ')) {
    resp[--len] = '\0';
  }
  if (len == 0) {
    return false;
  }
  // Keep the last 8 hex chars (uppercased) for a readable, still-unique id.
  const size_t keep = 8;
  const char *tail = (len > keep) ? (resp + len - keep) : resp;
  strncpy(s_nodeId, tail, sizeof(s_nodeId) - 1);
  s_nodeId[sizeof(s_nodeId) - 1] = '\0';
  for (char *p = s_nodeId; *p; ++p) {
    *p = (char)toupper((unsigned char)*p);
  }
  return true;
}

// Populate s_nodeId. Prefer the EUI-64 reported by the CLI; if that ever fails
// (CLI not ready), fall back to the chip's factory MAC so the node still gets a
// unique id and the demo keeps working.
static void makeNodeId() {
  if (makeNodeIdFromCli()) {
    return;
  }
  Serial.println("eui64 CLI query failed; falling back to chip MAC for node id.");
  unsigned long macLow = (unsigned long)(ESP.getEfuseMac() & 0xFFFFFFFFULL);
  snprintf(s_nodeId, sizeof(s_nodeId), "%08lX", macLow);
}

// Join the network and (optionally) configure sleepy behavior, all via CLI.
// The dataset is committed BEFORE bringing the interface up; the sleepy CLI
// commands ("mode n" etc.) are issued after the dataset but before "thread
// start" so the node attaches directly as a Sleepy End Device.
static bool setupThreadByCli() {
  const char *cmdsBase[] = {
    "thread stop",
    "ifconfig down",
    "dataset clear",
    "dataset networkname " OT_NETWORK_NAME,
    "dataset channel " OT_CHANNEL,
    "dataset panid " OT_PANID,
    "dataset extpanid " OT_EXTPANID,
    "dataset networkkey " OT_NETWORK_KEY,
    "dataset commit active",
  };

  for (size_t i = 0; i < sizeof(cmdsBase) / sizeof(cmdsBase[0]); i++) {
    if (!otCliCmd(cmdsBase[i])) {
      return false;
    }
  }

  // "mode n" = no rx-on-when-idle (the radio may sleep), "pollperiod" sets how
  // often we poll the parent for queued frames, and "childtimeout" tells the
  // parent how long to keep us as a child between polls. These only take effect
  // when the build was compiled with the low-power sdkconfig flags below.
  if (USE_SLEEPY_MODE) {
    if (!kBuildHasSleepySupport) {
      Serial.println("Sleepy mode requested but build lacks required sdkconfig flags.");
      Serial.println("Need: CONFIG_OPENTHREAD_MTD=y + CONFIG_IEEE802154_SLEEP_ENABLE=y + CONFIG_PM_ENABLE=y");
      Serial.println("Continuing as a regular child node.");
    } else {
      char pollCmd[32];
      char timeoutCmd[32];
      snprintf(pollCmd, sizeof(pollCmd), "pollperiod %lu", (unsigned long)SED_POLL_MS);
      snprintf(timeoutCmd, sizeof(timeoutCmd), "childtimeout %lu", (unsigned long)CHILD_TIMEOUT_S);
      const char *sleepyCmds[] = {
        "mode n",
        pollCmd,
        timeoutCmd,
      };
      for (size_t i = 0; i < sizeof(sleepyCmds) / sizeof(sleepyCmds[0]); i++) {
        if (!otCliCmd(sleepyCmds[i])) {
          return false;
        }
      }
    }
  }

  if (!otCliCmd("ifconfig up") || !otCliCmd("thread start")) {
    return false;
  }

  // Wait until attached (role at least Child); give up after ~60s.
  uint8_t tries = 30;
  while (tries-- && OThread.otGetDeviceRole() < OT_ROLE_CHILD) {
    Serial.print(".");
    delay(2000);
  }
  Serial.println();
  return OThread.otGetDeviceRole() >= OT_ROLE_CHILD;
}

// Open a CLI UDP socket and bind it to COLLECTOR_PORT. Binding the same port we
// send to lets the collector's unicast ACK arrive back on this socket so
// readCliLine()/parseUdpLine() can pick it up.
static bool setupUdpCli() {
  char bindCmd[48];
  snprintf(bindCmd, sizeof(bindCmd), "udp bind :: %u", COLLECTOR_PORT);
  const char *cmds[] = {
    "udp close",  // start from a known-clean socket state
    "udp open",
    bindCmd,
  };
  for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
    if (!otCliCmd(cmds[i])) {
      return false;
    }
  }
  return true;
}

// Stand-in for a real sensor: returns a plausible temperature (centi-degrees C)
// and battery level (mV) with some random jitter. Replace with real driver reads.
static void readFakeSensor(int32_t &tempCenti, uint16_t &battMv) {
  tempCenti = 2200 + (int32_t)random(-450, 550);
  battMv = 3600 + (uint16_t)random(0, 400);
}

// True if "ip" is one of this node's own Thread addresses. Used to reject a
// datagram that looped back to us (a node is never its own collector), so the
// node can never mistake its own traffic for a real ACK from the collector.
static bool isOwnAddress(const char *ip) {
  IPAddress addr;
  if (!addr.fromString(ip)) {
    return false;
  }
  for (size_t i = 0, n = OThread.getUnicastAddressCount(); i < n; i++) {
    if (OThread.getUnicastAddress(i) == addr) {
      return true;
    }
  }
  return false;
}

// Parse a collector ACK payload "OK,<nodeId>,<seq>" addressed to THIS node.
// Returns the acknowledged sequence number, or -1 if the line is not an ACK for
// us. The collector reports its own authoritative last-stored sequence here, so
// the caller can resync to it.
static long parseAckSeq(const char *payload) {
  char id[16] = {0};
  unsigned long ackSeq = 0;
  if (sscanf(payload, "OK,%15[^,],%lu", id, &ackSeq) != 2) {
    return -1;
  }
  if (strcmp(id, s_nodeId) != 0) {
    return -1;  // an ACK for a different node
  }
  return (long)ackSeq;
}

// Send one sensor frame for sequence "seq" and wait for the collector's ACK.
//
// The ACK payload is "OK,<nodeId>,<collectorLastSeq>", where collectorLastSeq is
// the last sequence the collector has stored for THIS node (its authoritative
// value). An ACK is considered only when it parses as a genuine UDP-receive line
// whose source is NOT one of our own addresses (no loopback) and whose id field
// matches ours.
//
// Returns the acknowledged sequence number (>= 0) so the caller can resync its
// own counter to the collector, or -1 if no valid ACK was seen after every
// retransmission (collector off/unreachable). On a miss we retransmit the SAME
// seq up to TX_RETRIES times so a single lost frame/ACK does not waste a whole
// sample period. An exact match (ack == seq) is returned immediately; otherwise
// the highest sequence the collector reported is returned for resyncing.
static long sendFrameAndWaitAck(uint32_t seq, int32_t tempCenti, uint16_t battMv) {
  char frame[120];
  char cmd[192];
  snprintf(frame, sizeof(frame), "id=%s,seq=%lu,temp_centi=%ld,batt_mv=%u", s_nodeId, (unsigned long)seq, (long)tempCenti, battMv);
  snprintf(cmd, sizeof(cmd), "udp send %s %u %s", COLLECTOR_GROUP, COLLECTOR_PORT, frame);

  long bestAcked = -1;  // highest sequence the collector reported, for resync
  for (uint8_t attempt = 0; attempt <= TX_RETRIES; attempt++) {
    // Drain any stale CLI output (e.g. a duplicate ACK for an older seq) so it
    // cannot be confused with the response to the frame we are about to send.
    while (readCliLine(s_cliLine, sizeof(s_cliLine), 10)) {}

    OThreadCLI.println(cmd);
    Serial.printf("TX [%s]:%u -> '%s'%s\n", COLLECTOR_GROUP, COLLECTOR_PORT, frame, attempt ? " (retransmit)" : "");

    // Watch the CLI stream for the ACK until the timeout. "Done" is just the
    // result of our send command; "Error" means the send itself failed (no
    // route to the collector) so we retry; anything else is tested as a UDP
    // receive line.
    uint32_t started = millis();
    while (millis() - started < ACK_TIMEOUT_MS) {
      if (!readCliLine(s_cliLine, sizeof(s_cliLine), 30)) {
        continue;
      }
      if (!strncmp(s_cliLine, "Done", 4)) {
        continue;
      }
      if (!strncmp(s_cliLine, "Error", 5)) {
        Serial.printf("UDP send command failed: %s\n", s_cliLine);
        break;  // give up on this attempt; the retry loop will resend
      }
      char srcIp[40] = {0};
      uint16_t srcPort = 0;
      char payload[160] = {0};
      if (parseUdpLine(s_cliLine, srcIp, sizeof(srcIp), srcPort, payload, sizeof(payload))) {
        if (isOwnAddress(srcIp)) {
          // Our own looped-back traffic is never a valid ACK. Log it so a
          // loopback situation is visible instead of silently masquerading.
          Serial.printf("Ignoring datagram from own address [%s]: '%s'\n", srcIp, payload);
          continue;
        }
        long ackSeq = parseAckSeq(payload);
        if (ackSeq >= 0) {
          Serial.printf("ACK [%s]:%u <- '%s'\n", srcIp, srcPort, payload);
          if ((uint32_t)ackSeq == seq) {
            return ackSeq;  // exact confirmation of the frame we just sent
          }
          if (ackSeq > bestAcked) {
            bestAcked = ackSeq;  // remember the collector's reported seq for resync
          }
        }
      }
    }
  }
  if (bestAcked < 0) {
    Serial.println("ACK timeout (no valid ACK from collector)");
  }
  return bestAcked;
}

// Force a clean re-attach to the Thread network. A Sleepy End Device only talks
// to its parent; if that parent (the collector/leader) reboots, the child can
// keep polling a dead parent until the child timeout expires - which can look
// like "the node never reconnects". Restarting the protocol makes it rescan and
// reattach to the rebooted collector right away. The committed dataset and
// sleepy settings persist across stop/start; the UDP socket does not, so we
// re-open it afterwards.
static void forceReattach() {
  Serial.println("Collector unreachable; forcing Thread re-attach...");
  otCliCmd("thread stop");
  delay(200);
  otCliCmd("thread start");

  uint8_t tries = 20;
  while (tries-- && OThread.otGetDeviceRole() < OT_ROLE_CHILD) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println();
  if (!setupUdpCli()) {
    Serial.println("UDP CLI setup failed after re-attach.");
  }
}

void setup() {
  Serial.begin(115200);

  randomSeed(esp_random());

  Serial.println("=== udp_sensor_node (CLI): Sleepy UDP sensor ===");
  // begin(false): start the OpenThread stack but do NOT auto-start a network -
  // we drive the whole bring-up ourselves through the CLI below.
  OThread.begin(false);
  OThreadCLI.begin();
  OThreadCLI.setTimeout(250);

  if (!setupThreadByCli()) {
    Serial.println("Thread setup failed. Halting.");
    while (1) {
      delay(1000);
    }
  }

  makeNodeId();

  Serial.printf("Role: %s\n", OThread.otGetStringDeviceRole());
  Serial.printf("Node ID: %s\n", s_nodeId);
  Serial.printf("Collector group: %s:%u\n", COLLECTOR_GROUP, COLLECTOR_PORT);
  if (USE_SLEEPY_MODE) {
    Serial.printf("Sleepy mode build support: %s\n", kBuildHasSleepySupport ? "enabled" : "not enabled");
  }

  if (!setupUdpCli()) {
    Serial.println("UDP CLI setup failed. Halting.");
    while (1) {
      delay(1000);
    }
  }
}

void loop() {
  // If we have dropped off the network (e.g. parent lost), pause sampling and
  // wait for OpenThread to reattach before sending again.
  if (OThread.otGetDeviceRole() < OT_ROLE_CHILD) {
    Serial.printf("Role=%s, waiting to reattach...\n", OThread.otGetStringDeviceRole());
    delay(1500);
    return;
  }

  int32_t tempCenti;
  uint16_t battMv;
  readFakeSensor(tempCenti, battMv);

  // Candidate sequence number: s_seq is the last value the collector has
  // confirmed, so the frame we send is always s_seq + 1.
  uint32_t seqToSend = s_seq + 1;
  long acked = sendFrameAndWaitAck(seqToSend, tempCenti, battMv);

  if (acked < 0) {
    // No ACK at all: the collector is off/unreachable. HOLD the sequence (do
    // not advance s_seq) and resend the same reading next time, so the count
    // never moves forward without proof of delivery.
    Serial.printf("sample seq=%lu temp=%.2fC batt=%umV status=NO_ACK (sequence held)\n", (unsigned long)seqToSend, (float)tempCenti / 100.0f, battMv);
    // A run of misses is the signature of a collector that rebooted/vanished,
    // so trigger a clean re-attach to recover the network quickly.
    if (++s_consecutiveNoAck >= REATTACH_AFTER_MISSED) {
      forceReattach();
      s_consecutiveNoAck = 0;
    }
  } else {
    // The collector responded, so the network is healthy. Resync our counter to
    // the collector's authoritative last sequence: this rolls back if we ran
    // ahead, or skips forward if the collector ignored a stale/old frame.
    s_consecutiveNoAck = 0;
    uint32_t ackedSeq = (uint32_t)acked;
    bool delivered = (ackedSeq == seqToSend);
    if (!delivered) {
      Serial.printf("Resync: collector last seq=%lu (we sent %lu); aligning local sequence.\n", (unsigned long)ackedSeq, (unsigned long)seqToSend);
    }
    s_seq = ackedSeq;  // collector is the source of truth for the sequence
    Serial.printf(
      "sample seq=%lu temp=%.2fC batt=%umV status=%s\n", (unsigned long)seqToSend, (float)tempCenti / 100.0f, battMv, delivered ? "ACKED" : "RESYNC"
    );
  }

  delay(SAMPLE_PERIOD_MS);
}
