// BT Classic validation test — CLIENT (SPP initiator)
// Phases:
//   1. basic_lifecycle  — begin / deinit / reinit
//   2. spp_connect_data — discover server by name, connect, bidirectional data, disconnect
//   3. bond_management  — list bonds, delete all, verify empty
//   4. memory_release   — end(true)

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
  {
    BTStatus bs = BLE.begin(clientName + "_ble");
    bool bleConn = false;
    BLEScan scn = BLE.getScan();
    scn.setActiveScan(true);
    unsigned long coexDl = millis() + 20000;
    while (!bleConn && millis() < coexDl) {
      for (const auto &dev : scn.startBlocking(3000)) {
        BLEAdvertisedDevice d = dev;
        if (d.getName() == serverName + "_ble") {
          BLEClient cli = BLE.createClient();
          bleConn = static_cast<bool>(cli.connect(d.getAddress()));
          break;
        }
      }
      scn.clearResults();
    }
    Serial.printf("[CLIENT] Coex ble=%d spp=%d\n", (int)((bool)bs && bleConn), (int)SerialBT.connected());
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
    Serial.printf(
      "[CLIENT] Coex endtrue begin=%d keep=%d again=%d\n", (int)(bool)bs, (int)endTrueKeep, (int)(bool)endTrueAgain
    );
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
  // Phase 4: memory release
  // -------------------------------------------------------------------------
  waitForPhase(4);

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
