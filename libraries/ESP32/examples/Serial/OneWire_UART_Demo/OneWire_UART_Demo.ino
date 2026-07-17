/*
  One-Wire UART Demo

  Same-pin RX/TX automatically enables one-wire UART (no opt-in API).
  Use setPins(p, p) or begin(..., p, p); -1 keep-current that makes RX==TX also enables it.

  Part A (all boards): UART1 self-echo on one GPIO — verifies one-wire TX/RX on the same pad.
  Part B (3+ UART boards): UART1 <-> UART2 half-duplex cross test over one signal wire.

  Set USE_INTERNAL_LOOPBACK to 1 for GPIO-matrix routing (no jumper), or 0 and wire ONEWIRE_PIN1
  to ONEWIRE_PIN2 with a recommended external pull-up (4.7–10 kΩ to 3.3 V). External Part B
  uses split RX/TX assignments and keeps only one UART TX on the shared wire. It is a
  single-wire half-duplex link; Part A is the direct one-wire (RX == TX) demonstration.
  Default ONEWIRE_PIN1 is board SDA (pins_arduino.h); ONEWIRE_PIN2 defaults to RX2.

  WARNING: Never drive TX on both UARTs at the same time. Use half-duplex turn-taking.

  Open Serial Monitor at 115200 baud on USB Serial.
*/

#include <Arduino.h>
#include "esp_rom_gpio.h"
#include "soc/uart_periph.h"

// 1 = GPIO matrix connects UART1 TX to the peer RX pad (no external wire)
// 0 = jumper ONEWIRE_PIN1 <-> ONEWIRE_PIN2 (+ external pull-up recommended on the bus)
#define USE_INTERNAL_LOOPBACK 1

#ifndef ONEWIRE_PIN1
#define ONEWIRE_PIN1 SDA // using a valid pin for the board
#endif

#ifdef SOC_UART_HP_NUM
#if SOC_UART_HP_NUM >= 3
#if defined(RX2) && defined(TX1) && defined(TX2)
#define HAS_UART2 1
#ifndef ONEWIRE_PIN2
#define ONEWIRE_PIN2 RX2
#endif
#ifndef ONEWIRE_PARK_PIN1
#define ONEWIRE_PARK_PIN1 TX1
#endif
#ifndef ONEWIRE_PARK_PIN2
#define ONEWIRE_PARK_PIN2 TX2
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

#if HAS_UART2
static const char *PEER_REQUEST = "UART1_TO_UART2";
static const char *PEER_REPLY = "UART2_TO_UART1";
static bool g_dualPeerReady = false;
// External interactive mode keeps the UART2 -> UART1 roles verified by Turn 2.
static bool g_interactiveUart2Drives = false;
#endif

static void beginOneWire(HardwareSerial &uart, int8_t pin) {
  uart.end();
  uart.setPins(pin, pin);  // same-pin auto-enables one-wire
  uart.begin(BAUD, SERIAL_8N1, pin, pin);
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

#if HAS_UART2 && USE_INTERNAL_LOOPBACK
// uart_internal_loopback() routes src UART TX onto dstPad but does not clear a previous
// TX drive left on that pad. Restore the pad owner's TX before reversing direction so
// both pads carry the same UART TX signal during each turn. An optional jumper then
// connects identical signals instead of shorting UART1 TX against UART2 TX.
static void restoreUartTxToOwnPin(uint8_t uartNum, int8_t pin) {
  esp_rom_gpio_connect_out_signal(pin, uart_periph_signal[uartNum].pins[SOC_UART_TX_PIN_IDX].signal, false, false);
}
#endif

#if HAS_UART2 && !USE_INTERNAL_LOOPBACK
// External Part B uses split RX/TX assignments so only the active endpoint's TX is
// connected to the bus. The inactive RX or TX is parked on an otherwise unused GPIO.
static bool beginSplitUart(HardwareSerial &uart, int8_t rxPin, int8_t txPin) {
  uart.setPins(rxPin, txPin);
  uart.begin(BAUD, SERIAL_8N1, rxPin, txPin);
  if (!uart) {
    Serial.printf("  Failed to start UART with RX GPIO %d and TX GPIO %d.\n", rxPin, txPin);
    return false;
  }
  return true;
}

static bool setExternalBusRoles(
  HardwareSerial &driver, int8_t driverBusTxPin, int8_t driverOffBusRxPin, HardwareSerial &listener, int8_t listenerBusRxPin,
  int8_t listenerOffBusTxPin
) {
  driver.end();
  listener.end();
  delay(20);
  if (!beginSplitUart(driver, driverOffBusRxPin, driverBusTxPin)) {
    return false;
  }
  if (!beginSplitUart(listener, listenerBusRxPin, listenerOffBusTxPin)) {
    return false;
  }
  delay(20);
  drain(driver);
  drain(listener);
  return true;
}
#endif

static void printLoopbackModeBanner() {
  Serial.printf("USE_INTERNAL_LOOPBACK=%d", USE_INTERNAL_LOOPBACK);
  if (USE_INTERNAL_LOOPBACK) {
    Serial.println(" — no external jumpers required (GPIO-matrix loopback).");
  } else {
    Serial.println(" — external signal-wire connection required for Part B.");
  }
}

static void printPartAWiringInstructions() {
  if (USE_INTERNAL_LOOPBACK) {
    Serial.printf("  Part A: no wire — internal loopback on UART1 one-wire GPIO %d.\n", ONEWIRE_PIN1);
  } else {
    Serial.printf("  Part A: no jumper — UART1 TX and RX share GPIO %d; an external pull-up is optional.\n", ONEWIRE_PIN1);
  }
}

#if HAS_UART2
static void printPartBWiringInstructions() {
  Serial.println();
  if (USE_INTERNAL_LOOPBACK) {
    Serial.println("  Part B: no external wire required; an existing jumper is optional and redundant.");
    Serial.println("  GPIO-matrix routing switches direction between each half-duplex turn.");
    Serial.println("  Both GPIOs carry the same active UART TX signal during each turn.");
    Serial.printf("    UART1 GPIO %d  <~~internal~~>  UART2 GPIO %d\n", ONEWIRE_PIN1, ONEWIRE_PIN2);
  } else {
    Serial.println("  Part B wiring — one signal wire between both UART bus pads:");
    Serial.printf("    UART1 GPIO %d  <----bus---->  UART2 GPIO %d\n", ONEWIRE_PIN1, ONEWIRE_PIN2);
    Serial.printf("  Leave parking GPIOs %d and %d unconnected.\n", ONEWIRE_PARK_PIN1, ONEWIRE_PARK_PIN2);
    Serial.println("  External 4.7–10 kΩ pull-up to 3.3 V is recommended.");
    Serial.println("  Never transmit from both UARTs at the same time.");
  }
  Serial.println();
}
#endif

static bool runSelfLoopbackTest() {
  Serial.printf("\n=== Part A: UART1 one-wire self-echo (GPIO %d) ===\n", ONEWIRE_PIN1);
  beginOneWire(Serial1, ONEWIRE_PIN1);

#if USE_INTERNAL_LOOPBACK
  uart_internal_loopback(1, ONEWIRE_PIN1);
  Serial.printf("  Internal loopback: UART1 TX -> same GPIO %d (RX input)\n", ONEWIRE_PIN1);
#else
  Serial.printf("  Same-pad self-echo on UART1 one-wire GPIO %d (no jumper needed)\n", ONEWIRE_PIN1);
#endif

  drain(Serial1);
  Serial1.println(TEST_MSG);
  Serial1.flush();
  delay(100);

  if (!waitForBytes(Serial1, 300)) {
    Serial.printf("  Self-echo failed on GPIO %d.", ONEWIRE_PIN1);
    if (USE_INTERNAL_LOOPBACK) {
      Serial.println(" Internal loopback enabled — verify one-wire on this GPIO.");
    } else {
      Serial.println(" Verify that this GPIO supports UART matrix routing; try the recommended external pull-up.");
    }
    return false;
  }

  String line = readLine(Serial1);
  Serial.printf("Self-echo received: \"%s\"\n", line.c_str());
  return line == TEST_MSG;
}

#if HAS_UART2
static bool runDualPeerCrossTest() {
  Serial.printf("\n=== Part B: UART1 <-> UART2 single-wire cross test (GPIO %d <-> GPIO %d) ===\n", ONEWIRE_PIN1, ONEWIRE_PIN2);
  printPartBWiringInstructions();

  if (ONEWIRE_PIN1 == ONEWIRE_PIN2) {
    Serial.println("  Cross test requires two different GPIOs connected by one wire; change ONEWIRE_PIN1 or ONEWIRE_PIN2.");
    g_dualPeerReady = false;
    return false;
  }
#if !USE_INTERNAL_LOOPBACK
  if (ONEWIRE_PARK_PIN1 == ONEWIRE_PIN1 || ONEWIRE_PARK_PIN1 == ONEWIRE_PIN2 || ONEWIRE_PARK_PIN2 == ONEWIRE_PIN1
      || ONEWIRE_PARK_PIN2 == ONEWIRE_PIN2 || ONEWIRE_PARK_PIN1 == ONEWIRE_PARK_PIN2) {
    Serial.println("  Cross test requires four distinct bus/parking GPIOs; override ONEWIRE_PIN1, ONEWIRE_PIN2,");
    Serial.println("  ONEWIRE_PARK_PIN1, or ONEWIRE_PARK_PIN2 for this board.");
    g_dualPeerReady = false;
    return false;
  }
#endif

#if USE_INTERNAL_LOOPBACK
  beginOneWire(Serial1, ONEWIRE_PIN1);
  beginOneWire(Serial2, ONEWIRE_PIN2);
  // GPIO ONEWIRE_PIN1 already carries UART1 TX; route that same signal onto
  // ONEWIRE_PIN2. With an optional jumper, both ends therefore have identical levels.
  uart_internal_loopback(1, ONEWIRE_PIN2);
  Serial.printf("  Internal loopback: UART1 TX -> UART2 one-wire GPIO %d\n", ONEWIRE_PIN2);
#else
  Serial.printf("  External bus: jumper GPIO %d <-> GPIO %d (+ pull-up recommended)\n", ONEWIRE_PIN1, ONEWIRE_PIN2);
  Serial.printf(
    "  Turn 1 roles: UART1 TX on GPIO %d (RX parked on GPIO %d); UART2 RX on GPIO %d (TX parked on GPIO %d).\n", ONEWIRE_PIN1,
    ONEWIRE_PARK_PIN1, ONEWIRE_PIN2, ONEWIRE_PARK_PIN2
  );
  if (!setExternalBusRoles(Serial1, ONEWIRE_PIN1, ONEWIRE_PARK_PIN1, Serial2, ONEWIRE_PIN2, ONEWIRE_PARK_PIN2)) {
    g_dualPeerReady = false;
    return false;
  }
#endif

  delay(20);
  drain(Serial1);
  drain(Serial2);
  Serial.printf("  Turn 1: UART1 sends \"%s\" to UART2\n", PEER_REQUEST);
  Serial1.println(PEER_REQUEST);
  Serial1.flush();
  delay(150);

  if (!waitForBytes(Serial2, 300)) {
    Serial.printf("  Cross test failed — UART2 GPIO %d received nothing.", ONEWIRE_PIN2);
    if (USE_INTERNAL_LOOPBACK) {
      Serial.println(" Internal loopback enabled — verify board has 3+ UARTs and valid ONEWIRE_PIN2.");
    } else {
      Serial.printf(" Check wire GPIO %d <-> %d and pull-up to 3.3 V.\n", ONEWIRE_PIN1, ONEWIRE_PIN2);
    }
    g_dualPeerReady = false;
    return false;
  }

  String line = readLine(Serial2);
  Serial.printf("UART2 received: \"%s\"\n", line.c_str());
  if (line != PEER_REQUEST) {
    Serial.println("  Cross test failed — UART2 request did not match.");
    g_dualPeerReady = false;
    return false;
  }

  // Clear both receivers before reversing direction on the half-duplex bus.
  drain(Serial1);
  drain(Serial2);

#if USE_INTERNAL_LOOPBACK
  // Turn 1 left UART1 TX driving ONEWIRE_PIN2. Put UART2 TX back on its own pad first;
  // then route UART2 TX onto ONEWIRE_PIN1. Both changes happen after UART1.flush(),
  // while the line is idle HIGH. During Turn 2 both pads carry UART2 TX, so an
  // optional jumper connects identical signals instead of causing output contention.
  restoreUartTxToOwnPin(2, ONEWIRE_PIN2);
  uart_internal_loopback(2, ONEWIRE_PIN1);
  Serial.printf("  Internal loopback reversed: UART2 TX -> UART1 one-wire GPIO %d\n", ONEWIRE_PIN1);
#else
  Serial.printf(
    "  Turn 2 roles: UART2 TX on GPIO %d (RX parked on GPIO %d); UART1 RX on GPIO %d (TX parked on GPIO %d).\n", ONEWIRE_PIN2,
    ONEWIRE_PARK_PIN2, ONEWIRE_PIN1, ONEWIRE_PARK_PIN1
  );
  if (!setExternalBusRoles(Serial2, ONEWIRE_PIN2, ONEWIRE_PARK_PIN2, Serial1, ONEWIRE_PIN1, ONEWIRE_PARK_PIN1)) {
    g_dualPeerReady = false;
    return false;
  }
#endif

  Serial.printf("  Turn 2: UART2 sends \"%s\" to UART1\n", PEER_REPLY);
  Serial2.println(PEER_REPLY);
  Serial2.flush();
  delay(150);

  if (!waitForBytes(Serial1, 300)) {
    Serial.printf("  Cross test failed — UART1 GPIO %d received no reply.\n", ONEWIRE_PIN1);
#if USE_INTERNAL_LOOPBACK
    restoreUartTxToOwnPin(1, ONEWIRE_PIN1);
    uart_internal_loopback(1, ONEWIRE_PIN2);
#endif
    g_dualPeerReady = false;
    return false;
  }

  line = readLine(Serial1);
  Serial.printf("UART1 received reply: \"%s\"\n", line.c_str());

#if USE_INTERNAL_LOOPBACK
  // Restore UART1 TX on both pads for interactive UART1 -> UART2 forwarding.
  restoreUartTxToOwnPin(1, ONEWIRE_PIN1);
  uart_internal_loopback(1, ONEWIRE_PIN2);
  g_interactiveUart2Drives = false;
#else
  // Keep Turn 2 roles for interactive mode. A second swap back to UART1-TX/UART2-RX
  // is unnecessary because Turn 2 already proved UART2 -> UART1.
  g_interactiveUart2Drives = true;
  Serial.printf(
    "  Interactive roles (unchanged from Turn 2): UART2 TX on GPIO %d; UART1 RX on GPIO %d.\n", ONEWIRE_PIN2, ONEWIRE_PIN1
  );
#endif

  // Remove any leftover RX data before entering interactive mode.
  drain(Serial1);
  drain(Serial2);

  g_dualPeerReady = (line == PEER_REPLY);
  if (!g_dualPeerReady) {
    Serial.println("  Cross test failed — UART1 reply did not match.");
  }
  return g_dualPeerReady;
}
#endif

void setup() {
  Serial.begin(BAUD);
  delay(500);
  Serial.println("\n=== One-Wire UART Demo ===");
  Serial.printf("UART1 one-wire/bus pin (ONEWIRE_PIN1): GPIO %d\n", ONEWIRE_PIN1);
#if HAS_UART2
  Serial.printf("UART2 peer bus pin (ONEWIRE_PIN2): GPIO %d\n", ONEWIRE_PIN2);
#else
  Serial.println("Part B skipped on this board (fewer than 3 HP UARTs or no RX2 define).");
#endif
  printLoopbackModeBanner();
  printPartAWiringInstructions();
#if HAS_UART2
#if USE_INTERNAL_LOOPBACK
  Serial.printf(
    "Part B preview: no jumper required (optional/redundant); internal matrix routes UART1 GPIO %d <-> UART2 GPIO %d.\n", ONEWIRE_PIN1,
    ONEWIRE_PIN2
  );
#else
  Serial.printf("Part B preview: connect one wire between UART1 GPIO %d and UART2 GPIO %d.\n", ONEWIRE_PIN1, ONEWIRE_PIN2);
#endif
#endif
  Serial.println();

  if (runSelfLoopbackTest()) {
    Serial.println("Part A OK");
  } else {
    Serial.println("Part A failed");
  }

#if HAS_UART2
  if (runDualPeerCrossTest()) {
    Serial.println("Part B OK");
    Serial.println("\nBidirectional single-wire cross test passed.");
    if (g_interactiveUart2Drives) {
      Serial.println("Type in Serial Monitor and press Send — bytes go UART2 -> bus -> UART1 and print here.");
    } else {
      Serial.println("Type in Serial Monitor and press Send — bytes go UART1 -> bus -> UART2 and print here.");
    }
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
    HardwareSerial &driveUart = g_interactiveUart2Drives ? Serial2 : Serial1;
    HardwareSerial &listenUart = g_interactiveUart2Drives ? Serial1 : Serial2;
    bool sent = false;
    while (Serial.available()) {
      driveUart.write((uint8_t)Serial.read());
      sent = true;
    }
    if (sent) {
      driveUart.flush();
    }
#if USE_INTERNAL_LOOPBACK
    // One-wire / matrix path also receives its own TX; discard so the RX buffer cannot fill.
    while (driveUart.available()) {
      driveUart.read();
    }
#endif
    while (listenUart.available()) {
      Serial.write(listenUart.read());
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
