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
 * Thread DNS-SD discover — async queryService + queryHost via onQueryEvent.
 *
 * Same lab as ThreadDNSSD_Query / ThreadDNSSD_QueryHost: pair with
 * ThreadDNSSD_Advertise (hostname sensor-1, service _ot._udp).
 *
 * Set OT_NETKEY to your OTBR Network Key.
 */

#include <Arduino.h>
#include "OThread.h"
#include "OThreadDNSSD.h"

// Same Network Key as the OTBR Thread network (other dataset fields are learned on attach).
static const uint8_t OT_NETKEY[OT_NETWORK_KEY_SIZE] = {
  0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff
};

static const char *kPeerHost = "sensor-1";
static const uint32_t kCycleDelayMs = 10000;

enum class Phase : uint8_t {
  Idle,
  WaitService,
  WaitHost,
  Delay,
};

static volatile bool s_gotQuery = false;
static volatile ot_dnssd_query_kind_t s_kind = OT_DNSSD_QUERY_SERVICE;
static volatile ot_dnssd_query_event_t s_event = OT_DNSSD_QUERY_DONE;
static volatile otError s_err = OT_ERROR_NONE;
static volatile int s_count = 0;

static Phase s_phase = Phase::Idle;
static uint32_t s_delayUntilMs = 0;
static bool s_ready = false;

static void onQueryEvent(ot_dnssd_query_kind_t kind, ot_dnssd_query_event_t event, otError error, int count, void *context) {
  (void)context;
  // OpenThread task: flags only — no OThreadDNSSD / Serial calls here.
  s_kind = kind;
  s_event = event;
  s_err = error;
  s_count = count;
  s_gotQuery = true;
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

// Returning from setup() still runs loop(); halt so startQuery* is not called without begin().
static void halt(const char *msg) {
  Serial.println(msg);
  while (true) {
    delay(1000);
  }
}

static void printServiceResults(int n) {
  Serial.printf("Service browse done: %d instance(s) (event=%d error=%d)\r\n", n, (int)s_event, (int)s_err);
  for (int i = 0; i < n; ++i) {
    Serial.printf(
      "  [%d] instance=%s host=%s port=%u addr=%s\r\n", i, OThreadDNSSD.instanceName(i), OThreadDNSSD.hostname(i),
      (unsigned)OThreadDNSSD.port(i), OThreadDNSSD.address(i).toString().c_str()
    );
    for (int t = 0; t < OThreadDNSSD.numTxt(i); ++t) {
      Serial.printf("       TXT %s=%s\r\n", OThreadDNSSD.txtKey(i, t).c_str(), OThreadDNSSD.txt(i, t).c_str());
    }
  }
}

static void printHostResult(int count) {
  IPAddress addr = OThreadDNSSD.resolvedAddress();
  Serial.printf("Host resolve done: count=%d (event=%d error=%d)\r\n", count, (int)s_event, (int)s_err);
  if (count > 0) {
    Serial.printf("  %s -> %s\r\n", kPeerHost, addr.toString().c_str());
  } else {
    Serial.printf("  %s not found (is Advertise board registered?)\r\n", kPeerHost);
  }
}

static bool startServiceBrowse() {
  Serial.println();
  Serial.println("startQueryService(\"ot\", \"udp\")...");
  if (!OThreadDNSSD.startQueryService("ot", "udp")) {
    Serial.println("FAIL: startQueryService (busy or not started)");
    return false;
  }
  s_phase = Phase::WaitService;
  return true;
}

static bool startHostResolve() {
  Serial.println();
  Serial.printf("startQueryHost(\"%s\")...\r\n", kPeerHost);
  if (!OThreadDNSSD.startQueryHost(kPeerHost)) {
    Serial.println("FAIL: startQueryHost (busy or not started)");
    return false;
  }
  s_phase = Phase::WaitHost;
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("ThreadDNSSD_Query_Callback");

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

  if (!OThreadDNSSD.begin("browser-cb")) {
    halt("FAIL: OThreadDNSSD.begin");
  }
  OThreadDNSSD.onQueryEvent(onQueryEvent, nullptr);
  delay(2000);
  s_ready = true;
  (void)startServiceBrowse();
}

void loop() {
  if (!s_ready) {
    delay(1000);
    return;
  }

  if (s_gotQuery) {
    s_gotQuery = false;
    ot_dnssd_query_kind_t kind = s_kind;
    ot_dnssd_query_event_t qevent = s_event;
    int count = s_count;

    if (kind == OT_DNSSD_QUERY_SERVICE && s_phase == Phase::WaitService) {
      printServiceResults(count);
      if (qevent == OT_DNSSD_QUERY_DONE && startHostResolve()) {
        // host resolve started
      } else {
        s_delayUntilMs = millis() + kCycleDelayMs;
        s_phase = Phase::Delay;
      }
    } else if (kind == OT_DNSSD_QUERY_HOST && s_phase == Phase::WaitHost) {
      printHostResult(count);
      s_delayUntilMs = millis() + kCycleDelayMs;
      s_phase = Phase::Delay;
    }
  }

  if (s_phase == Phase::Delay && (int32_t)(millis() - s_delayUntilMs) >= 0) {
    (void)startServiceBrowse();
  }

  delay(50);
}
