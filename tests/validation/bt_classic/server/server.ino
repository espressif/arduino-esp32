// BT Classic validation test — SERVER (SPP acceptor)
// Phases:
//   1. basic_lifecycle     — begin / deinit / reinit
//   2. spp_connect_data    — accept connection, bidirectional data exchange
//   3. bond_management     — list bonds, delete all, verify empty
//   4. discovery           — stay discoverable for the client's scan APIs
//   5. ssp_passkey_entry   — DisplayOnly: print the generated passkey, accept
//                            the KeyboardOnly client after it enters it
//   6. ssp_numeric_compare — DisplayYesNo: confirm the numeric value
//   7. ssp_passkey_reject  — DisplayOnly: peer rejects passkey entry; the
//                            failed authentication must be reported promptly
//   8. memory_release      — end(true)

#include <Arduino.h>
#include <BLE.h>
#include "esp32-hal-bt.h"
#include "esp_bt_main.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include "BluetoothSerial.h"
#pragma GCC diagnostic pop

BluetoothSerial SerialBT;

String serverName;
volatile int currentPhase = 0;

static String peerRx;
static volatile bool onConnectFired = false;
static volatile bool onDisconnectFired = false;
static volatile bool writeToProbeDone = false;
static BTAddress connectedPeer;

// Security phase state
static volatile bool authOk = false;
static volatile bool authFailed = false;
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
          Serial.printf("[SERVER] Phase %d started\n", phase);
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

// Wait until the client disconnects, then print a one-line phase summary.
static void waitForPeerDisconnect(uint32_t timeoutMs) {
  uint32_t deadline = millis() + timeoutMs;
  while (SerialBT.connected() && millis() < deadline) {
    checkSerial();
    delay(20);
  }
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
  Serial.println("[SERVER] Send name:");
  String input;
  while (input.length() == 0) {
    if (Serial.available()) {
      input = Serial.readStringUntil('\n');
      input.trim();
    }
    delay(50);
  }
  serverName = input;
  Serial.printf("[SERVER] Name: %s\n", serverName.c_str());

  // Security callbacks are registered once and persist across end()/begin().
  SerialBT.onAuthComplete([](bool success) {
    if (success) {
      authOk = true;
    } else {
      authFailed = true;
    }
    Serial.printf("[SERVER] AuthComplete=%d\n", (int)success);
  });
  SerialBT.onPassKeyDisplay([](const BTAddress &peer, uint32_t passkey) {
    Serial.printf("[SERVER] Passkey=%06lu peer=%s\n", (unsigned long)passkey, peer.toString().c_str());
  });
  SerialBT.onConfirmRequest([](uint32_t value) {
    bool accept = (bool)confirmAccept;
    Serial.printf("[SERVER] Confirm=%06lu accept=%d\n", (unsigned long)value, (int)accept);
    return accept;
  });

  // -------------------------------------------------------------------------
  // Phase 1: basic lifecycle
  // -------------------------------------------------------------------------
  waitForPhase(1);

  // External host adoption: another component may enable Bluedroid before the
  // Arduino libraries. The shared manager must adopt it and leave it up when
  // each library ends; only the external owner may tear it down.
  if (btStart()) {
    (void)esp_bluedroid_init();
    (void)esp_bluedroid_enable();
  }
  BTStatus extBle = BLE.begin(serverName + "_ext");
  BLE.end(false);
  bool extBleKeep = (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_ENABLED);
  BTStatus extSpp = SerialBT.begin(serverName, false);
  SerialBT.end(false);
  bool extSppKeep = (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_ENABLED);
  esp_bluedroid_disable();
  esp_bluedroid_deinit();
  btStop();
  Serial.printf("[SERVER] ExtHost ble=%d bleKeep=%d spp=%d sppKeep=%d\n", (int)(bool)extBle, (int)extBleKeep, (int)(bool)extSpp, (int)extSppKeep);

  // Start BLE before Classic: BLE owns the shared Bluedroid host and
  // BluetoothSerial adopts it below. The reverse order (Classic owns, BLE
  // adopts) is exercised again after the SPP exchange.
  BTStatus bleOwner = BLE.begin(serverName + "_ble");
  if (!bleOwner) {
    Serial.println("[SERVER] BLE owner init FAILED");
    while (true) {
      delay(1000);
    }
  }

  BTStatus status = SerialBT.begin(serverName, false);
  if (!status) {
    Serial.println("[SERVER] Init FAILED");
    while (true) {
      delay(1000);
    }
  }
  Serial.printf("[SERVER] Init OK, addr: %s\n", SerialBT.getAddress().toString().c_str());

  SerialBT.end(false);
  Serial.println("[SERVER] Deinit OK");

  status = SerialBT.begin(serverName, false);
  if (!status) {
    Serial.println("[SERVER] Reinit FAILED");
    while (true) {
      delay(1000);
    }
  }
  Serial.println("[SERVER] Reinit OK");

  SerialBT.onConnect([](const BTAddress &addr) {
    onConnectFired = true;
    connectedPeer = addr;
    Serial.printf("[SERVER] onConnect: %s\n", addr.toString().c_str());
  });

  SerialBT.onDisconnect([](const BTAddress &addr) {
    onDisconnectFired = true;
    Serial.printf("[SERVER] onDisconnect: %s\n", addr.toString().c_str());
  });

  SerialBT.onPeerData([](const BTAddress &addr, const uint8_t *data, size_t len) {
    if (len >= 18 && memcmp(data, "SPP_WRITE_TO_PROBE", 18) == 0) {
      Serial.printf("[SERVER] onPeerData: %s\n", addr.toString().c_str());
      SerialBT.writeTo(addr, (const uint8_t *)"SPP_WRITE_TO_ACK", 16);
      writeToProbeDone = true;
      return;
    }
    for (size_t i = 0; i < len; i++) {
      peerRx += (char)data[i];
    }
  });

  // Stay initialized; the SPP server is now accepting connections.

  // -------------------------------------------------------------------------
  // Phase 2: SPP connect + bidirectional data exchange
  // -------------------------------------------------------------------------
  waitForPhase(2);

  Serial.println("[SERVER] Waiting for client...");

  uint32_t deadline = millis() + 60000;
  while (!SerialBT.connected() && millis() < deadline) {
    checkSerial();
    delay(20);
  }
  if (!SerialBT.connected()) {
    Serial.println("[SERVER] Connect timeout");
    while (true) {
      delay(1000);
    }
  }
  Serial.println("[SERVER] Client connected");
  Serial.printf("[SERVER] peerCount=%u\n", SerialBT.peerCount());

  deadline = millis() + 5000;
  while (!onConnectFired && millis() < deadline) {
    checkSerial();
    delay(10);
  }
  if (!onConnectFired) {
    Serial.println("[SERVER] onConnect timeout");
  }

  {
    auto peerList = SerialBT.peers();
    Serial.printf("[SERVER] peers=%u\n", (unsigned)peerList.size());
    if (!peerList.empty()) {
      Serial.printf("[SERVER] peerAddr=%s\n", peerList[0].toString().c_str());
    }
  }

  // Wait for writeTo probe handled in onPeerData
  deadline = millis() + 15000;
  while (!writeToProbeDone && millis() < deadline) {
    checkSerial();
    delay(10);
  }
  if (writeToProbeDone) {
    Serial.println("[SERVER] writeTo ack sent");
  } else {
    Serial.println("[SERVER] writeTo probe timeout");
  }

  // Receive data from client (HELLO and disconnect request via onPeerData)
  peerRx = "";
  deadline = millis() + 15000;
  while (millis() < deadline) {
    if (peerRx.indexOf("HELLO_FROM_CLIENT") >= 0) {
      break;
    }
    checkSerial();
    delay(10);
  }
  if (peerRx.indexOf("HELLO_FROM_CLIENT") >= 0) {
    Serial.println("[SERVER] Received: HELLO_FROM_CLIENT");
  } else {
    Serial.println("[SERVER] Receive timeout");
  }

  // Send response (broadcast write)
  SerialBT.print("HELLO_FROM_SERVER");
  Serial.println("[SERVER] Sent: HELLO_FROM_SERVER");

  // BLE alongside the live SPP link. BLE started first (phase 1), so it owns
  // the shared Bluedroid host; ending it must leave the host up for Classic.
  // The link is latched via onConnect (not polling alone) so a fast
  // teardown cannot fall between two polls and report connected=0.
  // The server sends COEX_BLE_SEEN over SPP once observed; the client waits
  // for it before ending BLE, closing the race deterministically.
  {
    BTStatus bs = BLE.begin(serverName + "_ble");
    BLEServer srv = BLE.createServer();
    static volatile bool coexBleSeen = false;
    coexBleSeen = false;
    srv.onConnect([](BLEServer, const BLEConnInfo &) {
      coexBleSeen = true;
    });
    BLEAdvertising adv = BLE.getAdvertising();
    adv.setType(BLEAdvType::ConnectableScannable);
    adv.setName(serverName + "_ble");
    BTStatus as = adv.start();
    unsigned long coexDl = millis() + 30000;
    while (!coexBleSeen && srv.getConnectedCount() == 0 && millis() < coexDl) {
      checkSerial();
      delay(20);
    }
    bool bleConn = coexBleSeen || srv.getConnectedCount() > 0;
    Serial.printf("[SERVER] Coex ble=%d adv=%d connected=%d spp=%d\n", (int)(bool)bs, (int)(bool)as, (int)bleConn, (int)SerialBT.hasClient());
    SerialBT.print("COEX_BLE_SEEN");
    BLE.end(false);
    Serial.printf("[SERVER] Coex teardown spp=%d\n", (int)SerialBT.hasClient());

    // Now BLE adopts the Classic-owned host and ends as a non-owner.
    bs = BLE.begin(serverName + "_ble_adopt");
    Serial.printf("[SERVER] Coex adopt ble=%d spp=%d\n", (int)(bool)bs, (int)SerialBT.hasClient());
    BLE.end(false);
    Serial.printf("[SERVER] Coex adopt teardown spp=%d\n", (int)SerialBT.hasClient());

    // end(true) while Classic still owns the host: record the BLE terminal
    // contract, leave the host up, and refuse a later BLE begin.
    bs = BLE.begin(serverName + "_ble_endtrue");
    BLE.end(true);
    bool endTrueKeep = SerialBT.hasClient();
    BTStatus endTrueAgain = BLE.begin(serverName + "_ble_endtrue2");
    Serial.printf("[SERVER] Coex endtrue begin=%d keep=%d again=%d\n", (int)(bool)bs, (int)endTrueKeep, (int)(bool)endTrueAgain);
  }

  // Server-initiated per-peer disconnect
  deadline = millis() + 15000;
  while (millis() < deadline) {
    if (peerRx.indexOf("DISCONNECT_ME") >= 0) {
      break;
    }
    checkSerial();
    delay(10);
  }
  if (peerRx.indexOf("DISCONNECT_ME") >= 0) {
    BTStatus disc = SerialBT.disconnect(connectedPeer);
    Serial.printf("[SERVER] disconnect(addr): %s\n", disc ? "OK" : "FAILED");
    // onDisconnect may fire before or after disconnect() returns; wait for both
    // so the summary below is deterministic for the test script.
    deadline = millis() + 5000;
    while ((SerialBT.peerCount() > 0 || !onDisconnectFired) && millis() < deadline) {
      checkSerial();
      delay(10);
    }
    Serial.printf("[SERVER] disconnect done peerCount=%u callback=%d\n", SerialBT.peerCount(), (int)onDisconnectFired);
  } else {
    Serial.println("[SERVER] disconnect request timeout");
  }

  Serial.println("[SERVER] Client disconnected");

  // -------------------------------------------------------------------------
  // Phase 3: bond management
  // -------------------------------------------------------------------------
  waitForPhase(3);

  auto bonds = SerialBT.getBondedDevices();
  Serial.printf("[SERVER] Bonds: %d\n", (int)bonds.size());

  BTStatus deleteStatus = SerialBT.deleteAllBonds();
  Serial.printf("[SERVER] DeleteAllBonds: %s\n", deleteStatus ? "OK" : "FAILED");

  bonds = SerialBT.getBondedDevices();
  Serial.printf("[SERVER] Bonds after delete: %d\n", (int)bonds.size());

  // -------------------------------------------------------------------------
  // Phase 4: discovery — stay discoverable while the client scans
  // -------------------------------------------------------------------------
  waitForPhase(4);
  Serial.println("[SERVER] Discovery ready");

  // -------------------------------------------------------------------------
  // Phase 5: SSP passkey entry (DisplayOnly)
  // -------------------------------------------------------------------------
  waitForPhase(5);

  resetSecurityState();
  (void)SerialBT.deleteAllBonds();
  SerialBT.end(false);
  SerialBT.enableSSP(false, true);  // output only -> DisplayOnly
  BTStatus sspStatus = SerialBT.begin(serverName, false);
  Serial.printf("[SERVER] Passkey begin=%d\n", (int)(bool)sspStatus);

  peerRx = "";
  deadline = millis() + 60000;
  while (!SerialBT.connected() && millis() < deadline) {
    checkSerial();
    delay(20);
  }
  Serial.printf("[SERVER] Passkey connected=%d\n", (int)SerialBT.connected());

  if (SerialBT.connected()) {
    deadline = millis() + 20000;
    while (millis() < deadline && peerRx.indexOf("PASSKEY_PROBE") < 0) {
      checkSerial();
      delay(10);
    }
    bool probe = peerRx.indexOf("PASSKEY_PROBE") >= 0;
    Serial.printf("[SERVER] Passkey probe=%d\n", (int)probe);
    if (probe) {
      SerialBT.print("PASSKEY_ACK");
    }
    waitForPeerDisconnect(20000);
  }
  Serial.printf("[SERVER] Passkey done connected=%d auth=%d\n", (int)SerialBT.connected(), (int)authOk);

  // -------------------------------------------------------------------------
  // Phase 6: SSP numeric comparison (DisplayYesNo)
  // -------------------------------------------------------------------------
  waitForPhase(6);

  resetSecurityState();
  confirmAccept = true;
  (void)SerialBT.deleteAllBonds();
  SerialBT.end(false);
  SerialBT.enableSSP(true, true);  // input + output -> DisplayYesNo
  sspStatus = SerialBT.begin(serverName, false);
  Serial.printf("[SERVER] Numeric begin=%d\n", (int)(bool)sspStatus);

  peerRx = "";
  deadline = millis() + 60000;
  while (!SerialBT.connected() && millis() < deadline) {
    checkSerial();
    delay(20);
  }
  Serial.printf("[SERVER] Numeric connected=%d\n", (int)SerialBT.connected());

  if (SerialBT.connected()) {
    deadline = millis() + 20000;
    while (millis() < deadline && peerRx.indexOf("NUMERIC_PROBE") < 0) {
      checkSerial();
      delay(10);
    }
    bool probe = peerRx.indexOf("NUMERIC_PROBE") >= 0;
    Serial.printf("[SERVER] Numeric probe=%d\n", (int)probe);
    if (probe) {
      SerialBT.print("NUMERIC_ACK");
    }
    waitForPeerDisconnect(20000);
  }
  Serial.printf("[SERVER] Numeric done connected=%d auth=%d\n", (int)SerialBT.connected(), (int)authOk);

  // -------------------------------------------------------------------------
  // Phase 7: SSP passkey entry rejected by the peer
  // -------------------------------------------------------------------------
  waitForPhase(7);

  resetSecurityState();
  (void)SerialBT.deleteAllBonds();
  SerialBT.end(false);
  SerialBT.enableSSP(false, true);  // DisplayOnly; the client is KeyboardOnly
  sspStatus = SerialBT.begin(serverName, false);
  Serial.printf("[SERVER] Reject begin=%d\n", (int)(bool)sspStatus);

  deadline = millis() + 20000;
  while (!authFailed && !SerialBT.connected() && millis() < deadline) {
    checkSerial();
    delay(20);
  }
  Serial.printf("[SERVER] Reject authFail=%d connected=%d\n", (int)authFailed, (int)SerialBT.connected());

  // -------------------------------------------------------------------------
  // Phase 8: memory release
  // -------------------------------------------------------------------------
  waitForPhase(8);

  SerialBT.end(true);
  Serial.println("[SERVER] Memory released");

  while (true) {
    delay(1000);
  }
}

void loop() {
  checkSerial();
  delay(10);
}
