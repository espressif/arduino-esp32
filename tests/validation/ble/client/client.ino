// Combined BLE validation test — CLIENT
// Phases: basic lifecycle, BLE5 ext+periodic scan, GATT client, notifications,
//         large writes, descriptors, write-no-response, server disconnect,
//         security, PHY/DLE, reconnect

#include <Arduino.h>
#include <BLE.h>
#include "esp_heap_caps.h"
#include "esp_timer.h"

static BLEUUID serviceUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
static BLEUUID rwCharUUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");
static BLEUUID notifyCharUUID("cba1d466-344c-4be3-ab3f-189f80dd7518");
static BLEUUID indicateCharUUID("d5f782b2-a36e-4d68-947c-0e9a5f2c78e1");
static BLEUUID secureCharUUID("ff1d2614-e2d6-4c87-9154-6625d39ca7f8");
static BLEUUID descCharUUID("a3c87501-8ed3-4bdf-8a39-a01bebede295");
static BLEUUID writeNrCharUUID("1c95d5e3-d8f7-413a-bf3d-7a2e5d7be87e");
static BLEUUID permFailClosedCharUUID("e8b1d3a1-6b3e-4a5d-9f1d-9b7e5c3a2f41");
// Phase 15: custom-UUID descriptor on DESC_CHAR declared as read-only. The
// client reads it (should succeed) and then writes to it (should be rejected
// at the ATT layer), exercising runtime enforcement of the descriptor
// permissions on both NimBLE and Bluedroid.
static BLEUUID descReadOnlyUUID("b5f3e2c1-8a9e-4b7c-9f3d-2e8a5b7c3f91");
// Phase 18 (authorization) + Phase 19 (encrypted).
static BLEUUID authzCharUUID("ca11aaaa-1111-4222-8333-444455556666");
static BLEUUID encryptedCharUUID("ca11bbbb-1111-4222-8333-444455556666");
// Phase 23 (error paths) — these UUIDs are advertised as absent.
static BLEUUID unknownSvcUUID("deadbeef-0000-0000-0000-000000000001");
static BLEUUID unknownCharUUID("deadbeef-0000-0000-0000-000000000002");

String targetName;
BTAddress targetAddr;
volatile int currentPhase = 0;

// Phase 16 — client-side MTU + conn-params callback state. Set from the BLE
// stack task; logged from the Arduino task after the phase start marker.
volatile bool clientMtuChanged = false;
volatile uint16_t clientMtuLast = 0;
volatile bool clientConnParamsUpdated = false;
volatile uint16_t clientConnParamsReqInterval = 0;
volatile uint16_t clientConnParamsReqLatency = 0;
volatile uint16_t clientConnParamsReqTimeout = 0;

// ========================= Phase coordination ================================

void checkSerial() {
  static String buf;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n') {
      buf.trim();
      if (buf.startsWith("START_PHASE_")) {
        int phase = buf.substring(12).toInt();
        if (phase > currentPhase) {
          currentPhase = phase;
          Serial.printf("[CLIENT] Phase %d started\n", phase);
        }
      }
      buf = "";
    } else if (c != '\r') {
      buf += c;
    }
  }
}

// Same ordering guarantee as server: apply START_PHASE_* before BLE callback side effects.
static void syncPhaseFromHost() {
  checkSerial();
}

void waitForPhase(int n) {
  while (currentPhase < n) {
    checkSerial();
    delay(10);
  }
}

void readName() {
  Serial.println("[CLIENT] Device ready for name");
  Serial.println("[CLIENT] Send name:");
  while (targetName.length() == 0) {
    if (Serial.available()) {
      targetName = Serial.readStringUntil('\n');
      targetName.trim();
    }
    delay(100);
  }
  Serial.printf("[CLIENT] Target: %s\n", targetName.c_str());
}

// ========================= Phase 1 — Basic Lifecycle =========================

bool phase_basic() {
  BTStatus status = BLE.begin(targetName + "_client");
  if (!status) {
    Serial.printf("[CLIENT] Init FAILED: %s\n", status.toString());
    return false;
  }
  Serial.printf("[CLIENT] Stack: %s\n", BLE.getStackName());
  Serial.println("[CLIENT] Init OK");
  Serial.printf("[CLIENT] Device name: %s\n", BLE.getDeviceName().c_str());
  Serial.printf("[CLIENT] Address: %s\n", BLE.getAddress().toString().c_str());

  BLE.end(false);
  Serial.println("[CLIENT] Deinit OK");
  delay(1000);

  status = BLE.begin(targetName + "_client_reinit");
  if (!status) {
    Serial.printf("[CLIENT] Reinit FAILED: %s\n", status.toString());
    return false;
  }
  Serial.println("[CLIENT] Reinit OK");

  BLE.end(false);
  Serial.println("[CLIENT] Final deinit OK");
  return true;
}

// ============= Phase 2 — BLE5 Extended Scan + Periodic Sync =================

#if BLE5_SUPPORTED
bool phase_ble5_scan() {
  BTStatus status = BLE.begin("BLE_CLT_Ext");
  if (!status) {
    return false;
  }

  volatile bool found = false;
  volatile bool synced = false;
  volatile bool dataReceived = false;

  BLEScan scan = BLE.getScan();

  scan.onResult([&](BLEAdvertisedDevice dev) {
    syncPhaseFromHost();
    if (!found && dev.getName() == targetName) {
      Serial.println("[CLIENT] Found target via ext scan!");
      targetAddr = dev.getAddress();
      found = true;
      scan.createPeriodicSync(targetAddr, 1);
    }
  });

  scan.onPeriodicSync([&](uint16_t syncHandle, uint8_t sid, const BTAddress &addr, BLEPhy phy, uint16_t interval) {
    syncPhaseFromHost();
    Serial.println("[CLIENT] Synced to periodic adv!");
    synced = true;
  });

  scan.onPeriodicReport([&](uint16_t syncHandle, int8_t rssi, int8_t txPower, const uint8_t *data, size_t len) {
    syncPhaseFromHost();
    if (!dataReceived) {
      Serial.println("[CLIENT] Periodic data received");
      dataReceived = true;
      BLE.getScan().stop();
    }
  });

  Serial.println("[CLIENT] BLE5 init OK");
  Serial.println("[CLIENT] Scanning for periodic adv...");
  scan.startExtended(15000);

  if (found) {
    Serial.println("[CLIENT] Extended scan found target");
  }
  if (dataReceived) {
    Serial.println("[CLIENT] Periodic test complete");
  }

  BLE.end(false);
  Serial.println("[CLIENT] BLE5 scan phase done");
  return true;
}
#endif

// ======================== Phase 3+ — GATT Client =============================

bool scanForServer() {
  bool found = false;
  BLEScan scan = BLE.getScan();
  scan.onResult([&](BLEAdvertisedDevice dev) {
    syncPhaseFromHost();
    if (dev.getName() == targetName) {
      Serial.println("[CLIENT] Found target server!");
      targetAddr = dev.getAddress();
      found = true;
      BLE.getScan().stop();
    }
  });

  for (int attempt = 1; attempt <= 5 && !found; attempt++) {
    Serial.printf("[CLIENT] Scanning (attempt %d)...\n", attempt);
    scan.start(5000);
    if (!found) {
      delay(1000);
    }
  }
  scan.resetCallbacks();
  return found;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }
  readName();

  // ===== Phase 1: Basic lifecycle =====
  waitForPhase(1);
  if (!phase_basic()) {
    return;
  }

  // ===== Phase 2: BLE5 ext adv + periodic =====
  waitForPhase(2);
  {
    bool ble5_ok = false;
#if BLE5_SUPPORTED
    ble5_ok = phase_ble5_scan();
#endif
    if (!ble5_ok) {
      Serial.println("[CLIENT] BLE5 not supported, skipping");
    }
  }

  // ===== Phase 3: Init for GATT tests =====
  waitForPhase(3);

  size_t heapBefore = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  Serial.printf("[CLIENT] Heap before init: %u\n", (unsigned)heapBefore);

  BTStatus status = BLE.begin("BLE_CLT");
  if (!status) {
    Serial.printf("[CLIENT] GATT init FAILED: %s\n", status.toString());
    return;
  }
  Serial.println("[CLIENT] GATT init OK");
  BLE.setMTU(512);

  size_t heapAfterInit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  Serial.printf("[CLIENT] Heap after init: %u\n", (unsigned)heapAfterInit);

  BLESecurity sec = BLE.getSecurity();
  sec.setIOCapability(BLESecurity::DisplayYesNo);
  sec.setAuthenticationMode(true, true, true);
  // Distribute both the encryption key and the identity key so phase 22
  // can verify local/peer IRK round-trip across the bond.
  sec.setInitiatorKeys(BLESecurity::KeyDist::EncKey | BLESecurity::KeyDist::IdKey);
  sec.setResponderKeys(BLESecurity::KeyDist::EncKey | BLESecurity::KeyDist::IdKey);
  sec.onPassKeyDisplay([](const BLEConnInfo &conn, uint32_t passkey) {
    Serial.printf("[CLIENT] Passkey: %06lu\n", (unsigned long)passkey);
  });
  sec.onConfirmPassKey([](const BLEConnInfo &conn, uint32_t passkey) -> bool {
    Serial.printf("[CLIENT] Passkey: %06lu\n", (unsigned long)passkey);
    return true;
  });
  sec.onAuthenticationComplete([](const BLEConnInfo &conn, bool success) {
    Serial.println("[CLIENT] Authentication complete");
  });

  // Callback helpers installed on every BLEClient we create so phase 16
  // can assert MTU + conn-params events fired on the client side too.
  auto installClientCallbacks = [](BLEClient c) {
    c.onMtuChanged([](BLEClient, const BLEConnInfo &, uint16_t mtu) {
      syncPhaseFromHost();
      clientMtuLast = mtu;
      clientMtuChanged = true;
    });
    c.onConnParamsUpdateRequest([](BLEClient, const BLEConnParams &p) -> bool {
      syncPhaseFromHost();
      clientConnParamsReqInterval = p.maxInterval;
      clientConnParamsReqLatency = p.latency;
      clientConnParamsReqTimeout = p.timeout;
      clientConnParamsUpdated = true;
      return true;
    });
  };

  if (!scanForServer()) {
    Serial.println("[CLIENT] Server not found");
    return;
  }

  uint32_t connectStart = millis();
  BLEClient client = BLE.createClient();
  installClientCallbacks(client);
  status = client.connect(targetAddr);
  uint32_t connectTime = millis() - connectStart;
  if (!status) {
    Serial.printf("[CLIENT] Connect FAILED: %s\n", status.toString());
    return;
  }
  Serial.println("[CLIENT] Connected");
  Serial.printf("[CLIENT] Connect time: %lu ms\n", (unsigned long)connectTime);

  uint16_t mtu = client.getMTU();
  Serial.printf("[CLIENT] Negotiated MTU: %u\n", mtu);

  size_t heapAfterConnect = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  Serial.printf("[CLIENT] Heap after connect: %u\n", (unsigned)heapAfterConnect);

  BLERemoteService svc = client.getService(serviceUUID);
  if (!svc) {
    Serial.println("[CLIENT] Service not found");
    return;
  }
  Serial.println("[CLIENT] Found service");

  // ===== Phase 4: GATT read/write =====
  waitForPhase(4);

  BLERemoteCharacteristic rwChr = svc.getCharacteristic(rwCharUUID);
  if (!rwChr) {
    Serial.println("[CLIENT] RW characteristic not found");
    return;
  }
  Serial.println("[CLIENT] Found characteristic");

  uint64_t readStart = esp_timer_get_time();
  String val = rwChr.readValue();
  uint64_t readEnd = esp_timer_get_time();
  Serial.printf("[CLIENT] Read value: %s\n", val.c_str());
  Serial.printf("[CLIENT] Read latency: %lu us\n", (unsigned long)(readEnd - readStart));

  status = rwChr.writeValue("Hello from client!");
  if (status) {
    Serial.println("[CLIENT] Write OK");
  } else {
    Serial.printf("[CLIENT] Write FAILED: %s\n", status.toString());
  }

  val = rwChr.readValue();
  Serial.printf("[CLIENT] Read-back: %s\n", val.c_str());

  // ===== Phase 5: Notifications + Indications =====
  waitForPhase(5);

  BLERemoteCharacteristic notifyChr = svc.getCharacteristic(notifyCharUUID);
  BLERemoteCharacteristic indicateChr = svc.getCharacteristic(indicateCharUUID);

  // The BLE stack delivers the notification/indication callback from its own
  // task, which can race with the Arduino task that called subscribe(). To
  // keep logs both truthful and deterministically ordered (and identical
  // across NimBLE and Bluedroid), the callback only captures the payload and
  // signals; all logging is driven from this task in the intended sequence.
  volatile bool notifReceived = false;
  volatile bool indicReceived = false;
  String notifPayload;
  String indicPayload;

  BTStatus subStatus =
    notifyChr.subscribe(true, [&notifReceived, &notifPayload](BLERemoteCharacteristic chr, const uint8_t *data, size_t length, bool isNotify) {
      syncPhaseFromHost();
      notifPayload = String((const char *)data, length);
      notifReceived = true;
    });
  if (subStatus) {
    Serial.println("[CLIENT] Subscribed to notifications");
  } else {
    Serial.printf("[CLIENT] Subscribe to notifications FAILED: %s\n", subStatus.toString());
  }

  {
    unsigned long start = millis();
    while (!notifReceived && (millis() - start < 30000)) {
      delay(100);
    }
  }
  if (notifReceived) {
    Serial.printf("[CLIENT] Notification received: %s\n", notifPayload.c_str());
  }

  subStatus = indicateChr.subscribe(false, [&indicReceived, &indicPayload](BLERemoteCharacteristic chr, const uint8_t *data, size_t length, bool isNotify) {
    syncPhaseFromHost();
    indicPayload = String((const char *)data, length);
    indicReceived = true;
  });
  if (subStatus) {
    Serial.println("[CLIENT] Subscribed to indications");
  } else {
    Serial.printf("[CLIENT] Subscribe to indications FAILED: %s\n", subStatus.toString());
  }

  {
    unsigned long start = millis();
    while (!indicReceived && (millis() - start < 30000)) {
      delay(100);
    }
  }
  if (indicReceived) {
    Serial.printf("[CLIENT] Indication received: %s\n", indicPayload.c_str());
  }

  notifyChr.unsubscribe();
  indicateChr.unsubscribe();
  Serial.println("[CLIENT] Unsubscribed");

  // ===== Phase 6: Large ATT write (>MTU) =====
  waitForPhase(6);
  {
    const size_t bigLen = 512;
    uint8_t bigBuf[bigLen];
    for (size_t i = 0; i < bigLen; i++) {
      bigBuf[i] = (uint8_t)(i & 0xFF);
    }

    status = rwChr.writeValue(bigBuf, bigLen);
    if (status) {
      Serial.println("[CLIENT] Large write OK");
    } else {
      Serial.printf("[CLIENT] Large write FAILED: %s\n", status.toString());
    }

    val = rwChr.readValue();
    Serial.printf("[CLIENT] Large read (%u bytes)\n", val.length());

    bool match = (val.length() == bigLen);
    if (match) {
      for (size_t i = 0; i < val.length(); i++) {
        if ((uint8_t)val[i] != (uint8_t)(i & 0xFF)) {
          match = false;
          break;
        }
      }
    }
    if (match) {
      Serial.println("[CLIENT] Large data integrity OK");
    } else {
      Serial.println("[CLIENT] Large data integrity FAILED");
    }
  }

  // ===== Phase 7: Descriptor read/write =====
  waitForPhase(7);
  {
    BLERemoteCharacteristic descChr = svc.getCharacteristic(descCharUUID);
    if (!descChr) {
      Serial.println("[CLIENT] Descriptor char not found");
    } else {
      Serial.println("[CLIENT] Found descriptor char");
      auto descriptors = descChr.getDescriptors();
      Serial.printf("[CLIENT] Descriptor count: %u\n", (unsigned)descriptors.size());

      // Find User Description (0x2901)
      BLERemoteDescriptor userDesc = descChr.getDescriptor(BLEUUID(static_cast<uint16_t>(0x2901)));
      if (userDesc) {
        String descVal = userDesc.readValue();
        Serial.printf("[CLIENT] User description: %s\n", descVal.c_str());
      } else {
        Serial.println("[CLIENT] User description not found");
      }

      // Find Presentation Format (0x2904)
      BLERemoteDescriptor pfDesc = descChr.getDescriptor(BLEUUID(static_cast<uint16_t>(0x2904)));
      if (pfDesc) {
        String pfVal = pfDesc.readValue();
        if (pfVal.length() >= 7) {
          uint8_t format = (uint8_t)pfVal[0];
          Serial.printf("[CLIENT] Presentation format: %u\n", format);
          if (format == BLEDescriptor::FORMAT_UTF8) {
            Serial.println("[CLIENT] Format matches UTF8");
          }
        }
      } else {
        Serial.println("[CLIENT] Presentation format not found");
      }
    }
  }

  // ===== Phase 8: Write without response =====
  waitForPhase(8);
  {
    BLERemoteCharacteristic writeNrChr = svc.getCharacteristic(writeNrCharUUID);
    if (!writeNrChr) {
      Serial.println("[CLIENT] WriteNR char not found");
    } else {
      Serial.println("[CLIENT] Found WriteNR char");
      status = writeNrChr.writeValue("WriteNR_OK", false);
      if (status) {
        Serial.println("[CLIENT] WriteNR sent");
      } else {
        Serial.printf("[CLIENT] WriteNR FAILED: %s\n", status.toString());
      }
      delay(500);
      // Read back to verify server received it
      String readBack = writeNrChr.readValue();
      Serial.printf("[CLIENT] WriteNR readback: %s\n", readBack.c_str());
    }
    Serial.println("[CLIENT] Status: write_no_response done");
  }

  // ===== Phase 9: Server-initiated disconnect =====
  waitForPhase(9);
  {
    Serial.println("[CLIENT] Waiting for server disconnect...");
    unsigned long start = millis();
    while (client.isConnected() && (millis() - start < 10000)) {
      delay(100);
    }
    if (!client.isConnected()) {
      Serial.println("[CLIENT] Server disconnected us");
    } else {
      Serial.println("[CLIENT] Server disconnect timeout");
    }

    // Reconnect after server-initiated disconnect
    delay(2000);
    client = BLE.createClient();
    installClientCallbacks(client);
    status = client.connect(targetAddr);
    if (status) {
      Serial.println("[CLIENT] Reconnected after server disconnect");

      // Re-discover services
      svc = client.getService(serviceUUID);
      if (!svc) {
        Serial.println("[CLIENT] Service not found after reconnect");
      }
    } else {
      Serial.printf("[CLIENT] Reconnect FAILED: %s\n", status.toString());
    }
  }

  // ===== Phase 10: Security — encrypted characteristic =====
  waitForPhase(10);
  {
    // Host can deliver START_PHASE_10 to the client before the server has run
    // loop() to process the same line — give the server time to disconnect +
    // deleteAllBonds() so we do not reuse a bond from an earlier run/phase.
    delay(500);
    BLESecurity secWipe = BLE.getSecurity();
    if (secWipe) {
      (void)secWipe.deleteAllBonds();
    }
    if (client.isConnected()) {
      (void)client.disconnect();
    }
    delay(300);
    if (secWipe) {
      (void)secWipe.deleteAllBonds();
    }

    client = BLE.createClient();
    installClientCallbacks(client);
    status = client.connect(targetAddr);
    if (!status) {
      Serial.printf("[CLIENT] Phase10 reconnect FAILED: %s\n", status.toString());
    } else {
      Serial.println("[CLIENT] Phase10 reconnected for pairing");
    }
    svc = client.getService(serviceUUID);
    if (!svc) {
      Serial.println("[CLIENT] Service not found after phase10 reconnect");
    } else {
      BLERemoteCharacteristic secureChr = svc.getCharacteristic(secureCharUUID);
      if (!secureChr) {
        Serial.println("[CLIENT] Secure characteristic not found");
      } else {
        delay(1000);
        String sval = secureChr.readValue();
        Serial.printf("[CLIENT] Secure read: %s\n", sval.c_str());
      }
    }
  }

  // ===== Phase 11: BLE5 PHY + DLE =====
  waitForPhase(11);
  {
    bool phy_ok = false;
#if BLE5_SUPPORTED
    status = client.setPhy(BLEPhy::PHY_2M, BLEPhy::PHY_2M);
    if (status) {
      Serial.println("[CLIENT] PHY update OK");
      phy_ok = true;
    }
    if (phy_ok) {
      status = client.setDataLen(251, 2120);
      if (status) {
        Serial.println("[CLIENT] DLE set OK");
      } else {
        Serial.printf("[CLIENT] DLE failed: %s\n", status.toString());
      }
    }
#endif
    if (!phy_ok) {
      Serial.println("[CLIENT] BLE5 PHY/DLE not supported, skipping");
    }
  }

  // ===== Phase 12: Reconnect (3 cycles) =====
  waitForPhase(12);
  client.disconnect();
  Serial.println("[CLIENT] Disconnected for reconnect test");
  delay(2000);

  for (int i = 0; i < 3; i++) {
    Serial.printf("[CLIENT] Connect cycle %d\n", i + 1);
    BLEClient rc = BLE.createClient();
    installClientCallbacks(rc);
    status = rc.connect(targetAddr);
    if (status) {
      Serial.println("[CLIENT] Connected");
      delay(2000);
      rc.disconnect();
      Serial.println("[CLIENT] Disconnected");
      delay(2000);
    } else {
      Serial.printf("[CLIENT] Connect FAILED cycle %d\n", i + 1);
    }
  }

  Serial.println("[CLIENT] All cycles complete");

  // ===== Phase 13: BLEStream =====
  waitForPhase(13);
  {
    bool stream_ok = false;
    // Don't tear down BLE — BLEStream layers on the existing stack.
    // Give server time to reconfigure advertising with NUS service.
    delay(2000);
    BLEStream stream;
    for (int attempt = 1; attempt <= 5 && !stream_ok; attempt++) {
      BTStatus s = stream.beginClient(targetAddr, 7000);
      if (!s) {
        Serial.printf("[CLIENT] BLEStream connect attempt %d failed: %s\n", attempt, s.toString());
        delay(500);
        continue;
      }
      Serial.println("[CLIENT] BLEStream init OK");
      stream.setTimeout(10000);

      if (stream.connected()) {
        Serial.println("[CLIENT] BLEStream connected OK");
      }

      // Basic ping/pong
      stream.println("stream_ping");
      Serial.println("[CLIENT] BLEStream sent");
      {
        String rx = stream.readStringUntil('\n');
        rx.trim();
        Serial.printf("[CLIENT] BLEStream received: %s\n", rx.c_str());
      }

      // Send data for server-side peek() test
      stream.println("peek_test");
      Serial.println("[CLIENT] BLEStream peek_test sent");
      delay(200);

      // Bulk write: 200-byte repeating A-Z pattern (exercises multi-MTU chunking)
      {
        char bulk[201];
        for (int i = 0; i < 200; i++) {
          bulk[i] = 'A' + (i % 26);
        }
        bulk[200] = '\0';
        stream.println(bulk);
        Serial.println("[CLIENT] BLEStream bulk sent: 200 bytes");
      }

      // Receive server-initiated message (tests server→client direction)
      {
        String rx = stream.readStringUntil('\n');
        rx.trim();
        Serial.printf("[CLIENT] BLEStream server msg: %s\n", rx.c_str());
      }

      stream_ok = true;
      stream.end();
    }
    if (stream_ok) {
      Serial.println("[CLIENT] Status: blestream done");
    } else {
      Serial.println("[CLIENT] BLEStream phase FAILED");
    }
  }

  // ===== Phase 14: L2CAP CoC =====
  waitForPhase(14);
  {
    bool l2cap_supported = false;
#if BLE_L2CAP_SUPPORTED
    l2cap_supported = true;
    BTStatus s;
    BLEClient l2capClient = BLE.createClient();
    installClientCallbacks(l2capClient);
    s = l2capClient.connect(targetAddr);
    if (!s) {
      Serial.printf("[CLIENT] L2CAP connect FAILED: %s\n", s.toString());
    } else {
      Serial.println("[CLIENT] L2CAP init OK");
      BLEL2CAPChannel channel = BLE.connectL2CAP(l2capClient.getHandle(), 0x0080, 128);
      if (!channel) {
        Serial.println("[CLIENT] L2CAP open FAILED");
      } else {
        volatile bool dataReceived = false;
        String l2capRx;
        channel.onData([&](BLEL2CAPChannel ch, const uint8_t *data, size_t len) {
          syncPhaseFromHost();
          l2capRx = String((const char *)data, len);
          dataReceived = true;
        });

        unsigned long connectStart = millis();
        while (!channel.isConnected() && (millis() - connectStart < 10000)) {
          delay(20);
        }
        if (channel.isConnected()) {
          Serial.println("[CLIENT] L2CAP channel connected");
          const char *payload = "L2CAP_PING";
          s = channel.write((const uint8_t *)payload, strlen(payload));
          if (s) {
            Serial.println("[CLIENT] L2CAP sent");
          } else {
            Serial.printf("[CLIENT] L2CAP write FAILED: %s\n", s.toString());
          }

          unsigned long rxStart = millis();
          while (!dataReceived && (millis() - rxStart < 10000)) {
            delay(20);
          }
          if (dataReceived) {
            Serial.printf("[CLIENT] L2CAP received: %s\n", l2capRx.c_str());
            if (l2capRx == "L2CAP_OK") {
              Serial.println("[CLIENT] Status: l2cap done");
            }
          } else {
            Serial.println("[CLIENT] L2CAP receive timeout");
          }
          channel.disconnect();
        } else {
          Serial.println("[CLIENT] L2CAP channel connect timeout");
        }
      }
      l2capClient.disconnect();
    }
#endif
    if (!l2cap_supported) {
      Serial.println("[CLIENT] L2CAP not supported, skipping");
    }
  }

  // ===== Phase 15: Client-side introspection + fail-closed permissions =====
  // Exercises API surface that previous phases didn't touch:
  //   * BLEClient::getServices() / BLERemoteService::getCharacteristics()
  //   * BLEClient::getRSSI()
  //   * BLERemoteCharacteristic::can{Read,Write,WriteNoResponse,Notify,Indicate}()
  //   * BLERemoteCharacteristic::getDescriptors() / getDescriptor() / getHandle()
  // And verifies the new properties/permissions validation end-to-end: a char
  // declared as Read|Write with only OpenRead permission must be advertised
  // without the Write property (fail-closed masking) and must reject writes.
  waitForPhase(15);
  {
    // Reconnect with a fresh client — phase 14's client was disposed.
    BLEClient ic = BLE.createClient();
    installClientCallbacks(ic);
    BTStatus s = ic.connect(targetAddr);
    if (!s) {
      Serial.printf("[CLIENT] Introspect connect FAILED: %s\n", s.toString());
    } else {
      Serial.println("[CLIENT] Reconnected for introspection");

      int8_t rssi = ic.getRSSI();
      Serial.printf("[CLIENT] RSSI: %d\n", (int)rssi);

      // Call the aggregate getters first. The library auto-discovers when the
      // cache is empty, so both backends now behave symmetrically with the
      // targeted getService()/getCharacteristic() calls below.
      auto services = ic.getServices();
      Serial.printf("[CLIENT] Service count: %u\n", (unsigned)services.size());

      BLERemoteService isvc = ic.getService(serviceUUID);
      if (!isvc) {
        Serial.println("[CLIENT] Introspect service not found");
      } else {
        auto chars = isvc.getCharacteristics();
        Serial.printf("[CLIENT] Characteristic count: %u\n", (unsigned)chars.size());

        auto rw = isvc.getCharacteristic(rwCharUUID);
        auto nt = isvc.getCharacteristic(notifyCharUUID);
        auto ind = isvc.getCharacteristic(indicateCharUUID);
        auto wn = isvc.getCharacteristic(writeNrCharUUID);
        auto fc = isvc.getCharacteristic(permFailClosedCharUUID);

        // Property introspection — verifies both backends expose the right
        // property bits to the peer (after fail-closed masking).
        Serial.printf(
          "[CLIENT] RW: canRead=%d canWrite=%d canNotify=%d canIndicate=%d\n", (int)rw.canRead(), (int)rw.canWrite(), (int)rw.canNotify(), (int)rw.canIndicate()
        );
        Serial.printf("[CLIENT] Notify: canNotify=%d canIndicate=%d canRead=%d\n", (int)nt.canNotify(), (int)nt.canIndicate(), (int)nt.canRead());
        Serial.printf("[CLIENT] Indicate: canIndicate=%d canNotify=%d canRead=%d\n", (int)ind.canIndicate(), (int)ind.canNotify(), (int)ind.canRead());
        Serial.printf("[CLIENT] WriteNR: canWriteNoResponse=%d canWrite=%d\n", (int)wn.canWriteNoResponse(), (int)wn.canWrite());

        // Exercise the aggregate descriptor getter FIRST (no prior targeted
        // getDescriptor() call) to confirm it auto-discovers on both backends.
        auto ntDescriptors = nt.getDescriptors();
        Serial.printf("[CLIENT] Notify descriptor count: %u\n", (unsigned)ntDescriptors.size());
        auto cccd = nt.getDescriptor(BLEUUID(static_cast<uint16_t>(0x2902)));
        Serial.printf("[CLIENT] Notify CCCD found: %d\n", (int)(bool)cccd);

        // Fail-closed test: char declares Read|Write but only has OpenRead
        // permission. The backend must strip the Write property bit, so:
        //   - canRead()  -> true, canWrite() -> false
        //   - readValue() succeeds and returns "ro_data"
        //   - writeValue() returns a failure status (server rejects at ATT)
        if (!fc) {
          Serial.println("[CLIENT] FailClosed char not found");
        } else {
          Serial.printf("[CLIENT] FailClosed: canRead=%d canWrite=%d\n", (int)fc.canRead(), (int)fc.canWrite());
          String roVal = fc.readValue();
          Serial.printf("[CLIENT] FailClosed read: %s\n", roVal.c_str());
          BTStatus ws = fc.writeValue("should_fail");
          if (!ws) {
            Serial.printf("[CLIENT] FailClosed write rejected: %s\n", ws.toString());
          } else {
            Serial.println("[CLIENT] FailClosed write UNEXPECTEDLY succeeded");
          }
        }

        // Runtime enforcement of descriptor permissions. On DESC_CHAR the
        // server registers a custom-UUID descriptor with Read-only perms.
        // Expected behavior on both backends:
        //   - getDescriptor() finds it,
        //   - readValue() returns "ro_desc_val",
        //   - writeValue() fails at the ATT layer.
        // Previously Bluedroid hardcoded READ|WRITE for every descriptor,
        // silently ignoring the declared permissions — this write would
        // succeed. The new permToEsp()-based path must reject it.
        auto descChrRemote = isvc.getCharacteristic(descCharUUID);
        if (!descChrRemote) {
          Serial.println("[CLIENT] DescPerm descChr not found");
        } else {
          auto roDesc = descChrRemote.getDescriptor(descReadOnlyUUID);
          if (!roDesc) {
            Serial.println("[CLIENT] DescPerm RO descriptor not found");
          } else {
            Serial.println("[CLIENT] DescPerm RO descriptor found");
            String rv = roDesc.readValue();
            Serial.printf("[CLIENT] DescPerm RO read: %s\n", rv.c_str());
            BTStatus dws = roDesc.writeValue("nope");
            if (!dws) {
              Serial.printf("[CLIENT] DescPerm RO write rejected: %s\n", dws.toString());
            } else {
              Serial.println("[CLIENT] DescPerm RO write UNEXPECTEDLY succeeded");
            }
          }
        }
      }
      ic.disconnect();
      delay(500);
    }
    Serial.println("[CLIENT] Introspection done");
  }

  // ===========================================================================
  // Phase 16: Connection info + MTU/conn-params callbacks
  // ===========================================================================
  // Reconnect with a fresh client, force an MTU renegotiation by calling
  // setMTU before connect, trigger a conn-params update request through
  // BLEClient::updateConnParams, then sample both the client-side callback
  // state (via the globals set from installClientCallbacks()) and the
  // full BLEConnInfo introspection surface.
  waitForPhase(16);
  {
    clientMtuChanged = false;
    clientMtuLast = 0;
    clientConnParamsUpdated = false;

    BLEClient pc = BLE.createClient();
    installClientCallbacks(pc);
    pc.setMTU(247);  // non-default to force MTU exchange on connect
    BTStatus s = pc.connect(targetAddr);
    if (!s) {
      Serial.printf("[CLIENT] Phase16 connect FAILED: %s\n", s.toString());
    } else {
      Serial.println("[CLIENT] Phase16 connected");
      BLEConnInfo c = pc.getConnection();
      uint16_t mtu = pc.getMTU();
      BTAddress peer = pc.getPeerAddress();
      uint16_t h = pc.getHandle();
      Serial.printf("[CLIENT] ConnInfo handle=%u mtu=%u clientMtu=%u\n", (unsigned)c.getHandle(), (unsigned)c.getMTU(), (unsigned)mtu);
      Serial.printf("[CLIENT] ConnInfo peerAddr=%s handle=%u\n", peer.toString().c_str(), (unsigned)h);
      Serial.printf(
        "[CLIENT] ConnInfo sec enc=%d auth=%d bond=%d keySize=%u\n", (int)c.isEncrypted(), (int)c.isAuthenticated(), (int)c.isBonded(),
        (unsigned)c.getSecurityKeySize()
      );
      Serial.printf("[CLIENT] ConnInfo role central=%d peripheral=%d\n", (int)c.isCentral(), (int)c.isPeripheral());
      Serial.printf(
        "[CLIENT] ConnInfo params interval=%u latency=%u timeout=%u\n", (unsigned)c.getInterval(), (unsigned)c.getLatency(), (unsigned)c.getSupervisionTimeout()
      );
      Serial.printf("[CLIENT] ConnInfo phy tx=%u rx=%u\n", (unsigned)c.getTxPhy(), (unsigned)c.getRxPhy());
      Serial.printf("[CLIENT] ConnInfo rssi=%d\n", (int)c.getRSSI());

      // Trigger a conn-params update so the server's onConnParamsUpdate
      // callback has something to fire on. Use a noticeably different
      // interval so both sides see a real change.
      BLEConnParams np;
      np.minInterval = 24;  // 30 ms
      np.maxInterval = 40;  // 50 ms
      np.latency = 0;
      np.timeout = 400;  // 4 s
      BTStatus us = pc.updateConnParams(np);
      Serial.printf("[CLIENT] UpdateConnParams ok=%d\n", (int)(bool)us);
      delay(2500);  // allow the controller to apply the new params

      Serial.printf("[CLIENT] MtuChanged seen=%d last=%u\n", (int)clientMtuChanged, (unsigned)clientMtuLast);

      pc.disconnect();
      delay(1000);
    }
    Serial.println("[CLIENT] Phase16 done");
  }

  // ===========================================================================
  // Phase 17: Address types — API surface + connection roundtrip per type
  // ===========================================================================
  // (a) Call BLE.setOwnAddressType with every BTAddress::Type enum value.
  // (b) For each type the server rebuilds advertising with, scan for the
  //     per-type tagged name "T17_<TypeName>_<serverName>", connect, sample
  //     the peer's BTAddress::Type, and disconnect. This is the real
  //     end-to-end verification that BTAddress::Type propagates through
  //     both the adv PDU and the connection establishment path.
  waitForPhase(17);
  {
    BTAddress self = BLE.getAddress();
    Serial.printf("[CLIENT] OwnAddress addr=%s type=%u\n", self.toString().c_str(), (unsigned)self.type());

    // (a) API-surface coverage.
    BTStatus s1 = BLE.setOwnAddressType(BTAddress::Public);
    Serial.printf("[CLIENT] setOwnAddressType(Public) ok=%d\n", (int)(bool)s1);
    BTStatus s2 = BLE.setOwnAddressType(BTAddress::Random);
    Serial.printf("[CLIENT] setOwnAddressType(Random) ok=%d\n", (int)(bool)s2);
    BTStatus s3 = BLE.setOwnAddressType(BTAddress::PublicID);
    Serial.printf("[CLIENT] setOwnAddressType(PublicID) ok=%d\n", (int)(bool)s3);
    BTStatus s4 = BLE.setOwnAddressType(BTAddress::RandomID);
    Serial.printf("[CLIENT] setOwnAddressType(RandomID) ok=%d\n", (int)(bool)s4);
    (void)BLE.setOwnAddressType(self.type());

    // (b) Connection roundtrip per type. The server cycles through Public
    //     and Random, each tagged by a per-type name in the scan response.
    struct TypeSpec {
      const char *name;
    };
    const TypeSpec types[] = {{"Public"}, {"Random"}};
    for (const auto &ts : types) {
      String hitName = String("T17_") + ts.name + "_" + targetName;

      BLEScan scn = BLE.getScan();
      scn.resetCallbacks();
      scn.clearResults();
      scn.setActiveScan(true);  // need scan-response parsing to see the tag
      scn.setFilterDuplicates(false);
      BTAddress hitAddr;
      uint8_t hitType = 0xFF;
      bool hit = false;
      unsigned long deadline = millis() + 10000;
      while (!hit && millis() < deadline) {
        BLEScanResults res = scn.startBlocking(3000);
        for (const auto &dev : res) {
          BLEAdvertisedDevice d = dev;
          if (d.getName() == hitName) {
            hitAddr = d.getAddress();
            hitType = (uint8_t)d.getAddressType();
            hit = true;
            break;
          }
        }
        scn.clearResults();
      }
      Serial.printf("[CLIENT] Phase17 scan type=%s hit=%d advType=%u\n", ts.name, (int)hit, (unsigned)hitType);

      if (!hit) {
        continue;
      }

      BLEClient tc = BLE.createClient();
      installClientCallbacks(tc);
      BTStatus cs = tc.connect(hitAddr);
      Serial.printf("[CLIENT] Phase17 connect type=%s ok=%d\n", ts.name, (int)(bool)cs);
      if (cs) {
        BTAddress peer = tc.getPeerAddress();
        Serial.printf("[CLIENT] Phase17 peer type=%s addr=%s connType=%u\n", ts.name, peer.toString().c_str(), (unsigned)peer.type());
        tc.disconnect();
        delay(800);
      }
    }

    Serial.println("[CLIENT] Phase17 done");
  }

  // ===========================================================================
  // Phase 18: Authorization flow
  // ===========================================================================
  // The AUTHZ char has ReadAuthorized|WriteAuthorized. Every access traps
  // through BLESecurity::onAuthorization on the server. The handler:
  //   - always approves reads,
  //   - approves writes with payload "OK",
  //   - denies writes with any other payload.
  // We exercise all three branches and assert the expected statuses.
  waitForPhase(18);
  {
    // Phase 17 cycled own-address type on the server, so targetAddr is no
    // longer guaranteed to match the current adv. Re-scan by name.
    {
      BLEScan scn = BLE.getScan();
      scn.resetCallbacks();
      scn.clearResults();
      scn.setActiveScan(true);
      scn.setFilterDuplicates(false);
      unsigned long deadline = millis() + 6000;
      while (millis() < deadline) {
        BLEScanResults res = scn.startBlocking(2000);
        bool found = false;
        for (const auto &dev : res) {
          BLEAdvertisedDevice d = dev;
          if (d.getName() == targetName) {
            targetAddr = d.getAddress();
            found = true;
            break;
          }
        }
        scn.clearResults();
        if (found) {
          break;
        }
      }
    }
    BLEClient ac = BLE.createClient();
    installClientCallbacks(ac);
    BTStatus s = ac.connect(targetAddr);
    if (!s) {
      Serial.printf("[CLIENT] Phase18 connect FAILED: %s\n", s.toString());
    } else {
      Serial.println("[CLIENT] Phase18 connected");
      BLERemoteService as = ac.getService(serviceUUID);
      BLERemoteCharacteristic ach = as.getCharacteristic(authzCharUUID);
      if (!ach) {
        Serial.println("[CLIENT] Authz char not found");
      } else {
        String rv = ach.readValue();
        Serial.printf("[CLIENT] Authz read: %s\n", rv.c_str());
        BTStatus wok = ach.writeValue(String("OK"));
        Serial.printf("[CLIENT] Authz write OK status: %d\n", (int)(bool)wok);
        delay(200);
        BTStatus wno = ach.writeValue(String("NO"));
        Serial.printf("[CLIENT] Authz write NO status: %d\n", (int)(bool)wno);
        delay(200);
        // Readback should return the last APPROVED value ("OK"), not "NO".
        String rb = ach.readValue();
        Serial.printf("[CLIENT] Authz readback: %s\n", rb.c_str());
      }
      ac.disconnect();
      delay(500);
    }
    Serial.println("[CLIENT] Phase18 done");
  }

  // ===========================================================================
  // Phase 19: Encrypted-permission enforcement (post-pair positive control)
  // ===========================================================================
  // Link has been paired since phase 10; the bond persists in NVS across
  // disconnects. The encrypted char should be readable + writable and
  // BLEConnInfo should report encrypted=1 bonded=1 (authenticated=1 when
  // MITM was used by phase 10).
  waitForPhase(19);
  {
    // Refresh the server's address in case earlier phases rotated it.
    {
      BLEScan scn = BLE.getScan();
      scn.resetCallbacks();
      scn.clearResults();
      scn.setActiveScan(true);
      scn.setFilterDuplicates(false);
      unsigned long deadline = millis() + 6000;
      while (millis() < deadline) {
        BLEScanResults res = scn.startBlocking(2000);
        bool found = false;
        for (const auto &dev : res) {
          BLEAdvertisedDevice d = dev;
          if (d.getName() == targetName) {
            targetAddr = d.getAddress();
            found = true;
            break;
          }
        }
        scn.clearResults();
        if (found) {
          break;
        }
      }
    }
    BLEClient ec = BLE.createClient();
    installClientCallbacks(ec);
    BTStatus s = ec.connect(targetAddr);
    if (!s) {
      Serial.printf("[CLIENT] Phase19 connect FAILED: %s\n", s.toString());
    } else {
      Serial.println("[CLIENT] Phase19 connected");
      BLERemoteService es = ec.getService(serviceUUID);
      BLERemoteCharacteristic ech = es.getCharacteristic(encryptedCharUUID);
      String rv = ech.readValue();
      Serial.printf("[CLIENT] Enc read: %s\n", rv.c_str());
      BTStatus ws = ech.writeValue(String("enc_ok"));
      Serial.printf("[CLIENT] Enc write ok=%d\n", (int)(bool)ws);
      delay(200);
      BLEConnInfo c = ec.getConnection();
      Serial.printf("[CLIENT] Phase19 sec enc=%d auth=%d bond=%d\n", (int)c.isEncrypted(), (int)c.isAuthenticated(), (int)c.isBonded());
      ec.disconnect();
      delay(500);
    }
    Serial.println("[CLIENT] Phase19 done");
  }

  // ===========================================================================
  // Phase 20: Advertisement data builders + scan tuning
  // ===========================================================================
  // The server rebuilds its advertisement with every BLEAdvertisementData
  // setter. Configure a passive scan with custom interval/window and duplicate
  // filtering, run startBlocking so we can iterate getResults(), and assert
  // that every parser getter returns the server's advertised value.
  waitForPhase(20);
  {
    BLEScan scn = BLE.getScan();
    // A lingering background scan from earlier phases will steal radio
    // time and drop scan responses; make sure the radio is quiet before
    // we start the measurement window.
    scn.stop();
    delay(100);
    scn.resetCallbacks();
    scn.clearResults();
    scn.clearDuplicateCache();
    scn.setActiveScan(true);  // need scan response for short-name
    scn.setFilterDuplicates(false);
    scn.setInterval(160);  // 100 ms
    scn.setWindow(96);     // 60 ms
    bool sawAdv = false;
    String wantName = String("ADV_") + targetName;
    BLEScanResults results = scn.startBlocking(8000);
    Serial.printf("[CLIENT] Phase20 results=%u isScanning=%d want=%s\n", (unsigned)results.getCount(), (int)scn.isScanning(), wantName.c_str());
    for (const auto &dev : results) {
      BLEAdvertisedDevice d = dev;
      String n = d.getName();
      Serial.printf("[CLIENT] Phase20 seen name=\"%s\" addr=%s\n", n.c_str(), d.getAddress().toString().c_str());
      if (n == wantName) {
        sawAdv = true;
        Serial.printf("[CLIENT] Phase20 name=%s rssi=%d txPower=%d\n", n.c_str(), (int)d.getRSSI(), (int)d.getTXPower());
        Serial.printf(
          "[CLIENT] Phase20 haveName=%d haveRSSI=%d haveTxPow=%d haveAppearance=%d haveMfg=%d haveSvcData=%d haveSvcUUID=%d\n", (int)d.haveName(),
          (int)d.haveRSSI(), (int)d.haveTXPower(), (int)d.haveAppearance(), (int)d.haveManufacturerData(), (int)d.haveServiceData(), (int)d.haveServiceUUID()
        );
        Serial.printf(
          "[CLIENT] Phase20 appearance=0x%04X addrType=%u connectable=%d legacy=%d\n", (unsigned)d.getAppearance(), (unsigned)d.getAddressType(),
          (int)d.isConnectable(), (int)d.isLegacyAdvertisement()
        );
        Serial.printf("[CLIENT] Phase20 svcUUIDs=%u first=%s\n", (unsigned)d.getServiceUUIDCount(), d.getServiceUUID(0).toString().c_str());
        Serial.printf(
          "[CLIENT] Phase20 mfgCompany=0x%04X mfgLen=%u\n", (unsigned)d.getManufacturerCompanyId(), (unsigned)d.getManufacturerDataString().length()
        );
        size_t svcLen = 0;
        const uint8_t *svcRaw = d.getServiceData(0, &svcLen);
        BLEUUID svc16((uint16_t)0x180D);
        Serial.printf(
          "[CLIENT] Phase20 svcDataLen=%u advertisesSvc=%d payloadLen=%u\n", (unsigned)svcLen, (int)d.isAdvertisingService(svc16),
          (unsigned)d.getPayloadLength()
        );
        (void)svcRaw;
        break;
      }
    }
    Serial.printf("[CLIENT] Phase20 sawAdv=%d\n", (int)sawAdv);
    scn.clearResults();
    scn.setActiveScan(true);
    Serial.println("[CLIENT] Phase20 done");
  }

  // ===========================================================================
  // Phase 21: Beacon / Eddystone build + parse
  // ===========================================================================
  // Server rotates three payloads (iBeacon → Eddystone URL → Eddystone TLM),
  // each for ~4 s. We rely on BLEAdvertisedDevice::getFrameType() to route
  // each hit to the matching parser. The payload accessors below (mfgData,
  // serviceData) return the same structures the classifier inspected so we
  // don't need to redo any AD parsing.
  waitForPhase(21);
  {
    bool sawIBeacon = false, sawEddyUrl = false, sawEddyTlm = false;
    BLEScan scn = BLE.getScan();
    scn.resetCallbacks();
    scn.clearDuplicateCache();
    scn.setActiveScan(false);
    scn.setFilterDuplicates(false);
    const BLEUUID eddyUUID((uint16_t)0xFEAA);
    unsigned long deadline = millis() + 18000;
    while (millis() < deadline && (!sawIBeacon || !sawEddyUrl || !sawEddyTlm)) {
      BLEScanResults res = scn.startBlocking(3000);
      for (const auto &dev : res) {
        BLEAdvertisedDevice d = dev;
        switch (d.getFrameType()) {
          case BLEAdvertisedDevice::IBeacon:
          {
            if (sawIBeacon) {
              break;
            }
            size_t mfgLen = 0;
            const uint8_t *mfg = d.getManufacturerData(&mfgLen);
            // BLEBeacon::setFromPayload expects the stream *after* the
            // 2-byte company ID.
            if (mfg && mfgLen >= 25) {
              BLEBeacon b;
              b.setFromPayload(mfg + 2, mfgLen - 2);
              Serial.printf(
                "[CLIENT] Phase21 iBeacon major=0x%04X minor=0x%04X power=%d uuid=%s\n", (unsigned)b.getMajor(), (unsigned)b.getMinor(),
                (int)b.getSignalPower(), b.getProximityUUID().toString().c_str()
              );
              sawIBeacon = true;
            }
            break;
          }
          case BLEAdvertisedDevice::EddystoneURL:
          {
            if (sawEddyUrl) {
              break;
            }
            for (size_t i = 0; i < d.getServiceDataCount(); i++) {
              if (!(d.getServiceDataUUID(i) == eddyUUID)) {
                continue;
              }
              size_t sdLen = 0;
              const uint8_t *sd = d.getServiceData(i, &sdLen);
              if (!sd || sdLen < 2) {
                continue;
              }
              BLEEddystoneURL url;
              url.setFromServiceData(sd, sdLen);
              Serial.printf("[CLIENT] Phase21 EddyURL url=%s tx=%d\n", url.getURL().c_str(), (int)url.getTxPower());
              sawEddyUrl = true;
              break;
            }
            break;
          }
          case BLEAdvertisedDevice::EddystoneTLM:
          {
            if (sawEddyTlm) {
              break;
            }
            for (size_t i = 0; i < d.getServiceDataCount(); i++) {
              if (!(d.getServiceDataUUID(i) == eddyUUID)) {
                continue;
              }
              size_t sdLen = 0;
              const uint8_t *sd = d.getServiceData(i, &sdLen);
              if (!sd || sdLen < 2) {
                continue;
              }
              BLEEddystoneTLM tlm;
              tlm.setFromServiceData(sd, sdLen);
              Serial.printf(
                "[CLIENT] Phase21 EddyTLM volt=%u temp=%d cnt=%lu up=%lu\n", (unsigned)tlm.getBatteryVoltage(), (int)tlm.getTemperature(),
                (unsigned long)tlm.getAdvertisingCount(), (unsigned long)tlm.getUptime()
              );
              sawEddyTlm = true;
              break;
            }
            break;
          }
          default: break;
        }
      }
      scn.clearResults();
      scn.clearDuplicateCache();
    }
    Serial.printf("[CLIENT] Phase21 sawIBeacon=%d sawEddyURL=%d sawEddyTLM=%d\n", (int)sawIBeacon, (int)sawEddyUrl, (int)sawEddyTlm);
    Serial.println("[CLIENT] Phase21 done");
  }

  // ===========================================================================
  // Phase 22: Bond + whitelist management
  // ===========================================================================
  // Log bonds, round-trip the whitelist, then (optionally) exercise
  // deleteBond — but keep the bond intact so later phases that rely on
  // encryption still work without triggering an interactive re-pair.
  waitForPhase(22);
  {
    BLESecurity s = BLE.getSecurity();
    auto bonds = s.getBondedDevices();
    Serial.printf("[CLIENT] Bond count: %u\n", (unsigned)bonds.size());
    for (size_t i = 0; i < bonds.size() && i < 4; i++) {
      Serial.printf("[CLIENT] Bond[%u]=%s\n", (unsigned)i, bonds[i].toString().c_str());
    }
    BTStatus a = BLE.whiteListAdd(targetAddr);
    bool on = BLE.isOnWhiteList(targetAddr);
    BTStatus r = BLE.whiteListRemove(targetAddr);
    bool after = BLE.isOnWhiteList(targetAddr);
    Serial.printf("[CLIENT] Whitelist add=%d isOn=%d remove=%d after=%d\n", (int)(bool)a, (int)on, (int)(bool)r, (int)after);
    // See server Phase 22 for the matching lines — the Python harness
    // cross-checks local(A) == peer(B) and vice versa.
    String localIrkHex = BLE.getLocalIRKString();
    Serial.flush();
    delay(50);
    Serial.printf("[CLIENT] LocalIRK present=%d hex=%s\n", (int)(localIrkHex.length() > 0), localIrkHex.c_str());
    Serial.flush();
    delay(50);
    if (!bonds.empty()) {
      String peerIrkHex = BLE.getPeerIRKString(bonds[0]);
      Serial.printf("[CLIENT] PeerIRK present=%d hex=%s\n", (int)(peerIrkHex.length() > 0), peerIrkHex.c_str());
      Serial.flush();
      delay(50);
    } else {
      Serial.println("[CLIENT] PeerIRK present=0 hex=");
      Serial.flush();
      delay(50);
    }
    Serial.println("[CLIENT] Phase22 done");
  }

  // ===========================================================================
  // Phase 23: Error paths + miscellaneous coverage
  // ===========================================================================
  // Unknown service/char lookups, ops-after-disconnect, connectAsync/cancel,
  // typed readUInt* and readRawData paths, UUID algebra, operator bool.
  waitForPhase(23);
  {
    // Earlier phases (17, 20, 21) have repeatedly torn down adv, rotated
    // own-address type, and rebuilt payloads — any of which can stale the
    // cached targetAddr. Do a short re-scan by name before reconnecting so
    // we pick up the server's current advertised address.
    bool rescanFound = false;
    {
      BLEScan scn = BLE.getScan();
      scn.stop();
      delay(100);
      scn.resetCallbacks();
      scn.clearResults();
      scn.clearDuplicateCache();
      scn.setActiveScan(true);
      scn.setFilterDuplicates(false);
      unsigned long deadline = millis() + 10000;
      while (millis() < deadline) {
        BLEScanResults res = scn.startBlocking(3000);
        for (const auto &dev : res) {
          BLEAdvertisedDevice d = dev;
          if (d.getName() == targetName) {
            targetAddr = d.getAddress();
            rescanFound = true;
            break;
          }
        }
        scn.clearResults();
        if (rescanFound) {
          break;
        }
      }
    }
    Serial.printf("[CLIENT] Phase23 rescan found=%d addr=%s\n", (int)rescanFound, targetAddr.toString().c_str());
    BLEClient ec = BLE.createClient();
    installClientCallbacks(ec);
    BTStatus cs = ec.connect(targetAddr);
    if (!cs) {
      Serial.printf("[CLIENT] Phase23 connect FAILED: %s\n", cs.toString());
    } else {
      Serial.println("[CLIENT] Phase23 connected");
      BLERemoteService unk = ec.getService(unknownSvcUUID);
      Serial.printf("[CLIENT] Phase23 getService unknown ok=%d\n", (int)(bool)unk);

      BLERemoteService good = ec.getService(serviceUUID);
      BLERemoteCharacteristic unkC = good.getCharacteristic(unknownCharUUID);
      Serial.printf("[CLIENT] Phase23 getChar unknown ok=%d\n", (int)(bool)unkC);

      BLERemoteCharacteristic rwC = good.getCharacteristic(rwCharUUID);
      // Typed reads — any value fits in a uint8, so we just check that
      // the API returns something without crashing.
      uint8_t u8 = rwC.readUInt8();
      uint16_t u16 = rwC.readUInt16();
      uint32_t u32 = rwC.readUInt32();
      float f32 = rwC.readFloat();
      Serial.printf("[CLIENT] Phase23 typed u8=%u u16=%u u32=%lu f32=%d\n", (unsigned)u8, (unsigned)u16, (unsigned long)u32, (int)f32);

      uint8_t buf[32] = {0};
      size_t rLen = rwC.readValue(buf, sizeof(buf));
      Serial.printf("[CLIENT] Phase23 readBuf len=%u\n", (unsigned)rLen);
      size_t rawLen = 0;
      const uint8_t *raw = rwC.readRawData(&rawLen);
      Serial.printf("[CLIENT] Phase23 readRaw len=%u nonNull=%d\n", (unsigned)rawLen, (int)(raw != nullptr));

      // UUID algebra
      BLEUUID ua(static_cast<uint16_t>(0x180D));
      BLEUUID ub(static_cast<uint16_t>(0x180D));
      BLEUUID uc(static_cast<uint16_t>(0x180E));
      Serial.printf("[CLIENT] Phase23 UUID eq=%d ne=%d lt=%d\n", (int)(ua == ub), (int)(ua != uc), (int)(ua < uc));
      BLEUUID u128 = ua.to128();
      Serial.printf("[CLIENT] Phase23 UUID bitSize16=%u 128=%u toUint16=0x%04X\n", (unsigned)ua.bitSize(), (unsigned)u128.bitSize(), (unsigned)ua.toUint16());
      BLEUUID ubad;
      Serial.printf("[CLIENT] Phase23 UUID valid=%d defaultBool=%d\n", (int)ua.isValid(), (int)(bool)ubad);

      // Disconnect + op-after-disconnect
      ec.disconnect();
      delay(1500);
      Serial.printf("[CLIENT] Phase23 postDisc isConnected=%d\n", (int)ec.isConnected());
      BTStatus rwAfter = rwC.writeValue(String("after"));
      Serial.printf("[CLIENT] Phase23 writeAfterDisc ok=%d\n", (int)(bool)rwAfter);

      // connectAsync + cancelConnect
      BLEClient ac = BLE.createClient();
      BTStatus as = ac.connectAsync(targetAddr);
      Serial.printf("[CLIENT] Phase23 connectAsync ok=%d\n", (int)(bool)as);
      delay(100);
      BTStatus cancel = ac.cancelConnect();
      Serial.printf("[CLIENT] Phase23 cancelConnect ok=%d\n", (int)(bool)cancel);
    }
    Serial.println("[CLIENT] Phase23 done");
  }

  // ===========================================================================
  // Phase 24: BLE5 advanced (coded PHY, default PHY)
  // ===========================================================================
  waitForPhase(24);
  {
#if BLE5_SUPPORTED
    BLEPhy tx = BLEPhy::PHY_1M, rx = BLEPhy::PHY_1M;
    BTStatus gd = BLE.getDefaultPhy(tx, rx);
    Serial.printf("[CLIENT] Phase24 getDefaultPhy ok=%d tx=%u rx=%u\n", (int)(bool)gd, (unsigned)tx, (unsigned)rx);
    BTStatus sd = BLE.setDefaultPhy(BLEPhy::PHY_1M, BLEPhy::PHY_1M);
    Serial.printf("[CLIENT] Phase24 setDefaultPhy ok=%d\n", (int)(bool)sd);

    // Exercise coded-PHY path on the scanner. It's OK if the hardware
    // doesn't actually see anything; we only care about the API not
    // faulting.
    BLEScan scn = BLE.getScan();
    scn.resetCallbacks();
    BLEScan::ExtScanConfig coded;
    coded.phy = BLEPhy::PHY_Coded;
    coded.interval = 0x40;
    coded.window = 0x40;
    BTStatus es = scn.startExtended(1500, &coded, nullptr);
    Serial.printf("[CLIENT] Phase24 startExtendedCoded ok=%d\n", (int)(bool)es);
    delay(2000);
    scn.stopExtended();
    Serial.println("[CLIENT] Phase24 done");
#else
    Serial.println("[CLIENT] Phase24 BLE5 not supported, skipping");
    Serial.println("[CLIENT] Phase24 done");
#endif
  }

  // ===========================================================================
  // Phase 25: HID-over-GATT spec compliance
  // ===========================================================================
  // Server rebuilds its stack with the HID service. We scan, connect, and
  // verify all HoGP/HIDS spec features:
  //   - HID Service (0x1812) discoverable
  //   - Battery Service (0x180F) discoverable
  //   - Report chars (0x2A4D): at least 2 (input + output)
  //   - Boot Keyboard Input (0x2A22): Read + Notify
  //   - Boot Keyboard Output (0x2A29): Read + Write + WriteNR
  //   - Report Map (0x2A4B): External Report Reference descriptor (0x2907)
  waitForPhase(25);
  {
    BLEScan scn = BLE.getScan();
    scn.resetCallbacks();
    scn.clearResults();
    scn.setActiveScan(true);
    scn.setFilterDuplicates(false);
    bool found = false;
    String hidName = String("HID_") + targetName;
    BTAddress hidAddr;
    unsigned long deadline = millis() + 15000;
    while (!found && millis() < deadline) {
      BLEScanResults res = scn.startBlocking(3000);
      for (const auto &dev : res) {
        BLEAdvertisedDevice d = dev;
        if (d.getName() == hidName) {
          hidAddr = d.getAddress();
          found = true;
          break;
        }
      }
      scn.clearResults();
    }

    if (!found) {
      Serial.println("[CLIENT] Phase25 HID server not found");
    } else {
      Serial.println("[CLIENT] Phase25 HID found");
      BLEClient hc = BLE.createClient();
      installClientCallbacks(hc);
      BTStatus s = hc.connect(hidAddr, 10000);
      if (!s) {
        Serial.printf("[CLIENT] Phase25 HID connect FAILED: %s\n", s.toString());
      } else {
        auto svcs = hc.getServices();
        BLEUUID hidSvcUUID((uint16_t)0x1812);
        BLEUUID batSvcUUID((uint16_t)0x180F);
        bool haveHid = false;
        bool haveBat = false;
        for (auto &rs : svcs) {
          if (rs.getUUID() == hidSvcUUID) haveHid = true;
          if (rs.getUUID() == batSvcUUID) haveBat = true;
        }
        Serial.printf("[CLIENT] Phase25 services=%u haveHid=%d haveBat=%d\n",
                       (unsigned)svcs.size(), (int)haveHid, (int)haveBat);

        BLERemoteService hidSvc = hc.getService(hidSvcUUID);
        if (hidSvc) {
          auto chars = hidSvc.getCharacteristics();
          Serial.printf("[CLIENT] Phase25 hidChars=%u\n", (unsigned)chars.size());

          BLEUUID reportUUID((uint16_t)0x2A4D);
          BLEUUID bootInUUID((uint16_t)0x2A22);
          BLEUUID bootOutUUID((uint16_t)0x2A32);
          BLEUUID reportMapUUID((uint16_t)0x2A4B);

          int reportCount = 0;
          bool bootInOk = false;
          bool bootOutOk = false;
          bool extRefFound = false;

          for (auto &ch : chars) {
            BLEUUID u = ch.getUUID();
            if (u == reportUUID) {
              reportCount++;
            }
            if (u == bootInUUID) {
              bootInOk = ch.canRead() && ch.canNotify();
              Serial.printf("[CLIENT] Phase25 bootIn canRead=%d canNotify=%d\n",
                             (int)ch.canRead(), (int)ch.canNotify());
            }
            if (u == bootOutUUID) {
              bootOutOk = ch.canRead() && ch.canWrite() && ch.canWriteNoResponse();
              Serial.printf("[CLIENT] Phase25 bootOut canRead=%d canWrite=%d canWriteNR=%d\n",
                             (int)ch.canRead(), (int)ch.canWrite(), (int)ch.canWriteNoResponse());
            }
            if (u == reportMapUUID) {
              BLEUUID extRefUUID((uint16_t)0x2907);
              BLERemoteDescriptor extRef = ch.getDescriptor(extRefUUID);
              extRefFound = (bool)extRef;
              Serial.printf("[CLIENT] Phase25 extRefDesc=%d\n", (int)extRefFound);
            }
          }

          Serial.printf("[CLIENT] Phase25 reportCount=%d\n", reportCount);
          Serial.printf("[CLIENT] Phase25 bootInOk=%d bootOutOk=%d\n",
                         (int)bootInOk, (int)bootOutOk);
        }
        hc.disconnect();
      }
    }
    Serial.println("[CLIENT] Phase25 done");
  }

  // ===== Phase 26: Memory release + reinit guard =====
  waitForPhase(26);

  size_t heapBeforeRelease = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  Serial.printf("[CLIENT] Heap before release: %u\n", (unsigned)heapBeforeRelease);

  BLE.end(true);

  delay(500);
  size_t heapAfterRelease = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  Serial.printf("[CLIENT] Heap after release: %u\n", (unsigned)heapAfterRelease);

  int32_t freed = (int32_t)heapAfterRelease - (int32_t)heapBeforeRelease;
  Serial.printf("[CLIENT] Memory freed: %ld bytes\n", (long)freed);
  if (freed >= 10240) {
    Serial.println("[CLIENT] Memory release OK");
  } else {
    Serial.println("[CLIENT] Memory release INSUFFICIENT");
  }

  status = BLE.begin("ShouldFail");
  if (!status) {
    Serial.println("[CLIENT] Reinit blocked OK");
  } else {
    Serial.println("[CLIENT] Reinit was NOT blocked");
  }

  Serial.println("[CLIENT] All phases complete");
}

void loop() {
  delay(1000);
}
