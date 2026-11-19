// BT Classic validation test — CLIENT (SPP initiator)
// Phases:
//   1. basic_lifecycle     — begin / deinit / reinit
//   2. spp_connect_data    — discover server by name, connect, bidirectional data, disconnect
//   3. bond_management     — list bonds, delete all, verify empty
//   4. discovery           — sync discover(), discoverAsync() and discoverStop()
//   5. ssp_passkey_entry   — KeyboardOnly: receive the passkey from the test
//                            script and complete Passkey Entry with the server
//   6. ssp_numeric_compare — DisplayYesNo: confirm the numeric value
//   7. ssp_passkey_reject  — reject the passkey and verify the pairing fails
//   8. memory_release      — end(true)

#include <Arduino.h>
#include <BLE.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include "BluetoothSerial.h"
#pragma GCC diagnostic pop

BluetoothSerial SerialBT;

String serverName;
String clientName;
volatile int currentPhase = 0;

// Server address captured in phase 2 and reused for the discovery callback.
static BTAddress connectedPeer;

// Security phase state
static volatile bool authOk = false;
static volatile bool authFailed = false;
static volatile bool rejectPasskey = false;
static volatile bool confirmAccept = true;

// ---------------------------------------------------------------------------
// Phase coordination (same pattern as BLE validation tests)
// ---------------------------------------------------------------------------

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

void waitForPhase(int n) {
  while (currentPhase < n) {
    checkSerial();
    delay(10);
  }
}

static void resetSecurityState() {
  authOk = false;
  authFailed = false;
}

static void waitForDisconnect(uint32_t timeoutMs) {
  uint32_t deadline = millis() + timeoutMs;
  while (SerialBT.connected() && millis() < deadline) {
    checkSerial();
    delay(20);
  }
}

// Runs on the Bluetooth host task when the server displays a passkey. The
// test script sends the six digits as "PASSKEY=<value>" once the server has
// printed them.
uint32_t passkeyRequestHandler(const BTAddress &peer) {
  Serial.printf("[CLIENT] Passkey requested by %s reject=%d\n", peer.toString().c_str(), (int)rejectPasskey);
  if (rejectPasskey) {
    Serial.println("[CLIENT] Passkey rejected");
    return 0xFFFFFFFFu;  // > 999999: explicit negative reply
  }

  String buf;
  uint32_t deadline = millis() + 20000;
  while (millis() < deadline) {
    while (Serial.available()) {
      char c = (char)Serial.read();
      if (c == '\n') {
        buf.trim();
        if (buf.startsWith("PASSKEY=")) {
          uint32_t value = (uint32_t)strtoul(buf.c_str() + 8, nullptr, 10);
          Serial.printf("[CLIENT] Passkey=%06lu\n", (unsigned long)value);
          return value;
        }
        buf = "";
      } else if (c != '\r') {
        buf += c;
      }
    }
    delay(10);
  }

  Serial.println("[CLIENT] Passkey input timeout");
  return 0xFFFFFFFFu;
}

// ---------------------------------------------------------------------------
// Setup: run all phases sequentially
// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }

  // Receive server name from test script
  Serial.println("[CLIENT] Send server name:");
  String input;
  while (input.length() == 0) {
    if (Serial.available()) {
      input = Serial.readStringUntil('\n');
      input.trim();
    }
    delay(50);
  }
  serverName = input;
  clientName = serverName + "_C";
  Serial.printf("[CLIENT] Server: %s\n", serverName.c_str());

  // Security callbacks are registered once and persist across end()/begin().
  SerialBT.onAuthComplete([](bool success) {
    if (success) {
      authOk = true;
    } else {
      authFailed = true;
    }
    Serial.printf("[CLIENT] AuthComplete=%d\n", (int)success);
  });
  SerialBT.onPassKeyRequest(passkeyRequestHandler);
  SerialBT.onConfirmRequest([](uint32_t value) {
    bool accept = (bool)confirmAccept;
    Serial.printf("[CLIENT] Confirm=%06lu accept=%d\n", (unsigned long)value, (int)accept);
    return accept;
  });

  // -------------------------------------------------------------------------
  // Phase 1: basic lifecycle
  // -------------------------------------------------------------------------
  waitForPhase(1);

  // Start BLE before Classic: BLE owns the shared Bluedroid host and
  // BluetoothSerial adopts it below.
  BTStatus bleOwner = BLE.begin(clientName + "_ble");
  if (!bleOwner) {
    Serial.println("[CLIENT] BLE owner init FAILED");
    while (true) {
      delay(1000);
    }
  }

  BTStatus status = SerialBT.begin(clientName, true);
  if (!status) {
    Serial.println("[CLIENT] Init FAILED");
    while (true) {
      delay(1000);
    }
  }
  Serial.printf("[CLIENT] Init OK, addr: %s\n", SerialBT.getAddress().toString().c_str());

  SerialBT.end(false);
  Serial.println("[CLIENT] Deinit OK");

  status = SerialBT.begin(clientName, true);
  if (!status) {
    Serial.println("[CLIENT] Reinit FAILED");
    while (true) {
      delay(1000);
    }
  }
  Serial.println("[CLIENT] Reinit OK");
  // Stay initialized for phase 2.

  // -------------------------------------------------------------------------
  // Phase 2: SPP connect + bidirectional data exchange
  // -------------------------------------------------------------------------
  waitForPhase(2);

  Serial.println("[CLIENT] Connecting...");
  BTStatus connectStatus = SerialBT.connect(serverName, 30000);
  if (!connectStatus) {
    Serial.printf("[CLIENT] Connect FAILED: %s\n", connectStatus.toString());
    while (true) {
      delay(1000);
    }
  }
  Serial.println("[CLIENT] Connected");
  Serial.printf("[CLIENT] peerCount=%u\n", SerialBT.peerCount());
  {
    auto peerList = SerialBT.peers();
    Serial.printf("[CLIENT] peers=%u\n", (unsigned)peerList.size());
    if (!peerList.empty()) {
      connectedPeer = peerList[0];
      Serial.printf("[CLIENT] peerAddr=%s\n", peerList[0].toString().c_str());
    }
  }

  // Targeted writeTo round-trip
  SerialBT.print("SPP_WRITE_TO_PROBE");
  Serial.println("[CLIENT] Sent: SPP_WRITE_TO_PROBE");

  {
    String rx;
    uint32_t deadline = millis() + 15000;
    while (millis() < deadline) {
      while (SerialBT.available()) {
        rx += (char)SerialBT.read();
      }
      if (rx.indexOf("SPP_WRITE_TO_ACK") >= 0) {
        break;
      }
      delay(10);
    }
    if (rx.indexOf("SPP_WRITE_TO_ACK") >= 0) {
      Serial.println("[CLIENT] Received: SPP_WRITE_TO_ACK");
    } else {
      Serial.println("[CLIENT] writeTo ack timeout");
    }
  }

  // Broadcast path
  SerialBT.print("HELLO_FROM_CLIENT");
  Serial.println("[CLIENT] Sent: HELLO_FROM_CLIENT");

  // Receive response from server
  String rx;
  uint32_t deadline = millis() + 15000;
  while (millis() < deadline) {
    while (SerialBT.available()) {
      rx += (char)SerialBT.read();
    }
    if (rx.indexOf("HELLO_FROM_SERVER") >= 0) {
      break;
    }
    delay(10);
  }
  if (rx.indexOf("HELLO_FROM_SERVER") >= 0) {
    Serial.println("[CLIENT] Received: HELLO_FROM_SERVER");
  } else {
    Serial.println("[CLIENT] Receive timeout");
  }

  // BLE alongside the live SPP link (see the server side). BLE started first
  // (phase 1), so ending it must leave the shared host up for Classic.
  // Wait for the server's COEX_BLE_SEEN over SPP before tearing BLE down;
  // otherwise a fast teardown can fall between the server's polls and report
  // connected=0. Timeout proceeds anyway (the server latches via callback).
  {
    BTStatus bs = BLE.begin(clientName + "_ble");
    bool bleConn = false;
    BLEClient cli;
    BLEScan scn = BLE.getScan();
    scn.setActiveScan(true);
    unsigned long coexDl = millis() + 20000;
    while (!bleConn && millis() < coexDl) {
      for (const auto &dev : scn.startBlocking(3000)) {
        BLEAdvertisedDevice d = dev;
        if (d.getName() == serverName + "_ble") {
          cli = BLE.createClient();
          bleConn = static_cast<bool>(cli.connect(d.getAddress()));
          break;
        }
      }
      scn.clearResults();
    }
    Serial.printf("[CLIENT] Coex ble=%d spp=%d\n", (int)((bool)bs && bleConn), (int)SerialBT.connected());
    if (bleConn) {
      String coexAck;
      unsigned long ackDl = millis() + 10000;
      while (millis() < ackDl) {
        while (SerialBT.available()) {
          coexAck += (char)SerialBT.read();
        }
        if (coexAck.indexOf("COEX_BLE_SEEN") >= 0) {
          break;
        }
        delay(20);
      }
    }
    BLE.end(false);
    Serial.printf("[CLIENT] Coex teardown spp=%d\n", (int)SerialBT.connected());

    // Now BLE adopts the Classic-owned host and ends as a non-owner.
    bs = BLE.begin(clientName + "_ble_adopt");
    Serial.printf("[CLIENT] Coex adopt ble=%d spp=%d\n", (int)(bool)bs, (int)SerialBT.connected());
    BLE.end(false);
    Serial.printf("[CLIENT] Coex adopt teardown spp=%d\n", (int)SerialBT.connected());

    // end(true) while Classic still owns the host: record the BLE terminal
    // contract, leave the host up, and refuse a later BLE begin.
    bs = BLE.begin(clientName + "_ble_endtrue");
    BLE.end(true);
    bool endTrueKeep = SerialBT.connected();
    BTStatus endTrueAgain = BLE.begin(clientName + "_ble_endtrue2");
    Serial.printf("[CLIENT] Coex endtrue begin=%d keep=%d again=%d\n", (int)(bool)bs, (int)endTrueKeep, (int)(bool)endTrueAgain);
  }

  // Ask server to disconnect this peer by address
  SerialBT.print("DISCONNECT_ME");
  Serial.println("[CLIENT] Sent: DISCONNECT_ME");

  deadline = millis() + 15000;
  while (SerialBT.connected() && millis() < deadline) {
    delay(20);
  }
  if (!SerialBT.connected()) {
    Serial.println("[CLIENT] Disconnected by server");
  } else {
    SerialBT.disconnect();
    Serial.println("[CLIENT] Disconnected");
  }

  // -------------------------------------------------------------------------
  // Phase 3: bond management
  // -------------------------------------------------------------------------
  waitForPhase(3);

  auto bonds = SerialBT.getBondedDevices();
  Serial.printf("[CLIENT] Bonds: %d\n", (int)bonds.size());

  BTStatus deleteStatus = SerialBT.deleteAllBonds();
  Serial.printf("[CLIENT] DeleteAllBonds: %s\n", deleteStatus ? "OK" : "FAILED");

  bonds = SerialBT.getBondedDevices();
  Serial.printf("[CLIENT] Bonds after delete: %d\n", (int)bonds.size());

  // -------------------------------------------------------------------------
  // Phase 4: discovery APIs
  // -------------------------------------------------------------------------
  waitForPhase(4);

  Serial.println("[CLIENT] Discovering...");
  bool found = false;
  uint32_t discoverCount = 0;
  uint8_t discoverAttempts = 0;
  for (; discoverAttempts < 2 && !found; discoverAttempts++) {
    if (discoverAttempts > 0) {
      delay(3000);
    }
    auto results = SerialBT.discover(8000);
    discoverCount = results.size();
    for (const auto &result : results) {
      if (result.name == serverName) {
        found = true;
        break;
      }
    }
  }
  Serial.printf("[CLIENT] Discover found=%d count=%lu attempts=%u\n", (int)found, (unsigned long)discoverCount, discoverAttempts);

  delay(2000);

  // Match on the address learned in phase 2 as well: an inquiry result may
  // arrive before its EIR name, and the async path is about the callback, not
  // name resolution (connect(name) already covers that).
  static volatile bool asyncFound = false;
  static volatile uint32_t asyncCount = 0;
  uint8_t asyncAttempts = 0;
  bool asyncStarted = false;
  BTStatus stopStatus = BTStatus::Fail;
  asyncFound = false;
  asyncCount = 0;
  for (; asyncAttempts < 3 && !asyncFound; asyncAttempts++) {
    if (asyncAttempts > 0) {
      delay(3000);
    }
    asyncFound = false;
    asyncCount = 0;
    BTStatus asyncStatus = SerialBT.discoverAsync(
      [](const BluetoothSerial::DiscoveryResult &result) {
        asyncCount++;
        if (result.name == serverName || result.address == connectedPeer) {
          asyncFound = true;
        }
      },
      20000
    );
    if (!asyncStatus) {
      stopStatus = asyncStatus;
      break;
    }
    asyncStarted = true;
    uint32_t asyncDeadline = millis() + 15000;
    while (!asyncFound && millis() < asyncDeadline) {
      delay(10);
    }
    stopStatus = SerialBT.discoverStop();
    delay(1000);
  }
  Serial.printf(
    "[CLIENT] DiscoverAsync started=%d seen=%d attempts=%u results=%lu\n", (int)asyncStarted, (int)asyncFound, asyncAttempts, (unsigned long)asyncCount
  );
  Serial.printf("[CLIENT] DiscoverStop=%s\n", stopStatus ? "OK" : "FAILED");

  // -------------------------------------------------------------------------
  // Phase 5: SSP passkey entry (KeyboardOnly)
  // -------------------------------------------------------------------------
  waitForPhase(5);

  resetSecurityState();
  rejectPasskey = false;
  (void)SerialBT.deleteAllBonds();
  SerialBT.end(false);
  SerialBT.enableSSP(true, false);  // input only -> KeyboardOnly
  BTStatus sspStatus = SerialBT.begin(clientName, true);
  Serial.printf("[CLIENT] Passkey begin=%d\n", (int)(bool)sspStatus);

  Serial.println("[CLIENT] Connecting (passkey)...");
  BTStatus cs = SerialBT.connect(serverName, 30000);
  Serial.printf("[CLIENT] Passkey connect=%s\n", cs ? "OK" : cs.toString());

  if (cs) {
    SerialBT.print("PASSKEY_PROBE");
    String probeRx;
    uint32_t probeDeadline = millis() + 20000;
    while (millis() < probeDeadline) {
      while (SerialBT.available()) {
        probeRx += (char)SerialBT.read();
      }
      if (probeRx.indexOf("PASSKEY_ACK") >= 0) {
        break;
      }
      delay(10);
    }
    if (probeRx.indexOf("PASSKEY_ACK") >= 0) {
      Serial.println("[CLIENT] Received: PASSKEY_ACK");
    } else {
      Serial.println("[CLIENT] PASSKEY_ACK timeout");
    }
    SerialBT.disconnect();
    waitForDisconnect(10000);
  }
  Serial.printf("[CLIENT] Passkey done connected=%d auth=%d\n", (int)SerialBT.connected(), (int)authOk);

  // -------------------------------------------------------------------------
  // Phase 6: SSP numeric comparison (DisplayYesNo)
  // -------------------------------------------------------------------------
  waitForPhase(6);

  resetSecurityState();
  confirmAccept = true;
  (void)SerialBT.deleteAllBonds();
  SerialBT.end(false);
  SerialBT.enableSSP(true, true);  // input + output -> DisplayYesNo
  sspStatus = SerialBT.begin(clientName, true);
  Serial.printf("[CLIENT] Numeric begin=%d\n", (int)(bool)sspStatus);

  Serial.println("[CLIENT] Connecting (numeric)...");
  cs = SerialBT.connect(serverName, 30000);
  Serial.printf("[CLIENT] Numeric connect=%s\n", cs ? "OK" : cs.toString());

  if (cs) {
    SerialBT.print("NUMERIC_PROBE");
    String probeRx;
    uint32_t probeDeadline = millis() + 20000;
    while (millis() < probeDeadline) {
      while (SerialBT.available()) {
        probeRx += (char)SerialBT.read();
      }
      if (probeRx.indexOf("NUMERIC_ACK") >= 0) {
        break;
      }
      delay(10);
    }
    if (probeRx.indexOf("NUMERIC_ACK") >= 0) {
      Serial.println("[CLIENT] Received: NUMERIC_ACK");
    } else {
      Serial.println("[CLIENT] NUMERIC_ACK timeout");
    }
    SerialBT.disconnect();
    waitForDisconnect(10000);
  }
  Serial.printf("[CLIENT] Numeric done connected=%d auth=%d\n", (int)SerialBT.connected(), (int)authOk);

  // -------------------------------------------------------------------------
  // Phase 7: SSP passkey entry rejected by this side
  // -------------------------------------------------------------------------
  waitForPhase(7);

  resetSecurityState();
  rejectPasskey = true;
  (void)SerialBT.deleteAllBonds();
  SerialBT.end(false);
  SerialBT.enableSSP(true, false);  // KeyboardOnly
  sspStatus = SerialBT.begin(clientName, true);
  Serial.printf("[CLIENT] Reject begin=%d\n", (int)(bool)sspStatus);

  Serial.println("[CLIENT] Connecting (reject)...");
  uint32_t startMs = millis();
  cs = SerialBT.connect(serverName, 12000);
  uint32_t elapsedMs = millis() - startMs;
  Serial.printf("[CLIENT] Reject connect=%s elapsed=%lu\n", cs ? "OK" : cs.toString(), (unsigned long)elapsedMs);

  // -------------------------------------------------------------------------
  // Phase 8: memory release
  // -------------------------------------------------------------------------
  waitForPhase(8);

  SerialBT.end(true);
  Serial.println("[CLIENT] Memory released");

  while (true) {
    delay(1000);
  }
}

void loop() {
  checkSerial();
  delay(10);
}
