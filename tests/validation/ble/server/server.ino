// Combined BLE validation test — SERVER
// Phases: basic lifecycle, BLE5 ext+periodic adv, GATT server, notifications,
//         large writes, descriptors, write-no-response, server disconnect,
//         security, reconnect

#include <Arduino.h>
#include <BLE.h>
#include "esp_heap_caps.h"

#define SERVICE_UUID       "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define RW_CHAR_UUID       "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define NOTIFY_CHAR_UUID   "cba1d466-344c-4be3-ab3f-189f80dd7518"
#define INDICATE_CHAR_UUID "d5f782b2-a36e-4d68-947c-0e9a5f2c78e1"
#define SECURE_CHAR_UUID   "ff1d2614-e2d6-4c87-9154-6625d39ca7f8"
#define DESC_CHAR_UUID     "a3c87501-8ed3-4bdf-8a39-a01bebede295"
#define WRITENR_CHAR_UUID  "1c95d5e3-d8f7-413a-bf3d-7a2e5d7be87e"
// Phase 15 (introspection_and_permissions): R|W properties but only OpenRead
// permission. Exercises the fail-closed mapping: the Write property must be
// stripped by the backend so the peer sees it as read-only, and writes must
// be rejected at the ATT layer.
#define PERM_FAIL_CLOSED_CHAR_UUID "e8b1d3a1-6b3e-4a5d-9f1d-9b7e5c3a2f41"

// Phase 15 (descriptor permissions): custom-UUID descriptor attached to
// DESC_CHAR with read-only permission. The client reads it (expect success)
// then attempts a write (expect ATT-layer rejection), proving that
// bleValidateDescProps + the BluedroidServer descPerm translation enforce
// the declared descriptor permissions end-to-end.
#define DESC_READONLY_UUID "b5f3e2c1-8a9e-4b7c-9f3d-2e8a5b7c3f91"

// --- New UUIDs introduced by phases 16+ ---------------------------------
// Phase 18: application-level access gating via BLESecurity::onAuthorization.
// Declares ReadAuthorized|WriteAuthorized so every read/write traps through
// the handler. The handler approves reads and specific write payloads and
// denies everything else, exercising both approve and deny branches.
#define AUTHZ_CHAR_UUID "ca11aaaa-1111-4222-8333-444455556666"

// Phase 19: EncryptedReadWrite permissions. Pre-pair access will fail with
// insufficient-encryption; post-pair access succeeds. Used by both the
// encrypted_perm_enforcement phase and the bond_and_whitelist phase (after
// the bond is erased, the next read must re-trigger pairing).
#define ENCRYPTED_CHAR_UUID "ca11bbbb-1111-4222-8333-444455556666"

// Phase 23 helpers: fake UUIDs that must NOT be discoverable on the server,
// and an extra writable char used to assert ops-after-disconnect failure.
#define UNKNOWN_SVC_UUID  "deadbeef-0000-0000-0000-000000000001"
#define UNKNOWN_CHAR_UUID "deadbeef-0000-0000-0000-000000000002"

String serverName;
BLECharacteristic notifyChr;
BLECharacteristic indicateChr;
BLECharacteristic descChr;            // exposed to loop() so Phase 15 can
                                      // run construction-time validator
                                      // tests against a real char handle
BLECharacteristic permFailClosedChr;  // same rationale
BLECharacteristic authzChr;           // phase 18
BLECharacteristic encryptedChr;       // phase 19
int notifyStep = 0;
volatile int currentPhase = 0;
bool serverDisconnectDone = false;
bool phase10BondsCleared = false;
bool descPermValidationDone = false;

// Phase 16-26 completion flags (each phase fires once)
bool phase16Done = false;
bool phase17Done = false;
bool phase18Started = false;  // phase 18 is event-driven; no single "done" marker
bool phase19Done = false;
bool phase20Done = false;
bool phase21Done = false;
bool phase22Done = false;
bool phase23Done = false;
bool phase24Done = false;
bool phase25Done = false;
bool phase26Done = false;

// Phase 16 — server callback aggregation (set from BLE stack task, read from loop).
// Without these we'd race the MTU/conn-param update events against the phase
// coordination token. Storing the last-seen values lets loop() log them
// deterministically AFTER the phase start marker.
volatile bool phase16MtuSeen = false;
volatile uint16_t phase16LastMtu = 0;
volatile bool phase16ConnParamsSeen = false;
volatile uint16_t phase16LastInterval = 0;
volatile uint16_t phase16LastLatency = 0;
volatile uint16_t phase16LastTimeout = 0;

// Phase 18 authorization counters — touched from callback (stack task) and
// read/logged from loop() to avoid interleaving with Serial prints from the
// stack task.
volatile uint32_t authzReadApprovals = 0;
volatile uint32_t authzWriteApprovals = 0;
volatile uint32_t authzWriteDenials = 0;
volatile uint16_t authzLastAttrHandle = 0;
// Per-write toggle used by the Phase 18 authorization handler. The BLE spec
// treats the Write-Authorized bit as a per-(connection, attribute) policy —
// the write payload is NOT available to the handler because the ATT layer
// dispatches authorization before applying the value. We therefore
// alternate "approve / deny" across consecutive writes so the test can
// observe one approval and one denial from the same characteristic.
volatile bool authzApproveNextWrite = true;

// Phase 24 BLE5 advanced — last PHY/DLE state observed by server callbacks.
volatile uint8_t phase24TxPhy = 0;
volatile uint8_t phase24RxPhy = 0;

BLEStream bleStream;
bool bleStreamInitDone = false;
bool bleStreamPhaseDone = false;
int bleStreamStep = 0;
String bleStreamRxLine;
volatile bool bleStreamConnectCb = false;
volatile bool bleStreamDisconnectCb = false;
bool bleStreamPeekDone = false;
bool l2capInitDone = false;

#if BLE_L2CAP_SUPPORTED
BLEL2CAPServer l2capServer;
BLEL2CAPChannel l2capChannel;
#endif

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
          Serial.printf("[SERVER] Phase %d started\n", phase);
        }
      }
      buf = "";
    } else if (c != '\r') {
      buf += c;
    }
  }
}

// BLE callbacks run on the stack's context and may preempt loop() before it calls
// checkSerial(). Drain UART first so START_PHASE_* is applied and [SERVER] Phase N started
// always precedes phase-dependent GATT logs (host and pexpect stay in order).
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
  Serial.println("[SERVER] Device ready for name");
  Serial.println("[SERVER] Send name:");
  while (serverName.length() == 0) {
    if (Serial.available()) {
      serverName = Serial.readStringUntil('\n');
      serverName.trim();
    }
    delay(100);
  }
  Serial.printf("[SERVER] Name: %s\n", serverName.c_str());
}

// ========================= Phase 1 — Basic Lifecycle =========================

bool phase_basic() {
  BTStatus status = BLE.begin(serverName);
  if (!status) {
    Serial.printf("[SERVER] Init FAILED: %s\n", status.toString());
    return false;
  }
  Serial.printf("[SERVER] Stack: %s\n", BLE.getStackName());
  Serial.println("[SERVER] Init OK");
  Serial.printf("[SERVER] Device name: %s\n", BLE.getDeviceName().c_str());
  Serial.printf("[SERVER] Address: %s\n", BLE.getAddress().toString().c_str());

  BLE.end(false);
  Serial.println("[SERVER] Deinit OK");
  delay(1000);

  status = BLE.begin(serverName + "_reinit");
  if (!status) {
    Serial.printf("[SERVER] Reinit FAILED: %s\n", status.toString());
    return false;
  }
  Serial.println("[SERVER] Reinit OK");

  BLE.end(false);
  Serial.println("[SERVER] Final deinit OK");
  return true;
}

// ============== Phase 2 — BLE5 Extended + Periodic Advertising ===============

#if BLE5_SUPPORTED
bool phase_ble5_adv() {
  BTStatus status = BLE.begin(serverName);
  if (!status) {
    return false;
  }

  BLEAdvertising adv = BLE.getAdvertising();

  BLEAdvertising::ExtAdvConfig extCfg;
  extCfg.instance = 0;
  extCfg.type = BLEAdvType::NonConnectable;
  extCfg.primaryPhy = BLEPhy::PHY_1M;
  extCfg.secondaryPhy = BLEPhy::PHY_2M;
  extCfg.sid = 1;
  status = adv.configureExtended(extCfg);
  if (!status) {
    BLE.end(false);
    return false;
  }

  Serial.println("[SERVER] BLE5 init OK");
  Serial.println("[SERVER] Extended adv configured");

  BLEAdvertisementData extData;
  extData.setName(serverName);
  adv.setExtAdvertisementData(0, extData);

  BLEAdvertising::PeriodicAdvConfig perCfg;
  perCfg.instance = 0;
  perCfg.intervalMin = 0x20;
  perCfg.intervalMax = 0x40;
  adv.configurePeriodicAdv(perCfg);

  BLEAdvertisementData perData;
  perData.setName("PeriodicPayload");
  adv.setPeriodicAdvData(0, perData);

  status = adv.startExtended(0, 0, 0);
  if (!status) {
    BLE.end(false);
    return false;
  }
  Serial.println("[SERVER] Extended adv started");

  adv.startPeriodicAdv(0);
  Serial.println("[SERVER] Periodic adv started");

  delay(20000);

  adv.stopPeriodicAdv(0);
  adv.stopExtended(0);
  BLE.end(false);
  Serial.println("[SERVER] BLE5 adv phase done");
  return true;
}
#endif

// ====================== Phase 3 — GATT Server Setup =========================

bool phase_gatt_setup() {
  size_t heapBefore = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  Serial.printf("[SERVER] Heap before init: %u\n", (unsigned)heapBefore);

  BTStatus status = BLE.begin(serverName);
  if (!status) {
    Serial.println("[SERVER] GATT init FAILED");
    return false;
  }
  Serial.println("[SERVER] GATT init OK");
  BLE.setMTU(512);

  size_t heapAfterInit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  Serial.printf("[SERVER] Heap after init: %u\n", (unsigned)heapAfterInit);

  BLESecurity sec = BLE.getSecurity();
  sec.setIOCapability(BLESecurity::DisplayYesNo);
  sec.setAuthenticationMode(true, true, true);
  // Distribute both the encryption key and the identity key so phase 22
  // can verify local/peer IRK round-trip across the bond.
  sec.setInitiatorKeys(BLESecurity::KeyDist::EncKey | BLESecurity::KeyDist::IdKey);
  sec.setResponderKeys(BLESecurity::KeyDist::EncKey | BLESecurity::KeyDist::IdKey);
  // Passkey can be delivered as display (BLE_SM_IOACT_DISP) on one side and
  // numeric comparison (BLE_SM_IOACT_NUMCMP) on the other; log both.
  sec.onPassKeyDisplay([](const BLEConnInfo &conn, uint32_t passkey) {
    Serial.printf("[SERVER] Passkey: %06lu\n", (unsigned long)passkey);
  });
  sec.onConfirmPassKey([](const BLEConnInfo &conn, uint32_t passkey) -> bool {
    Serial.printf("[SERVER] Passkey: %06lu\n", (unsigned long)passkey);
    return true;
  });
  sec.onAuthenticationComplete([](const BLEConnInfo &conn, bool success) {
    Serial.println("[SERVER] Authentication complete");
  });
  // Phase 18: application-level authorization callback. Always approves
  // reads; alternates approve/deny on writes (see authzApproveNextWrite).
  // Counters are drained from loop() to keep output ordered.
  sec.onAuthorization([](const BLEConnInfo &conn, uint16_t attrHandle, bool isRead) -> bool {
    authzLastAttrHandle = attrHandle;
    if (isRead) {
      uint32_t n = authzReadApprovals;
      authzReadApprovals = n + 1;
      return true;
    }
    bool approve = authzApproveNextWrite;
    authzApproveNextWrite = !authzApproveNextWrite;
    if (approve) {
      uint32_t n = authzWriteApprovals;
      authzWriteApprovals = n + 1;
    } else {
      uint32_t n = authzWriteDenials;
      authzWriteDenials = n + 1;
    }
    return approve;
  });
  Serial.println("[SERVER] Security configured");

  BLEServer server = BLE.createServer();
  server.onConnect([](BLEServer s, const BLEConnInfo &conn) {
    syncPhaseFromHost();
    Serial.println("[SERVER] Client connected");
  });
  server.onDisconnect([](BLEServer s, const BLEConnInfo &conn, uint8_t reason) {
    syncPhaseFromHost();
    Serial.println("[SERVER] Client disconnected");
  });
  // Phase 16 — capture MTU/connection-parameter events. Values are logged
  // from loop() after the phase coordination token is applied to guarantee
  // order on the test harness.
  server.onMtuChanged([](BLEServer s, const BLEConnInfo &conn, uint16_t mtu) {
    syncPhaseFromHost();
    phase16LastMtu = mtu;
    phase16MtuSeen = true;
  });
  server.onConnParamsUpdate([](BLEServer s, const BLEConnInfo &conn) {
    syncPhaseFromHost();
    phase16LastInterval = conn.getInterval();
    phase16LastLatency = conn.getLatency();
    phase16LastTimeout = conn.getSupervisionTimeout();
    phase16ConnParamsSeen = true;
  });
  server.advertiseOnDisconnect(true);

  BLEService svc = server.createService(BLEUUID(SERVICE_UUID));

  auto rwChr = svc.createCharacteristic(BLEUUID(RW_CHAR_UUID), BLEProperty::Read | BLEProperty::Write, BLEPermissions::OpenReadWrite);
  rwChr.setValue("Hello from server!");
  rwChr.onWrite([](BLECharacteristic c, const BLEConnInfo &conn) {
    syncPhaseFromHost();
    size_t len = 0;
    const uint8_t *data = c.getValue(&len);
    if (len <= 50) {
      Serial.printf("[SERVER] Received write: %.*s\n", (int)len, (const char *)data);
    }
    Serial.printf("[SERVER] Received %u bytes\n", (unsigned)len);
  });

  notifyChr = svc.createCharacteristic(BLEUUID(NOTIFY_CHAR_UUID), BLEProperty::Read | BLEProperty::Notify, BLEPermissions::OpenRead);
  notifyChr.onSubscribe([](BLECharacteristic chr, const BLEConnInfo &conn, uint16_t subValue) {
    syncPhaseFromHost();
    Serial.printf("[SERVER] Subscriber count: %u\n", chr.getSubscribedCount());
  });

  indicateChr = svc.createCharacteristic(BLEUUID(INDICATE_CHAR_UUID), BLEProperty::Read | BLEProperty::Indicate, BLEPermissions::OpenRead);

  auto secureChr = svc.createCharacteristic(BLEUUID(SECURE_CHAR_UUID), BLEProperty::Read, BLEPermissions::AuthenticatedRead);
  secureChr.setValue("Secure Data!");

  // Phase 7: Descriptor test characteristic with User Description + Presentation Format
  descChr = svc.createCharacteristic(BLEUUID(DESC_CHAR_UUID), BLEProperty::Read | BLEProperty::Write, BLEPermissions::OpenReadWrite);
  descChr.setValue("DescTest");
  descChr.setDescription("Test Characteristic");

  auto pfDesc = descChr.createDescriptor(BLEUUID(static_cast<uint16_t>(0x2904)), BLEPermission::Read, 7);
  uint8_t pfData[7] = {0};
  pfData[0] = BLEDescriptor::FORMAT_UTF8;  // format
  pfData[1] = 0;                           // exponent
  pfData[2] = 0x00;
  pfData[3] = 0x27;  // unit = 0x2700 (unitless)
  pfData[4] = 1;     // namespace (Bluetooth SIG)
  pfData[5] = 0;
  pfData[6] = 0;  // description
  pfDesc.setValue(pfData, 7);

  // Phase 15: custom-UUID descriptor with read-only permissions. The client
  // will read it successfully and then attempt a write, which must fail at
  // the ATT layer because the declared permissions do not include Write.
  // Using a custom (non-SIG) UUID avoids triggering the reserved-UUID
  // access-profile rules so this is purely a permission-enforcement test.
  auto roDesc = descChr.createDescriptor(BLEUUID(DESC_READONLY_UUID), BLEPermission::Read, 32);
  roDesc.setValue("ro_desc_val");

  // Phase 8: WriteNR test characteristic
  auto writeNrChr = svc.createCharacteristic(BLEUUID(WRITENR_CHAR_UUID), BLEProperty::Read | BLEProperty::WriteNR, BLEPermissions::OpenReadWrite);
  writeNrChr.setValue("waiting");
  writeNrChr.onWrite([](BLECharacteristic c, const BLEConnInfo &conn) {
    syncPhaseFromHost();
    size_t len = 0;
    const uint8_t *data = c.getValue(&len);
    Serial.printf("[SERVER] WriteNR received: %.*s\n", (int)len, (const char *)data);
  });

  // Phase 15: Fail-closed permission masking. Declares Read|Write but only
  // grants OpenRead, so the backend must drop the Write property from the
  // advertised characteristic and reject any write attempts.
  permFailClosedChr = svc.createCharacteristic(BLEUUID(PERM_FAIL_CLOSED_CHAR_UUID), BLEProperty::Read | BLEProperty::Write, BLEPermissions::OpenRead);
  permFailClosedChr.setValue("ro_data");
  permFailClosedChr.onWrite([](BLECharacteristic c, const BLEConnInfo &conn) {
    syncPhaseFromHost();
    Serial.println("[SERVER] FailClosed write callback fired — fail-closed broken!");
  });

  // Phase 18: AuthorizedReadWrite — every read/write traps through the
  // onAuthorization handler registered above. The handler approves reads
  // and approves writes only when the payload is "OK"; this lets the
  // client-side phase walk both approve and deny branches while the
  // counters accumulated from the handler are asserted against the exact
  // number of attempts. onWrite still fires on approved writes so we can
  // confirm the value reached the char.
  authzChr = svc.createCharacteristic(BLEUUID(AUTHZ_CHAR_UUID), BLEProperty::Read | BLEProperty::Write, BLEPermissions::AuthorizedReadWrite);
  authzChr.setValue("init");
  authzChr.onWrite([](BLECharacteristic c, const BLEConnInfo &conn) {
    syncPhaseFromHost();
    size_t len = 0;
    const uint8_t *data = c.getValue(&len);
    Serial.printf("[SERVER] Authz write applied: %.*s\n", (int)len, (const char *)data);
  });

  // Phase 19 & 22: EncryptedReadWrite. Access requires a paired link; used
  // to prove BLEConnInfo::isEncrypted/isBonded/isAuthenticated flip the
  // right way and to prove bond-delete forces a re-pair on next access.
  encryptedChr = svc.createCharacteristic(BLEUUID(ENCRYPTED_CHAR_UUID), BLEProperty::Read | BLEProperty::Write, BLEPermissions::EncryptedReadWrite);
  encryptedChr.setValue("enc_data");
  encryptedChr.onWrite([](BLECharacteristic c, const BLEConnInfo &conn) {
    syncPhaseFromHost();
    size_t len = 0;
    const uint8_t *data = c.getValue(&len);
    Serial.printf(
      "[SERVER] Encrypted write applied: %.*s enc=%d auth=%d bond=%d\n", (int)len, (const char *)data, (int)conn.isEncrypted(), (int)conn.isAuthenticated(),
      (int)conn.isBonded()
    );
  });

  server.start();

  size_t heapAfterServer = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  Serial.printf("[SERVER] Heap after server: %u\n", (unsigned)heapAfterServer);
  Serial.println("[SERVER] Server started");

  BLEAdvertising adv = BLE.getAdvertising();
  adv.addServiceUUID(BLEUUID(SERVICE_UUID));
  adv.start();
  Serial.println("[SERVER] Advertising started");
  return true;
}

// =============================================================================

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }
  readName();

  waitForPhase(1);
  if (!phase_basic()) {
    return;
  }

  waitForPhase(2);
  {
    bool ble5_ok = false;
#if BLE5_SUPPORTED
    ble5_ok = phase_ble5_adv();
#endif
    if (!ble5_ok) {
      Serial.println("[SERVER] BLE5 not supported, skipping");
    }
  }

  waitForPhase(3);
  if (!phase_gatt_setup()) {
    return;
  }
}

void loop() {
  checkSerial();

  // Phase 10: clear bond store so encrypted read forces a fresh pairing (passkey
  // lines). Runs once on the first loop after START_PHASE_10; disconnect first
  // so the stack can drop the old link before ble_store is cleared.
  if (currentPhase >= 10 && !phase10BondsCleared) {
    phase10BondsCleared = true;
    syncPhaseFromHost();
    {
      BLEServer srv = BLE.createServer();
      for (const auto &c : srv.getConnections()) {
        (void)srv.disconnect(c.getHandle());
      }
    }
    delay(200);
    BLESecurity se = BLE.getSecurity();
    if (se) {
      (void)se.deleteAllBonds();
    }
  }

  static unsigned long notifyTime = 0;

  // Phase 5: Send notification then indication
  if (currentPhase >= 5 && notifyStep == 0 && notifyChr.getSubscribedCount() > 0) {
    notifyChr.setValue("notify_test_1");
    BTStatus s = notifyChr.notify();
    if (s) {
      Serial.println("[SERVER] Notification sent");
    }
    notifyStep = 1;
    notifyTime = millis();
  }
  if (notifyStep == 1 && millis() - notifyTime >= 1000) {
    indicateChr.setValue("indicate_test_1");
    BTStatus s = indicateChr.indicate();
    if (s) {
      Serial.println("[SERVER] Indication sent");
    }
    notifyStep = 2;
  }

  // Phase 9: Server-initiated disconnect
  if (currentPhase >= 9 && !serverDisconnectDone) {
    serverDisconnectDone = true;
    BLEServer server = BLE.createServer();
    auto conns = server.getConnections();
    if (!conns.empty()) {
      BTStatus s = server.disconnect(conns[0].getHandle());
      if (s) {
        Serial.println("[SERVER] Server-initiated disconnect OK");
      } else {
        Serial.println("[SERVER] Server-initiated disconnect FAILED");
      }
    }
  }

  // Phase 13: BLEStream (server side)
  if (currentPhase >= 13 && !bleStreamInitDone) {
    bleStreamInitDone = true;
    bleStream.onConnect([](const BLEConnInfo& connInfo) {
      syncPhaseFromHost();
      bleStreamConnectCb = true;
    });
    bleStream.onDisconnect([](const BLEConnInfo& connInfo, uint8_t reason) {
      syncPhaseFromHost();
      bleStreamDisconnectCb = true;
    });
    BTStatus s = bleStream.begin(serverName);
    if (s) {
      Serial.println("[SERVER] BLEStream init OK");
    } else {
      Serial.printf("[SERVER] BLEStream init FAILED: %s\n", s.toString());
    }
  }

  if (currentPhase >= 13 && bleStreamInitDone && !bleStreamPhaseDone) {
    // Step 0: Wait for onConnect callback
    if (bleStreamStep == 0 && bleStreamConnectCb) {
      Serial.println("[SERVER] BLEStream onConnect fired");
      if (bleStream.connected()) {
        Serial.println("[SERVER] BLEStream connected OK");
      }
      bleStreamStep = 1;
    }

    // Step 1: Receive "stream_ping", reply "STREAM_PONG"
    if (bleStreamStep == 1 && bleStream.available()) {
      while (bleStream.available()) {
        int c = bleStream.read();
        if (c < 0) {
          break;
        }
        if (c == '\n') {
          bleStreamRxLine.trim();
          Serial.printf("[SERVER] BLEStream received: %s\n", bleStreamRxLine.c_str());
          bleStream.println("STREAM_PONG");
          Serial.println("[SERVER] BLEStream reply sent");
          bleStreamRxLine = "";
          bleStreamStep = 2;
          break;
        }
        bleStreamRxLine += (char)c;
      }
    }

    // Step 2: peek() test — peek first byte, then read full "peek_test" line
    if (bleStreamStep == 2 && bleStream.available()) {
      if (!bleStreamPeekDone) {
        int p = bleStream.peek();
        Serial.printf("[SERVER] BLEStream peek: %d\n", p);
        bleStreamPeekDone = true;
      }
      while (bleStream.available()) {
        int c = bleStream.read();
        if (c < 0) {
          break;
        }
        if (c == '\n') {
          bleStreamRxLine.trim();
          Serial.printf("[SERVER] BLEStream peek read: %s\n", bleStreamRxLine.c_str());
          bleStreamRxLine = "";
          bleStreamStep = 3;
          break;
        }
        bleStreamRxLine += (char)c;
      }
    }

    // Step 3: Receive bulk data (200-char pattern), verify integrity
    if (bleStreamStep == 3 && bleStream.available()) {
      while (bleStream.available()) {
        int c = bleStream.read();
        if (c < 0) {
          break;
        }
        if (c == '\n') {
          bleStreamRxLine.trim();
          size_t len = bleStreamRxLine.length();
          Serial.printf("[SERVER] BLEStream bulk received: %u bytes\n", (unsigned)len);
          bool ok = (len == 200);
          for (size_t i = 0; i < len && ok; i++) {
            if (bleStreamRxLine[i] != (char)('A' + (i % 26))) {
              ok = false;
            }
          }
          Serial.println(ok ? "[SERVER] BLEStream bulk integrity OK" : "[SERVER] BLEStream bulk integrity FAILED");
          bleStreamRxLine = "";
          bleStreamStep = 4;
          break;
        }
        bleStreamRxLine += (char)c;
      }
    }

    // Step 4: Server-initiated write to client
    if (bleStreamStep == 4) {
      bleStream.println("server_says_hi");
      Serial.println("[SERVER] BLEStream server msg sent");
      bleStreamStep = 5;
    }

    // Step 5: Wait for onDisconnect callback
    if (bleStreamStep == 5 && bleStreamDisconnectCb) {
      Serial.println("[SERVER] BLEStream onDisconnect fired");
      bleStreamPhaseDone = true;
      bleStream.end();
    }
  }

  // Phase 14: L2CAP CoC (server side)
  if (currentPhase >= 14 && !l2capInitDone) {
#if BLE_L2CAP_SUPPORTED
    l2capInitDone = true;
    l2capServer = BLE.createL2CAPServer(0x0080, 128);
    if (!l2capServer) {
      Serial.println("[SERVER] L2CAP init FAILED");
    } else {
      l2capServer.onAccept([](BLEL2CAPChannel channel) {
        syncPhaseFromHost();
        l2capChannel = channel;
        Serial.println("[SERVER] L2CAP channel accepted");
      });
      l2capServer.onData([](BLEL2CAPChannel channel, const uint8_t *data, size_t len) {
        syncPhaseFromHost();
        String msg((const char *)data, len);
        Serial.printf("[SERVER] L2CAP received: %s\n", msg.c_str());
        const char *reply = "L2CAP_OK";
        BTStatus s = channel.write((const uint8_t *)reply, strlen(reply));
        if (s) {
          Serial.println("[SERVER] L2CAP reply sent");
        } else {
          Serial.printf("[SERVER] L2CAP reply FAILED: %s\n", s.toString());
        }
      });
      Serial.println("[SERVER] L2CAP init OK");
    }
#else
    l2capInitDone = true;
    Serial.println("[SERVER] L2CAP not supported, skipping");
#endif
  }

  // Phase 15: Construction-time descriptor permission validation.
  //
  // Runs bleValidateDescProps through BLECharacteristic::createDescriptor for
  // a handful of malformed specs and verifies each one is rejected (empty
  // BLEDescriptor handle → `!desc` is true). These calls happen after
  // server.start() has already registered the live GATT table, so any
  // descriptor that *would* have been accepted here won't be advertised to
  // the peer — that's intentional: this block tests the validator itself,
  // independent of the live GATT state. The runtime enforcement counterpart
  // (reading/writing the read-only custom descriptor on DESC_CHAR) is
  // driven by the client.
  if (currentPhase >= 15 && !descPermValidationDone) {
    descPermValidationDone = true;
    Serial.println("[SERVER] DescPerm validation start");

    // 1. BLEPermission::None — fail-closed: a descriptor with no access is
    //    inaccessible by construction and must be rejected.
    auto d1 = permFailClosedChr.createDescriptor(BLEUUID("a0000000-0000-4000-8000-000000000001"), BLEPermission::None);
    Serial.printf("[SERVER] DescPerm None rejected: %d\n", (int)(!d1));

    // 2. Presentation Format (0x2904) declared writable — the spec says
    //    this descriptor must be read-only.
    auto d2 = permFailClosedChr.createDescriptor(BLEUUID(static_cast<uint16_t>(0x2904)), BLEPermission::Read | BLEPermission::Write, 7);
    Serial.printf("[SERVER] DescPerm 0x2904+Write rejected: %d\n", (int)(!d2));

    // 3. Extended Properties (0x2900) with Write — must be read-only.
    auto d3 = permFailClosedChr.createDescriptor(BLEUUID(static_cast<uint16_t>(0x2900)), BLEPermission::Write, 2);
    Serial.printf("[SERVER] DescPerm 0x2900+Write rejected: %d\n", (int)(!d3));

    // 4. CCCD (0x2902) with read-only — must have both R+W so clients can
    //    subscribe.
    auto d4 = permFailClosedChr.createDescriptor(BLEUUID(static_cast<uint16_t>(0x2902)), BLEPermission::Read, 2);
    Serial.printf("[SERVER] DescPerm 0x2902 read-only rejected: %d\n", (int)(!d4));

    // 5. User Description (0x2901) with no read permission — must be
    //    readable (even when writable is gated by 0x2900).
    auto d5 = permFailClosedChr.createDescriptor(BLEUUID(static_cast<uint16_t>(0x2901)), BLEPermission::Write, 16);
    Serial.printf("[SERVER] DescPerm 0x2901 no-read rejected: %d\n", (int)(!d5));

    // 6. WriteAuthorized without any write direction — authorization must
    //    attach to a base direction.
    auto d6 = permFailClosedChr.createDescriptor(BLEUUID("a0000000-0000-4000-8000-000000000002"), BLEPermission::Read | BLEPermission::WriteAuthorized);
    Serial.printf("[SERVER] DescPerm WriteAuthorized-no-write rejected: %d\n", (int)(!d6));

    // 7. Control: a valid descriptor spec must NOT be rejected. Uses a
    //    custom UUID so no reserved-UUID rule fires.
    auto d7 = permFailClosedChr.createDescriptor(BLEUUID("a0000000-0000-4000-8000-000000000003"), BLEPermission::Read);
    Serial.printf("[SERVER] DescPerm valid accepted: %d\n", (int)((bool)d7));

    Serial.println("[SERVER] DescPerm validation done");
  }

  // =========================================================================
  // Phase 16: Connection info + MTU / conn-params callbacks
  // =========================================================================
  // The client reconnects for this phase and issues an updateConnParams
  // request. By the time we observe phase16 on the host, both the MTU and
  // conn-params callbacks should have fired at least once (MTU during
  // reconnect, conn-params after the client's request round-trips through
  // the controller). We log them here rather than from the callbacks to
  // guarantee ordering after the phase start marker.
  if (currentPhase >= 16 && !phase16Done) {
    phase16Done = true;
    Serial.println("[SERVER] Phase16 waiting for peer");
    unsigned long startWait = millis();
    while ((millis() - startWait) < 10000) {
      BLEServer srv = BLE.createServer();
      if (srv.getConnectedCount() > 0) {
        break;
      }
      delay(50);
    }
    BLEServer srv = BLE.createServer();
    auto conns = srv.getConnections();
    if (conns.empty()) {
      Serial.println("[SERVER] Phase16 no peer connected");
    } else {
      const BLEConnInfo &c = conns[0];
      uint16_t peerMtu = srv.getPeerMTU(c.getHandle());
      Serial.printf("[SERVER] ConnInfo handle=%u mtu=%u\n", (unsigned)c.getHandle(), (unsigned)c.getMTU());
      Serial.printf("[SERVER] ConnInfo peerMtu=%u\n", (unsigned)peerMtu);
      Serial.printf(
        "[SERVER] ConnInfo sec enc=%d auth=%d bond=%d keySize=%u\n", (int)c.isEncrypted(), (int)c.isAuthenticated(), (int)c.isBonded(),
        (unsigned)c.getSecurityKeySize()
      );
      Serial.printf("[SERVER] ConnInfo role central=%d peripheral=%d\n", (int)c.isCentral(), (int)c.isPeripheral());
      Serial.printf(
        "[SERVER] ConnInfo params interval=%u latency=%u timeout=%u\n", (unsigned)c.getInterval(), (unsigned)c.getLatency(), (unsigned)c.getSupervisionTimeout()
      );
      BTAddress ota = c.getAddress();
      BTAddress id = c.getIdAddress();
      Serial.printf("[SERVER] ConnInfo addr ota=%s id=%s\n", ota.toString().c_str(), id.toString().c_str());
      // Wait up to 10 s for the client's updateConnParams to round-trip. The
      // controller may take several intervals before the new values propagate.
      unsigned long paramWait = millis();
      while (!phase16ConnParamsSeen && (millis() - paramWait) < 10000) {
        delay(50);
      }
      Serial.printf("[SERVER] MtuChanged seen=%d last=%u\n", (int)phase16MtuSeen, (unsigned)phase16LastMtu);
      Serial.printf(
        "[SERVER] ConnParamsUpdate seen=%d interval=%u latency=%u timeout=%u\n", (int)phase16ConnParamsSeen, (unsigned)phase16LastInterval,
        (unsigned)phase16LastLatency, (unsigned)phase16LastTimeout
      );
    }
    Serial.println("[SERVER] Phase16 done");
  }

  // =========================================================================
  // Phase 17: Address types — API surface + connection roundtrip per type
  // =========================================================================
  // Two halves:
  //   (a) API-surface coverage — call BLE.setOwnAddressType with every
  //       BTAddress::Type enum value and log the BTStatus.
  //   (b) Connection roundtrip — for Public + Random, stop advertising,
  //       switch the own-address type, restart adv with a per-type nonce
  //       in the scan response, and wait for the client to scan + connect
  //       + disconnect. This proves the full stack propagates the type
  //       through adv PDUs, not just the API.
  //
  // Types that the controller rejects (typically PublicID/RandomID when
  // LL privacy is off) are still exercised in the API-surface pass so the
  // return value is observable; the connection roundtrip covers the two
  // types that are guaranteed to work on every ESP32 variant.
  if (currentPhase >= 17 && !phase17Done) {
    phase17Done = true;
    BTAddress self = BLE.getAddress();
    Serial.printf("[SERVER] OwnAddress addr=%s type=%u\n", self.toString().c_str(), (unsigned)self.type());

    // (a) API-surface coverage over every enum value.
    BTStatus s1 = BLE.setOwnAddressType(BTAddress::Public);
    Serial.printf("[SERVER] setOwnAddressType(Public) ok=%d\n", (int)(bool)s1);
    BTStatus s2 = BLE.setOwnAddressType(BTAddress::Random);
    Serial.printf("[SERVER] setOwnAddressType(Random) ok=%d\n", (int)(bool)s2);
    BTStatus s3 = BLE.setOwnAddressType(BTAddress::PublicID);
    Serial.printf("[SERVER] setOwnAddressType(PublicID) ok=%d\n", (int)(bool)s3);
    BTStatus s4 = BLE.setOwnAddressType(BTAddress::RandomID);
    Serial.printf("[SERVER] setOwnAddressType(RandomID) ok=%d\n", (int)(bool)s4);

    // Drop any existing connection before swapping the identity type.
    {
      BLEServer sv = BLE.createServer();
      auto cs = sv.getConnections();
      for (auto &c : cs) {
        (void)sv.disconnect(c.getHandle());
      }
    }
    delay(500);

    // (b) Connection roundtrip per type.
    struct TypeSpec {
      const char *name;
      BTAddress::Type type;
    };
    const TypeSpec types[] = {
      {"Public", BTAddress::Public},
      {"Random", BTAddress::Random},
    };
    BLEAdvertising adv = BLE.getAdvertising();
    for (const auto &ts : types) {
      adv.stop();
      delay(150);
      BTStatus st = BLE.setOwnAddressType(ts.type);
      Serial.printf("[SERVER] Phase17 set type=%s ok=%d\n", ts.name, (int)(bool)st);

      // Per-type name in the primary ADV payload lets the client
      // unambiguously pair a scan hit with the current type.
      adv.reset();
      adv.setName(String("T17_") + ts.name + "_" + serverName);
      BTStatus as = adv.start();
      Serial.printf("[SERVER] Phase17 adv type=%s ok=%d\n", ts.name, (int)(bool)as);

      // Wait up to 12s for the client to connect+disconnect using this type.
      unsigned long startWait = millis();
      bool observedConnect = false;
      bool observedDisconnect = false;
      uint16_t lastHandle = 0;
      while ((millis() - startWait) < 12000) {
        BLEServer sv = BLE.createServer();
        auto cs = sv.getConnections();
        if (!cs.empty()) {
          if (!observedConnect) {
            observedConnect = true;
            lastHandle = cs[0].getHandle();
          }
        } else if (observedConnect) {
          observedDisconnect = true;
          break;
        }
        delay(50);
      }
      Serial.printf(
        "[SERVER] Phase17 type=%s connObserved=%d discObserved=%d handle=%u\n", ts.name, (int)observedConnect, (int)observedDisconnect, (unsigned)lastHandle
      );
    }

    // Restore a stable, connectable default for subsequent phases. The
    // controller-selected default (`self.type()`) is preferred, falling
    // back to Public if that type is somehow rejected here.
    adv.stop();
    delay(150);
    BTStatus rst = BLE.setOwnAddressType(self.type());
    if (!rst) {
      (void)BLE.setOwnAddressType(BTAddress::Public);
    }
    adv.reset();
    adv.setType(BLEAdvType::Connectable);
    adv.addServiceUUID(BLEUUID(SERVICE_UUID));
    (void)adv.start();

    Serial.println("[SERVER] Phase17 done");
  }

  // =========================================================================
  // Phase 18: Authorization handler — runtime access gating
  // =========================================================================
  // The client reads the AUTHZ char (approved), then writes "OK" (approved),
  // then writes "NO" (denied by handler — controller returns an ATT error
  // and onWrite must NOT fire for the denied attempt). We log the counters
  // that the handler incremented from the stack task. Because reads and
  // writes are driven by the client, this phase just samples the counters
  // once the client signals it has finished exercising them.
  if (currentPhase >= 18 && !phase18Started) {
    phase18Started = true;
    Serial.println("[SERVER] Phase18 authz ready");
    // Observer loop: let the client drive for up to 15 s, then report.
    unsigned long startObs = millis();
    uint32_t lastReads = 0, lastApprovals = 0, lastDenials = 0;
    while ((millis() - startObs) < 15000) {
      if (authzReadApprovals != lastReads || authzWriteApprovals != lastApprovals || authzWriteDenials != lastDenials) {
        lastReads = authzReadApprovals;
        lastApprovals = authzWriteApprovals;
        lastDenials = authzWriteDenials;
        // Wait a little longer for the client to finish any remaining ops.
        delay(500);
      }
      // Exit early once we've seen at least one of each.
      if (authzReadApprovals >= 1 && authzWriteApprovals >= 1 && authzWriteDenials >= 1) {
        delay(500);
        break;
      }
      delay(50);
    }
    Serial.printf(
      "[SERVER] Authz counters reads=%lu writesOK=%lu writesDeny=%lu\n", (unsigned long)authzReadApprovals, (unsigned long)authzWriteApprovals,
      (unsigned long)authzWriteDenials
    );
    Serial.printf("[SERVER] Authz lastAttrHandle=%u\n", (unsigned)authzLastAttrHandle);
    Serial.println("[SERVER] Phase18 done");
  }

  // =========================================================================
  // Phase 19: Encrypted-permission enforcement
  // =========================================================================
  // Link has been paired since phase 10; the client reads + writes the
  // encrypted char and we log the current connection security view from
  // the server side. This is the positive-control pairing exists, the
  // encrypted ops succeed.
  if (currentPhase >= 19 && !phase19Done) {
    phase19Done = true;
    // The client reconnects asynchronously for this phase. Poll for a peer
    // before sampling so we don't race the connection handshake; otherwise
    // getConnections() can be empty even though the client is about to
    // complete the setup. A 10s budget is generous enough for pairing
    // restoration over a flaky radio environment.
    unsigned long startWait = millis();
    bool havePeer = false;
    BLEConnInfo sample;
    while ((millis() - startWait) < 10000) {
      BLEServer srv = BLE.createServer();
      auto conns = srv.getConnections();
      if (!conns.empty()) {
        havePeer = true;
        sample = conns[0];
        break;
      }
      delay(50);
    }
    if (!havePeer) {
      Serial.println("[SERVER] Phase19 no peer connected");
    } else {
      // Give the client time to exercise the encrypted ops before the
      // link tears down, so the sec flags we report actually reflect the
      // post-decryption view.
      delay(2000);
      Serial.printf("[SERVER] Phase19 encSec enc=%d auth=%d bond=%d\n", (int)sample.isEncrypted(), (int)sample.isAuthenticated(), (int)sample.isBonded());
    }
    Serial.println("[SERVER] Phase19 done");
  }

  // =========================================================================
  // Phase 20: Advertisement data builders + scan tuning (server side)
  // =========================================================================
  // Requires tearing down the current connection: building a new legacy adv
  // payload with every BLEAdvertisementData setter, pushing it via
  // setAdvertisementData(), and letting the client scan+parse it. The server
  // stays initialized but the connection is dropped and a new advertising
  // configuration is applied.
  if (currentPhase >= 20 && !phase20Done) {
    phase20Done = true;
    BLEServer srv = BLE.createServer();
    auto conns = srv.getConnections();
    for (auto &c : conns) {
      (void)srv.disconnect(c.getHandle());
    }
    delay(500);

    BLEAdvertising adv = BLE.getAdvertising();
    adv.stop();
    adv.reset();
    adv.setType(BLEAdvType::Connectable);
    adv.setInterval(100, 200);
    adv.setMinPreferred(0x06);
    adv.setMaxPreferred(0x12);
    adv.setAppearance(0x0340);  // Generic Sensor
    adv.setScanResponse(true);

    // Split across primary (31 B max) and scan response (31 B max).
    //   primary  -> flags (3) + name "ADV_xxxx" (~12) + appearance (4) + txPower (3)
    //   scan rsp -> manufacturer data (7) + svc data (6) + 16-bit svc UUID (4)
    String advName = String("ADV_") + serverName;
    BLEAdvertisementData d;
    d.setFlags(0x06);
    d.setName(advName);
    d.setAppearance(0x0340);
    d.setTxPower(-4);
    adv.setAdvertisementData(d);

    BLEAdvertisementData sr;
    uint8_t mfg[] = {'E', 'S', 'P'};
    sr.setManufacturerData(0x02E5, mfg, sizeof(mfg));
    uint8_t svcData[] = {0xAB, 0xCD};
    sr.setServiceData(BLEUUID((uint16_t)0x180D), svcData, sizeof(svcData));
    sr.setCompleteServices(BLEUUID((uint16_t)0x180D));
    adv.setScanResponseData(sr);

    BTStatus s = adv.start();
    Serial.printf("[SERVER] Phase20 advReady ok=%d isAdv=%d\n", (int)(bool)s, (int)adv.isAdvertising());
    // Must outlast the client's 8 s scan (which starts ~1 s after the
    // phase begins due to disconnect + adv setup time).
    delay(12000);
    adv.stop();
    adv.reset();
    adv.addServiceUUID(BLEUUID(SERVICE_UUID));
    adv.start();
    Serial.println("[SERVER] Phase20 done");
  }

  // =========================================================================
  // Phase 21: iBeacon + Eddystone (URL + TLM) frame build
  // =========================================================================
  // Three non-connectable advertising payloads, one after another, so the
  // client can classify each via BLEAdvertisedDevice::getFrameType() and
  // parse it back to the original fields.
  if (currentPhase >= 21 && !phase21Done) {
    phase21Done = true;
    BLEServer srv = BLE.createServer();
    auto conns = srv.getConnections();
    for (auto &c : conns) {
      (void)srv.disconnect(c.getHandle());
    }
    delay(500);
    BLEAdvertising adv = BLE.getAdvertising();
    adv.stop();
    adv.reset();
    adv.setType(BLEAdvType::NonConnectable);

    // iBeacon frame.
    BLEBeacon beacon;
    beacon.setProximityUUID(BLEUUID("a1b2c3d4-e5f6-1122-3344-556677889900"));
    beacon.setMajor(0x1234);
    beacon.setMinor(0x5678);
    beacon.setSignalPower(-59);
    adv.setAdvertisementData(beacon.getAdvertisementData());
    BTStatus s = adv.start();
    Serial.printf("[SERVER] Phase21 iBeacon adv ok=%d\n", (int)(bool)s);
    delay(4000);
    adv.stop();

    // Eddystone URL.
    BLEEddystoneURL url;
    url.setURL("https://espressif.com");
    url.setTxPower(-20);
    adv.setAdvertisementData(url.getAdvertisementData());
    s = adv.start();
    Serial.printf("[SERVER] Phase21 EddystoneURL ok=%d\n", (int)(bool)s);
    delay(4000);
    adv.stop();

    // Eddystone TLM.
    BLEEddystoneTLM tlm;
    tlm.setBatteryVoltage(3000);
    tlm.setTemperature(25.5f);
    tlm.setAdvertisingCount(42);
    tlm.setUptime(1234);
    adv.setAdvertisementData(tlm.getAdvertisementData());
    s = adv.start();
    Serial.printf("[SERVER] Phase21 EddystoneTLM ok=%d\n", (int)(bool)s);
    delay(4000);
    adv.stop();

    adv.reset();
    adv.setType(BLEAdvType::Connectable);
    adv.addServiceUUID(BLEUUID(SERVICE_UUID));
    adv.start();
    Serial.println("[SERVER] Phase21 done");
  }

  // =========================================================================
  // Phase 22: Bond management + whitelist
  // =========================================================================
  // Enumerate bonds (phase 10 established at least one), log IRK, round-trip
  // the whitelist API. deleteAllBonds is left for the client-driven
  // re-pair test on encryptedChr — after delete the client must re-pair
  // on the next encrypted read. The server lists bonds before and after
  // deletion so both branches are observable.
  if (currentPhase >= 22 && !phase22Done) {
    phase22Done = true;
    BLESecurity s = BLE.getSecurity();
    auto bonds = s.getBondedDevices();
    Serial.printf("[SERVER] Bond count: %u\n", (unsigned)bonds.size());
    for (size_t i = 0; i < bonds.size() && i < 4; i++) {
      Serial.printf("[SERVER] Bond[%u]=%s type=%u\n", (unsigned)i, bonds[i].toString().c_str(), (unsigned)bonds[i].type());
    }

    // Whitelist round-trip uses the first bonded peer if any, otherwise a
    // fixed fake address. The goal is to exercise add/query/remove paths.
    BTAddress wlAddr = bonds.empty() ? BTAddress("AA:BB:CC:DD:EE:FF") : bonds[0];
    BTStatus a = BLE.whiteListAdd(wlAddr);
    bool on = BLE.isOnWhiteList(wlAddr);
    BTStatus r = BLE.whiteListRemove(wlAddr);
    bool after = BLE.isOnWhiteList(wlAddr);
    Serial.printf("[SERVER] Whitelist add=%d isOn=%d remove=%d after=%d\n", (int)(bool)a, (int)on, (int)(bool)r, (int)after);

    // Local IRK + peer IRK: the client exports its local IRK on the line
    // "[CLIENT] LocalIRK hex=<32 hex>" and its peer IRK (= our local IRK)
    // on "[CLIENT] PeerIRK hex=<32 hex>". We emit the mirror lines here so
    // the Python harness can assert local(server)==peer(client) and
    // peer(server)==local(client) — proving identity keys round-tripped
    // correctly across the SMP pairing.
    String localIrkHex = BLE.getLocalIRKString();
    Serial.flush();
    delay(50);
    Serial.printf("[SERVER] LocalIRK present=%d hex=%s\n", (int)(localIrkHex.length() > 0), localIrkHex.c_str());
    Serial.flush();
    delay(50);
    if (!bonds.empty()) {
      String peerIrkHex = BLE.getPeerIRKString(bonds[0]);
      Serial.printf("[SERVER] PeerIRK present=%d hex=%s\n", (int)(peerIrkHex.length() > 0), peerIrkHex.c_str());
      Serial.flush();
      delay(50);
    } else {
      Serial.println("[SERVER] PeerIRK present=0 hex=");
      Serial.flush();
      delay(50);
    }

    Serial.println("[SERVER] Phase22 done");
  }

  // =========================================================================
  // Phase 23: Error paths + miscellaneous API coverage
  // =========================================================================
  // - BLEServer::getServices()/getService(uuid)/getService(unknown)
  // - BLEService::getCharacteristics()/getCharacteristic(uuid)/removeCharacteristic
  // - BLECharacteristic::notify(connHandle), isSubscribed(conn)
  // - BLEUUID ==, bitSize, toUint16, to128 round-trip
  // Then trigger a notify targeted at the specific connection handle.
  if (currentPhase >= 23 && !phase23Done) {
    phase23Done = true;
    // Ensure we're advertising so the client can reconnect for this phase.
    BLEAdvertising adv23 = BLE.getAdvertising();
    if (!adv23.isAdvertising()) {
      adv23.start();
    }
    BLEServer srv = BLE.createServer();

    // Server-local API surface tests (no connection needed).
    auto svcs = srv.getServices();
    Serial.printf("[SERVER] Services: %u\n", (unsigned)svcs.size());
    BLEService primarySvc = srv.getService(BLEUUID(SERVICE_UUID));
    Serial.printf("[SERVER] GetService known ok=%d started=%d\n", (int)(bool)primarySvc, (int)primarySvc.isStarted());
    BLEService unknownSvc = srv.getService(BLEUUID(UNKNOWN_SVC_UUID));
    Serial.printf("[SERVER] GetService unknown ok=%d\n", (int)(bool)unknownSvc);

    if (primarySvc) {
      auto chars = primarySvc.getCharacteristics();
      Serial.printf("[SERVER] Characteristics: %u\n", (unsigned)chars.size());
      auto known = primarySvc.getCharacteristic(BLEUUID(RW_CHAR_UUID));
      Serial.printf("[SERVER] GetCharacteristic known ok=%d\n", (int)(bool)known);
      auto unknown = primarySvc.getCharacteristic(BLEUUID(UNKNOWN_CHAR_UUID));
      Serial.printf("[SERVER] GetCharacteristic unknown ok=%d\n", (int)(bool)unknown);
      Serial.printf("[SERVER] Characteristic handle=%u\n", (unsigned)known.getHandle());
    }

    BLEUUID u16(static_cast<uint16_t>(0x180D));
    BLEUUID u128 = u16.to128();
    bool eq = (u16 == u128);
    Serial.printf(
      "[SERVER] UUID bitSize16=%u bitSize128=%u to16=0x%04X eq=%d\n", (unsigned)u16.bitSize(), (unsigned)u128.bitSize(), (unsigned)u16.toUint16(), (int)eq
    );
    BLEUUID u32(static_cast<uint32_t>(0x0000180DUL));
    Serial.printf("[SERVER] UUID bitSize32=%u to32=0x%08lX\n", (unsigned)u32.bitSize(), (unsigned long)u32.toUint32());

    // Wait for the client to connect (it rescans before connecting).
    {
      unsigned long deadline = millis() + 20000;
      bool sawConnect = false;
      while (millis() < deadline) {
        auto conns = srv.getConnections();
        if (!conns.empty()) {
          sawConnect = true;
          uint16_t h = conns[0].getHandle();
          notifyChr.setValue("targeted");
          BTStatus s = notifyChr.notify(h);
          Serial.printf("[SERVER] NotifyHandle ok=%d handle=%u\n", (int)(bool)s, (unsigned)h);
          Serial.printf("[SERVER] IsSubscribed handle=%u sub=%d\n", (unsigned)h, (int)notifyChr.isSubscribed(h));
          auto subs = notifyChr.getSubscribedConnections();
          Serial.printf("[SERVER] SubscribedConnections: %u\n", (unsigned)subs.size());
          break;
        }
        delay(100);
      }
      if (!sawConnect) {
        Serial.println("[SERVER] Phase23 no peer for notify(handle)");
      }
    }
    Serial.println("[SERVER] Phase23 done");
  }

  // =========================================================================
  // Phase 24: BLE5 advanced (coded PHY, server setPhy/setDataLen, defaultPhy)
  // =========================================================================
  // Self-skipping when BLE5 is unavailable. Only BLE.getDefaultPhy/
  // setDefaultPhy and per-connection setPhy/setDataLen are exercised from
  // the server side; the client drives coded-PHY extended adv/scan in its
  // own phase 24 block.
  if (currentPhase >= 24 && !phase24Done) {
    phase24Done = true;
#if BLE5_SUPPORTED
    BLEPhy tx = BLEPhy::PHY_1M;
    BLEPhy rx = BLEPhy::PHY_1M;
    BTStatus gd = BLE.getDefaultPhy(tx, rx);
    Serial.printf("[SERVER] Phase24 getDefaultPhy ok=%d tx=%u rx=%u\n", (int)(bool)gd, (unsigned)tx, (unsigned)rx);
    BTStatus sd = BLE.setDefaultPhy(BLEPhy::PHY_1M, BLEPhy::PHY_1M);
    Serial.printf("[SERVER] Phase24 setDefaultPhy ok=%d\n", (int)(bool)sd);

    BLEServer srv = BLE.createServer();
    auto conns = srv.getConnections();
    if (!conns.empty()) {
      uint16_t h = conns[0].getHandle();
      BTStatus sp = srv.setPhy(h, BLEPhy::PHY_2M, BLEPhy::PHY_2M);
      Serial.printf("[SERVER] Phase24 setPhy(2M) ok=%d\n", (int)(bool)sp);
      BLEPhy txp = BLEPhy::PHY_1M, rxp = BLEPhy::PHY_1M;
      BTStatus gp = srv.getPhy(h, txp, rxp);
      Serial.printf("[SERVER] Phase24 getPhy ok=%d tx=%u rx=%u\n", (int)(bool)gp, (unsigned)txp, (unsigned)rxp);
      BTStatus dl = srv.setDataLen(h, 251, 2120);
      Serial.printf("[SERVER] Phase24 setDataLen ok=%d\n", (int)(bool)dl);
    } else {
      Serial.println("[SERVER] Phase24 no peer for setPhy");
    }
#else
    Serial.println("[SERVER] Phase24 BLE5 not supported, skipping");
#endif
    Serial.println("[SERVER] Phase24 done");
  }

  // =========================================================================
  // Phase 25: HID-over-GATT spec compliance
  // =========================================================================
  // Tears down the existing server/service state, rebuilds a fresh
  // BLEHIDDevice and exercises every HoGP/HIDS spec addition:
  //   - Included Service (Battery in HID)
  //   - External Report Reference descriptor on Report Map
  //   - Multiple Report characteristics (same UUID 0x2A4D)
  //   - Boot Keyboard Input/Output with correct properties + caching
  //   - Auto-advertised HID Service UUID
  if (currentPhase >= 25 && !phase25Done) {
    phase25Done = true;

    {
      BLEServer srv = BLE.createServer();
      auto conns = srv.getConnections();
      for (auto &c : conns) {
        (void)srv.disconnect(c.getHandle());
      }
    }
    delay(500);
    BLE.end(false);
    delay(500);

    BTStatus s = BLE.begin(String("HID_") + serverName);
    if (!s) {
      Serial.printf("[SERVER] Phase25 HID init FAILED: %s\n", s.toString());
    } else {
      BLEServer hidServer = BLE.createServer();
      BLEHIDDevice hid(hidServer);
      hid.manufacturer("Espressif");
      hid.pnp(0x02, 0x303A, 0x1001, 0x0100);
      hid.hidInfo(0x00, 0x01);
      static const uint8_t kbdReportMap[] = {
        0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x85, 0x01,
        0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00,
        0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
        0x75, 0x08, 0x95, 0x01, 0x81, 0x03, 0xC0
      };
      hid.reportMap(kbdReportMap, sizeof(kbdReportMap));

      BLECharacteristic inRpt = hid.inputReport(1);
      BLECharacteristic outRpt = hid.outputReport(1);

      BLECharacteristic bootIn1 = hid.bootInput();
      BLECharacteristic bootIn2 = hid.bootInput();

      BLECharacteristic bootOut = hid.bootOutput();

      hid.setBatteryLevel(84);

      auto hidChars = hid.hidService().getCharacteristics();
      Serial.printf("[SERVER] Phase25 hidChars=%u\n", (unsigned)hidChars.size());

      hidServer.start();

      // Handle comparisons must happen after start() — handles are 0 before registration.
      bool multiReport = (inRpt && outRpt && inRpt.getHandle() != outRpt.getHandle());
      Serial.printf("[SERVER] Phase25 multiReport=%d\n", (int)multiReport);

      bool bootCached = (bootIn1 && bootIn2 && bootIn1.getHandle() == bootIn2.getHandle());
      Serial.printf("[SERVER] Phase25 bootCached=%d\n", (int)bootCached);

      Serial.printf("[SERVER] Phase25 bootOut=%d\n", (int)(bool)bootOut);

      BLEAdvertising adv = BLE.getAdvertising();
      // BLE.end() above already reset the advertising state (cleared all
      // accumulated serviceUUIDs, appearance, etc.), so no adv.reset() is
      // needed here.  BLEHIDDevice's constructor automatically added the HID
      // Service UUID (0x1812) per HoGP §4.3.1; it is already present.
      adv.setType(BLEAdvType::Connectable);
      adv.setAppearance(BLE_APPEARANCE_HID_KEYBOARD);
      BTStatus as = adv.start();
      Serial.printf("[SERVER] Phase25 HID adv ok=%d\n", (int)(bool)as);
      Serial.println("[SERVER] Phase25 HID ready");
      delay(10000);
      adv.stop();
    }
    Serial.println("[SERVER] Phase25 done");
  }

  // Phase 26: Memory release (tail of the suite; runs after every other phase
  // phases 16-25 can exercise the BLE stack while it's still alive).
  if (currentPhase >= 26 && !phase26Done) {
    phase26Done = true;
    delay(500);

    size_t heapBeforeRelease = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    Serial.printf("[SERVER] Heap before release: %u\n", (unsigned)heapBeforeRelease);

    BLE.end(true);

    delay(500);
    size_t heapAfterRelease = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    Serial.printf("[SERVER] Heap after release: %u\n", (unsigned)heapAfterRelease);

    int32_t freed = (int32_t)heapAfterRelease - (int32_t)heapBeforeRelease;
    Serial.printf("[SERVER] Memory freed: %ld bytes\n", (long)freed);
    if (freed >= 10240) {
      Serial.println("[SERVER] Memory release OK");
    } else {
      Serial.println("[SERVER] Memory release INSUFFICIENT");
    }

    BTStatus s = BLE.begin("ShouldFail");
    if (!s) {
      Serial.println("[SERVER] Reinit blocked OK");
    } else {
      Serial.println("[SERVER] Reinit was NOT blocked");
    }

    Serial.println("[SERVER] All phases complete");
  }

  delay(50);
}
