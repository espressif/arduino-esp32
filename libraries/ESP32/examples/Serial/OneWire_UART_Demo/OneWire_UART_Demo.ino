/*
  One-Wire UART Demo

  Uses enableOneWireMode(true) so RX and TX share one GPIO (ESP-IDF one-wire / single-wire UART).

  Part A (all boards): UART1 self loopback on one GPIO — verifies one-wire TX/RX on the same pad.
  Part B (3+ UART boards): UART1 one-wire peer -> UART2 one-wire peer (half-duplex bus).

  Set USE_INTERNAL_LOOPBACK to 1 for GPIO-matrix routing (no jumper), or 0 and wire ONEWIRE_PIN1
  to ONEWIRE_PIN2 with an external pull-up on the shared line for bus-style use.
  Default ONEWIRE_PIN1 is board SDA (pins_arduino.h); ONEWIRE_PIN2 defaults to RX2.

  WARNING: Never drive TX on both UARTs at the same time. Use half-duplex turn-taking.

  Open Serial Monitor at 115200 baud on USB Serial.
*/

#include <Arduino.h>

// 1 = GPIO matrix connects UART1 TX to the peer RX pad (no external wire)
// 0 = jumper ONEWIRE_PIN1 <-> ONEWIRE_PIN2 (+ external pull-up recommended on the bus)
#define USE_INTERNAL_LOOPBACK 1

#ifndef ONEWIRE_PIN1
#define ONEWIRE_PIN1 SDA
#endif

#ifdef SOC_UART_HP_NUM
#if SOC_UART_HP_NUM >= 3
#ifdef RX2
#define HAS_UART2 1
#ifndef ONEWIRE_PIN2
#define ONEWIRE_PIN2 RX2
#endif
#else
#define HAS_UART2 0
#endif
#else
#define HAS_UART2 0
#endif
#else
#define HAS_UART2 0
#endif

static const uint32_t BAUD = 115200;
static const char *TEST_MSG = "ONEWIRE";

static bool g_dualPeerReady = false;

static void beginOneWire(HardwareSerial &uart, int8_t pin) {
  uart.end();
  uart.enableOneWireMode(true);
  uart.setPins(pin, pin);
  uart.begin(BAUD, SERIAL_8N1, pin, pin);
  while (!uart) {
    delay(10);
  }
}

static bool waitForBytes(HardwareSerial &uart, uint32_t timeoutMs) {
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    if (uart.available() > 0) {
      return true;
    }
    delay(1);
  }
  return false;
}

static String readLine(HardwareSerial &uart) {
  String line = uart.readStringUntil('\n');
  line.trim();
  return line;
}

static void drain(HardwareSerial &uart) {
  while (uart.available()) {
    uart.read();
  }
}

static void printLoopbackModeBanner() {
  Serial.printf("USE_INTERNAL_LOOPBACK=%d", USE_INTERNAL_LOOPBACK);
  if (USE_INTERNAL_LOOPBACK) {
    Serial.println(" — no external jumpers required (GPIO-matrix loopback).");
  } else {
    Serial.println(" — external jumper wiring required (see instructions below).");
  }
}

static void printPartAWiringInstructions() {
  if (USE_INTERNAL_LOOPBACK) {
    Serial.printf("  Part A: no wire — internal loopback on UART1 one-wire GPIO %d.\n", ONEWIRE_PIN1);
  } else {
    Serial.printf("  Part A: optional self-jumper on UART1 one-wire GPIO %d (same pad for TX and RX).\n", ONEWIRE_PIN1);
  }
}

#if HAS_UART2
static void printPartBWiringInstructions() {
  Serial.println();
  if (USE_INTERNAL_LOOPBACK) {
    Serial.println("  Part B: no wire — UART1 TX routed internally to UART2 one-wire pad.");
    Serial.printf("    UART1 one-wire GPIO %d  ~~internal~~>  UART2 one-wire GPIO %d\n", ONEWIRE_PIN1, ONEWIRE_PIN2);
  } else {
    Serial.println("  Part B wiring — half-duplex bus between UART one-wire pads:");
    Serial.printf("    UART1 GPIO %d  <----bus---->  UART2 GPIO %d\n", ONEWIRE_PIN1, ONEWIRE_PIN2);
    Serial.println("  External pull-up on the shared line recommended.");
    Serial.println("  Never transmit from both UARTs at the same time.");
  }
  Serial.println();
}
#endif

static bool runSelfLoopbackTest() {
  Serial.printf("\n=== Part A: UART1 one-wire self loopback (GPIO %d) ===\n", ONEWIRE_PIN1);
  beginOneWire(Serial1, ONEWIRE_PIN1);

#if USE_INTERNAL_LOOPBACK
  uart_internal_loopback(1, ONEWIRE_PIN1);
  Serial.printf("  Internal loopback: UART1 TX -> same GPIO %d (RX input)\n", ONEWIRE_PIN1);
#else
  Serial.printf("  External loopback: optional jumper on UART1 one-wire GPIO %d\n", ONEWIRE_PIN1);
#endif

  drain(Serial1);
  Serial1.println(TEST_MSG);
  Serial1.flush();
  delay(100);

  if (!waitForBytes(Serial1, 300)) {
    Serial.printf("  Self loopback failed on GPIO %d.", ONEWIRE_PIN1);
    if (USE_INTERNAL_LOOPBACK) {
      Serial.println(" Internal loopback enabled — verify one-wire on this GPIO.");
    } else {
      Serial.println(" Set USE_INTERNAL_LOOPBACK=1 or add optional self-jumper on this GPIO.");
    }
    return false;
  }

  String line = readLine(Serial1);
  Serial.printf("Self loopback received: \"%s\"\n", line.c_str());
  return line == TEST_MSG;
}

#if HAS_UART2
static bool runDualPeerLoopbackTest() {
  Serial.printf("\n=== Part B: UART1 -> UART2 one-wire peer (GPIO %d -> GPIO %d) ===\n", ONEWIRE_PIN1, ONEWIRE_PIN2);
  printPartBWiringInstructions();

  beginOneWire(Serial1, ONEWIRE_PIN1);
  beginOneWire(Serial2, ONEWIRE_PIN2);

#if USE_INTERNAL_LOOPBACK
  uart_internal_loopback(1, ONEWIRE_PIN2);
  Serial.printf("  Internal loopback: UART1 TX -> UART2 one-wire GPIO %d\n", ONEWIRE_PIN2);
#else
  Serial.printf("  External bus: jumper GPIO %d <-> GPIO %d (+ pull-up recommended)\n", ONEWIRE_PIN1, ONEWIRE_PIN2);
#endif

  drain(Serial2);
  Serial1.println(TEST_MSG);
  Serial1.flush();
  delay(150);

  if (!waitForBytes(Serial2, 300)) {
    Serial.printf("  Peer loopback failed — UART2 GPIO %d received nothing.", ONEWIRE_PIN2);
    if (USE_INTERNAL_LOOPBACK) {
      Serial.println(" Internal loopback enabled — verify board has 3+ UARTs and valid ONEWIRE_PIN2.");
    } else {
      Serial.printf(" Connect GPIO %d to GPIO %d, add bus pull-up, reset, or set USE_INTERNAL_LOOPBACK=1.\n", ONEWIRE_PIN1, ONEWIRE_PIN2);
    }
    g_dualPeerReady = false;
    return false;
  }

  String line = readLine(Serial2);
  Serial.printf("UART2 received: \"%s\"\n", line.c_str());
  g_dualPeerReady = (line == TEST_MSG);
  return g_dualPeerReady;
}
#endif

void setup() {
  Serial.begin(BAUD);
  delay(500);
  Serial.println("\n=== One-Wire UART Demo ===");
  Serial.printf("UART1 one-wire pin (ONEWIRE_PIN1): GPIO %d\n", ONEWIRE_PIN1);
#if HAS_UART2
  Serial.printf("UART2 one-wire pin (ONEWIRE_PIN2): GPIO %d\n", ONEWIRE_PIN2);
#else
  Serial.println("Part B skipped on this board (fewer than 3 HP UARTs or no RX2 define).");
#endif
  printLoopbackModeBanner();
  printPartAWiringInstructions();
#if HAS_UART2
  Serial.printf("Part B preview: UART1 GPIO %d -> UART2 GPIO %d (details before Part B runs).\n", ONEWIRE_PIN1, ONEWIRE_PIN2);
#endif
  Serial.println();

  if (runSelfLoopbackTest()) {
    Serial.println("Part A OK");
  }

#if HAS_UART2
  if (runDualPeerLoopbackTest()) {
    Serial.println("Part B OK");
    Serial.println("\nType in Serial Monitor — bytes go to UART1; replies print from UART2 (half-duplex peer).");
  } else {
    Serial.println("Part B failed — interactive peer mode disabled.");
  }
#else
  Serial.println("\nPart B skipped (board has fewer than 3 UARTs or no RX2 define).");
  Serial.println("Type in Serial Monitor — UART1 echoes on the same one-wire GPIO (Part A only).");
#endif
}

void loop() {
#if HAS_UART2
  if (g_dualPeerReady) {
    if (Serial.available()) {
      Serial1.write(Serial.read());
    }
    while (Serial2.available()) {
      Serial.write(Serial2.read());
    }
    return;
  }
#endif

  if (Serial.available()) {
    int c = Serial.read();
    Serial1.write(c);
    Serial1.flush();
  }
  if (Serial1.available()) {
    Serial.write(Serial1.read());
  }
}
