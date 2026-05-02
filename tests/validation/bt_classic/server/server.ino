// BT Classic validation test — SERVER (SPP acceptor)
// Phases:
//   1. basic_lifecycle  — begin / deinit / reinit
//   2. spp_connect_data — accept connection, bidirectional data exchange
//   3. bond_management  — list bonds, delete all, verify empty
//   4. memory_release   — end(true)

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include "BluetoothSerial.h"
#pragma GCC diagnostic pop

BluetoothSerial SerialBT;

String serverName;
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

  // Receive data from client
  String rx;
  deadline = millis() + 15000;
  while (millis() < deadline) {
    while (SerialBT.available()) {
      rx += (char)SerialBT.read();
    }
    if (rx.indexOf("HELLO_FROM_CLIENT") >= 0) {
      break;
    }
    checkSerial();
    delay(10);
  }
  if (rx.indexOf("HELLO_FROM_CLIENT") >= 0) {
    Serial.println("[SERVER] Received: HELLO_FROM_CLIENT");
  } else {
    Serial.println("[SERVER] Receive timeout");
  }

  // Send response
  SerialBT.print("HELLO_FROM_SERVER");
  Serial.println("[SERVER] Sent: HELLO_FROM_SERVER");

  // Wait for client to disconnect
  deadline = millis() + 15000;
  while (SerialBT.connected() && millis() < deadline) {
    checkSerial();
    delay(20);
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
