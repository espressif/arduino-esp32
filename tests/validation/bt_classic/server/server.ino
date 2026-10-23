// BT Classic validation test — SERVER (SPP acceptor)
// Phases:
//   1. basic_lifecycle  — begin / deinit / reinit
//   2. spp_connect_data — accept connection, bidirectional data exchange
//   3. bond_management  — list bonds, delete all, verify empty
//   4. memory_release   — end(true)

#include <Arduino.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include "BluetoothSerial.h"
#pragma GCC diagnostic pop

BluetoothSerial SerialBT;

String serverName;
volatile int currentPhase = 0;

static String peerRx;
static volatile bool onConnectFired = false;
static volatile bool writeToProbeDone = false;
static BTAddress connectedPeer;

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

  // -------------------------------------------------------------------------
  // Phase 1: basic lifecycle
  // -------------------------------------------------------------------------
  waitForPhase(1);

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
    deadline = millis() + 5000;
    while (SerialBT.peerCount() > 0 && millis() < deadline) {
      checkSerial();
      delay(10);
    }
    Serial.printf("[SERVER] peerCount=%u\n", SerialBT.peerCount());
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
  // Phase 4: memory release
  // -------------------------------------------------------------------------
  waitForPhase(4);

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
