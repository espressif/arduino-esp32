/*
  Open-Drain One-Wire UART Between Two Boards

  Flash the same sketch on both boards. Set NODE_ROLE differently on each board.
  Same-pin UART (RX == TX) automatically enables open-drain.

  Wiring (external pull-up REQUIRED):
                        3.3 V
                          |
                   pull-up resistor
                          |
    Board A BUS_PIN  -----+-----  Board B BUS_PIN
    Board A GND      -----------  Board B GND

  Local TX is also received on RX. sendLine() discards that self-echo before
  waiting for the peer.

  --- Single board (UART1 + UART2) ---
  On SoCs with Serial1 and Serial2 (typically ESP32, ESP32-S3, ESP32-P4),
  you can run initiator and responder on one board:

  1. Remove NODE_ROLE (and ROLE_INITIATOR / ROLE_RESPONDER). That define
     picks only one role per flash; a single board must run both.
  2. Remove the #if NODE_ROLE branches in setup()/loop() that call either
     runInitiator() or runResponder().
  3. Use two same-pin UARTs (Serial1 and Serial2), two BUS pins, and wire
     them together with an external pull-up to 3.3 V.

  Full steps, capable boards, wiring diagram, and code outline: README.md
*/

#include <Arduino.h>

#define ROLE_INITIATOR 1
#define ROLE_RESPONDER 2

// Change this per board: one INITIATOR, one RESPONDER.
// For a single-board UART1+UART2 setup, remove NODE_ROLE entirely (see header / README.md).
#define NODE_ROLE ROLE_INITIATOR

#ifndef BUS_PIN
#ifdef RX1
#define BUS_PIN RX1
#else
#error "Define BUS_PIN to a GPIO supported by Serial1 on this board"
#endif
#endif

#ifndef LINK_BAUD
#define LINK_BAUD 115200
#endif

static HardwareSerial &Link = Serial1;

static void drainLink() {
  while (Link.available()) {
    Link.read();
  }
}

// Write one line, then discard the local self-echo of that same line.
static void sendLine(const String &line) {
  drainLink();

  String wire = line + "\n";
  Link.print(wire);
  Link.flush();
  delay(2);  // let the last stop bit reach RX

  // Same-pin RX sees our own TX — read it back and drop it.
  uint32_t start = millis();
  size_t got = 0;
  while (got < wire.length() && millis() - start < 300) {
    if (Link.available()) {
      Link.read();
      got++;
    } else {
      delay(1);
    }
  }

  // Drop any post-TX glitch bytes before listening for the peer.
  delay(10);
  drainLink();
}

static bool readLine(String &line, uint32_t timeoutMs) {
  line = "";
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    while (Link.available()) {
      char c = (char)Link.read();
      if (c == '\n') {
        line.trim();
        return true;
      }
      if (c != '\r' && line.length() < 64) {
        line += c;
      }
    }
    delay(1);
  }
  return false;
}

static void runInitiator() {
  static uint32_t n = 0;
  String request = "PING " + String(n);
  String expected = "PONG " + String(n);

  Serial.printf("TX: %s\n", request.c_str());
  sendLine(request);

  String reply;
  if (readLine(reply, 1500) && reply == expected) {
    Serial.printf("RX: %s (OK)\n", reply.c_str());
  } else if (reply.length()) {
    Serial.printf("RX: \"%s\" (unexpected)\n", reply.c_str());
  } else {
    Serial.println("RX timeout — check wiring, GND, pull-up, roles, and baud.");
  }

  drainLink();
  n++;
  delay(1000);
}

static void runResponder() {
  String request;
  if (!readLine(request, 200) || !request.startsWith("PING ")) {
    return;
  }

  Serial.printf("RX: %s\n", request.c_str());
  delay(50);  // let the initiator finish discarding its own echo

  String reply = "PONG " + request.substring(5);
  Serial.printf("TX: %s\n", reply.c_str());
  sendLine(reply);
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n=== One-Wire UART Two-Board Demo ===");
  Serial.printf("Role: %s\n", NODE_ROLE == ROLE_INITIATOR ? "INITIATOR" : "RESPONDER");
  Serial.printf("BUS_PIN: %d  baud: %lu\n", BUS_PIN, (unsigned long)LINK_BAUD);
  Serial.println("Wire: BUS_PIN<->BUS_PIN, GND<->GND, pull-up to 3.3 V.");

  Link.begin(LINK_BAUD, SERIAL_8N1, BUS_PIN, BUS_PIN);
  drainLink();

#if NODE_ROLE == ROLE_INITIATOR
  // Give the responder time to boot and sit idle on the bus.
  delay(1500);
#else
  Serial.println("Waiting for PING...");
#endif
}

void loop() {
#if NODE_ROLE == ROLE_INITIATOR
  runInitiator();
#else
  runResponder();
#endif
}
