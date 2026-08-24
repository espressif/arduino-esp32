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
 * Thread DNS-SD advertise — onServiceEvent + re-advertise recovery.
 *
 * Watches OT_DNSSD_EVENT_ERROR, lost Thread attach, and lost announce state
 * (e.g. OTBR restart). Recovery is end() + begin() + addService() from loop()
 * — not from the SRP callback (flags only there).
 *
 * Set OT_NETKEY to your OTBR Network Key.
 * For a minimal blocking example, see ThreadDNSSD_Advertise.
 */

#include <Arduino.h>
#include "OThread.h"
#include "OThreadDNSSD.h"

// Same Network Key as the OTBR Thread network (other dataset fields are learned on attach).
static const uint8_t OT_NETKEY[OT_NETWORK_KEY_SIZE] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};

static const char *kHostName = "sensor-cb";
static const uint16_t kServicePort = 12346;
static const uint32_t kReadvertiseCooldownMs = 15000;

static volatile bool s_gotEvent = false;
static volatile ot_dnssd_event_t s_event = OT_DNSSD_EVENT_ERROR;
static volatile otError s_err = OT_ERROR_NONE;
static volatile bool s_ignoreLocalRemoved = false;

static bool s_attached = false;
static bool s_wasAttached = false;
static bool s_announced = false;
static bool s_needReadvertise = false;
static bool s_nameConflict = false;
static uint32_t s_lastAdvertiseMs = 0;

static void onDnsEvent(ot_dnssd_event_t event, otError error, void *context) {
  (void)context;
  // OpenThread task (or caller task for end()-generated REMOVED): flags only.
  if (event == OT_DNSSD_EVENT_REMOVED && s_ignoreLocalRemoved) {
    return;
  }
  s_event = event;
  s_err = error;
  s_gotEvent = true;
}

static bool isAttachedRole(ot_device_role_t role) {
  return role == OT_ROLE_CHILD || role == OT_ROLE_ROUTER || role == OT_ROLE_LEADER;
}

static bool waitAttached(uint32_t timeoutMs) {
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    if (isAttachedRole(OThread.otGetDeviceRole())) {
      return true;
    }
    delay(200);
  }
  return false;
}

// Returning from setup() still runs loop(); halt on initial attach fail.
// begin()/addService failures in startAdvertise() are retried from loop().
static void halt(const char *msg) {
  Serial.println(msg);
  while (true) {
    delay(1000);
  }
}

// Queue registration; completion arrives via OT_DNSSD_EVENT_ANNOUNCED.
static bool startAdvertise(const char *reason) {
  Serial.printf("Advertise (%s) as %s...\r\n", reason, kHostName);

  s_ignoreLocalRemoved = true;
  OThreadDNSSD.end();
  s_ignoreLocalRemoved = false;
  s_announced = false;

  if (!OThreadDNSSD.begin(kHostName)) {
    Serial.println("FAIL: OThreadDNSSD.begin");
    s_needReadvertise = true;
    s_lastAdvertiseMs = millis();
    return false;
  }
  if (!OThreadDNSSD.addService("ot", "udp", kServicePort)) {
    Serial.println("FAIL: addService");
    s_needReadvertise = true;
    s_lastAdvertiseMs = millis();
    return false;
  }

  s_lastAdvertiseMs = millis();
  Serial.println("Waiting for OT_DNSSD_EVENT_ANNOUNCED...");
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("ThreadDNSSD_Advertise_Callback");

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
  s_attached = true;
  s_wasAttached = true;
  Serial.printf("Attached as %s\r\n", OThread.otGetStringDeviceRole());

  OThreadDNSSD.onServiceEvent(onDnsEvent);
  (void)startAdvertise("initial");
}

void loop() {
  ot_device_role_t role = OThread.otGetDeviceRole();
  s_attached = isAttachedRole(role);

  if (s_wasAttached && !s_attached) {
    Serial.printf("Lost attach (role=%s) — will re-advertise when attached again\r\n", OThread.otGetStringDeviceRole());
    s_announced = false;
    s_needReadvertise = true;
  }
  s_wasAttached = s_attached;

  if (s_gotEvent) {
    s_gotEvent = false;
    if (s_event == OT_DNSSD_EVENT_ANNOUNCED) {
      s_announced = true;
      s_needReadvertise = false;
      s_nameConflict = false;
      Serial.printf("PASS: ANNOUNCED as %s\r\n", OThreadDNSSD.hostname());
    } else if (s_event == OT_DNSSD_EVENT_ERROR) {
      Serial.printf("EVENT: ERROR (%d)\r\n", (int)s_err);
      if (s_err == OT_ERROR_DUPLICATED || s_err == OT_ERROR_SECURITY) {
        s_nameConflict = true;
        s_needReadvertise = false;
        Serial.printf("Name conflict for '%s' — not auto-retrying with the same hostname\r\n", OThreadDNSSD.hostname());
      } else {
        s_announced = false;
        s_needReadvertise = true;
      }
    } else if (s_event == OT_DNSSD_EVENT_REMOVED) {
      Serial.println("EVENT: REMOVED");
      s_announced = false;
      s_needReadvertise = true;
    }
  }

  if (s_attached && s_announced && !OThreadDNSSD.isAnnounceComplete()) {
    Serial.println("Announce incomplete — scheduling re-advertise");
    s_announced = false;
    s_needReadvertise = true;
  }

  if (s_needReadvertise && s_attached && !s_nameConflict) {
    uint32_t now = millis();
    if (now - s_lastAdvertiseMs >= kReadvertiseCooldownMs) {
      s_needReadvertise = false;
      (void)startAdvertise("recovery");
    }
  }

  delay(50);
  static uint32_t s_lastStatusMs = 0;
  if (millis() - s_lastStatusMs >= 5000) {
    s_lastStatusMs = millis();
    Serial.printf(
      "status announceComplete=%d attached=%d needReadvertise=%d role=%s\r\n", OThreadDNSSD.isAnnounceComplete(), s_attached, s_needReadvertise,
      OThread.otGetStringDeviceRole()
    );
  }
}
