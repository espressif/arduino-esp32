/*
  RX Internal Pull Demo

  Demonstrates why enableRxInternalPull() matters on split RX/TX pins:
    A) Floating RX — pull defines idle when nothing drives the line
    B) Inverted RX — pull direction tracks begin(invert) / setRxInvert()
    C) Wired loopback (optional, 3+ UART boards) — partner TX usually holds idle;
       internal pull is often redundant but still verified here

  Wiring:
    Part A & B: leave UART1 RX1 unconnected (no wire on RX pin)
    Part C (optional): jumper UART1 TX1 -> UART2 RX2

  Open Serial Monitor at 115200 baud.
*/

#include <Arduino.h>
#include "driver/gpio.h"
#include "hal/gpio_types.h"

#ifndef RX1
#define RX1 4
#endif
#ifndef TX1
#define TX1 5
#endif

#ifdef SOC_UART_HP_NUM
#if SOC_UART_HP_NUM >= 3
#ifdef RX2
#define HAS_UART2 1
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
static const uint32_t FLOAT_SAMPLE_MS = 500;
static const char *TEST_MSG = "RxPull loopback";

static volatile uint32_t g_rxErrors = 0;

static const char *pullModeName(gpio_pull_mode_t mode) {
  switch (mode) {
    case GPIO_PULLUP_ONLY:   return "pull-up";
    case GPIO_PULLDOWN_ONLY: return "pull-down";
    case GPIO_FLOATING:      return "floating";
    default:                 return "other";
  }
}

static void onRxError(hardwareSerial_error_t err) {
  if (err != UART_NO_ERROR) {
    g_rxErrors++;
  }
}

static gpio_pull_mode_t readRxPullMode(int8_t rxPin) {
  gpio_io_config_t cfg = {};
  if (gpio_get_io_config((gpio_num_t)rxPin, &cfg) != ESP_OK) {
    return GPIO_FLOATING;
  }
  if (cfg.pu && cfg.pd) {
    return GPIO_PULLUP_PULLDOWN;
  }
  if (cfg.pu) {
    return GPIO_PULLUP_ONLY;
  }
  if (cfg.pd) {
    return GPIO_PULLDOWN_ONLY;
  }
  return GPIO_FLOATING;
}

static void printPullState(int8_t rxPin, const char *expect) {
  gpio_pull_mode_t mode = readRxPullMode(rxPin);
  Serial.printf("  RX GPIO %d pull: %s", rxPin, pullModeName(mode));
  if (expect != nullptr) {
    Serial.printf(" (expect %s)", expect);
  }
  Serial.println();
}

#if HAS_UART2
static void printPartCWiringInstructions() {
  Serial.println();
  Serial.println("  Part C wiring — connect with a jumper wire:");
  Serial.printf("    UART1 TX1  (GPIO %d)  ------>  UART2 RX2  (GPIO %d)\n", TX1, RX2);
  Serial.printf("  Leave UART1 RX1 (GPIO %d) unconnected for Parts A and B.\n", RX1);
  Serial.println("  Reset or re-upload after connecting the wire, then read Part C output below.");
  Serial.println();
}
#endif

static void sampleFloatingIdle(HardwareSerial &uart, int8_t rxPin, int8_t txPin, bool pullEnabled, const char *label) {
  uart.end();
  uart.enableRxInternalPull(pullEnabled);
  uart.setPins(rxPin, txPin);
  uart.begin(BAUD, SERIAL_8N1, rxPin, txPin);

  Serial.printf("\n--- %s ---\n", label);
  Serial.printf("  enableRxInternalPull(%s)\n", pullEnabled ? "true" : "false");
  printPullState(rxPin, pullEnabled ? "pull-up" : "floating");

  g_rxErrors = 0;
  uart.onReceiveError(onRxError);
  delay(FLOAT_SAMPLE_MS);
  uart.onReceiveError(nullptr);

  Serial.printf("  %lu ms idle on unconnected RX: UART errors=%lu, available=%u\n", FLOAT_SAMPLE_MS, g_rxErrors, uart.available());
  if (!pullEnabled) {
    Serial.println("  Tip: floating RX may pick up noise; scope or onReceiveError helps show undefined idle.");
  } else {
    Serial.println("  Pull-up holds logical idle HIGH so the receiver sees a defined line level.");
  }

  uart.end();
}

static void runFloatingRxDemo() {
  Serial.println("\n=== Part A: Floating RX (leave RX1 unconnected) ===");
  Serial.printf("  No jumper on UART1 RX1 (GPIO %d).\n", RX1);
  sampleFloatingIdle(Serial1, RX1, TX1, false, "Pull disabled — RX pad floats");
  sampleFloatingIdle(Serial1, RX1, TX1, true, "Default pull enabled — RX idle defined");
}

static void runInversionPullDemo() {
  Serial.println("\n=== Part B: Pull follows RX inversion ===");

  Serial1.end();
  Serial1.enableRxInternalPull(true);
  Serial1.setPins(RX1, TX1);
  Serial1.begin(BAUD, SERIAL_8N1, RX1, TX1);
  Serial.println("\n--- Default begin() ---");
  printPullState(RX1, "pull-up");
  Serial1.end();

  Serial1.begin(BAUD, SERIAL_8N1, RX1, TX1, true);
  Serial.println("\n--- begin(..., invert=true) ---");
  printPullState(RX1, "pull-down");
  Serial1.end();

  Serial1.begin(BAUD, SERIAL_8N1, RX1, TX1);
  Serial1.setRxInvert(true);
  Serial.println("\n--- setRxInvert(true) after begin() ---");
  printPullState(RX1, "pull-down");
  Serial1.setRxInvert(false);
  Serial.println("\n--- setRxInvert(false) ---");
  printPullState(RX1, "pull-up");
  Serial1.end();

  Serial1.begin(BAUD, SERIAL_8N1, RX1, TX1);
  Serial1.setTxInvert(true);
  Serial.println("\n--- setTxInvert(true) only (RX pull unchanged) ---");
  printPullState(RX1, "pull-up");
  Serial1.end();

  Serial.println("\nInverted UART (RS-232 style PHY) needs pull-down on RX to match electrical idle LOW.");
}

#if HAS_UART2
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

static void runWiredLoopbackDemo(bool pullEnabled) {
  Serial1.end();
  Serial2.end();

  Serial1.setPins(RX1, TX1);
  Serial2.enableRxInternalPull(pullEnabled);
  Serial2.setPins(RX2, TX2);
  Serial1.begin(BAUD, SERIAL_8N1, RX1, TX1);
  Serial2.begin(BAUD, SERIAL_8N1, RX2, TX2);

  Serial.printf("\n--- Loopback with enableRxInternalPull(%s) on UART2 RX ---\n", pullEnabled ? "true" : "false");
  printPullState(RX2, pullEnabled ? "pull-up" : "floating");

  while (Serial2.available()) {
    Serial2.read();
  }

  Serial1.println(TEST_MSG);
  Serial1.flush();

  if (waitForBytes(Serial2, 300)) {
    String line = Serial2.readStringUntil('\n');
    line.trim();
    Serial.printf("  Received on UART2: \"%s\"\n", line.c_str());
    Serial.println("  OK — partner TX drives the line during idle and while sending.");
  } else {
    Serial.printf("  No data received — connect GPIO %d (UART1 TX1) to GPIO %d (UART2 RX2) and reset.\n", TX1, RX2);
  }

  Serial1.end();
  Serial2.end();
}

static void runWiredLoopbackSection() {
  Serial.println("\n=== Part C: Wired loopback ===");
  printPartCWiringInstructions();
  Serial.println("When UART1 TX is wired to UART2 RX, the transmitter output usually holds idle HIGH.");
  Serial.println("Internal pull on UART2 RX is often redundant here; both cases below should receive data.");

  runWiredLoopbackDemo(true);
  runWiredLoopbackDemo(false);
}
#endif

void setup() {
  Serial.begin(BAUD);
  delay(500);
  Serial.println("\n=== RX Internal Pull Demo ===");
  Serial.printf("UART1 pins: RX=%d TX=%d\n", RX1, TX1);
#if HAS_UART2
  Serial.printf("Part C (after A & B): jumper UART1 TX1 GPIO %d -> UART2 RX2 GPIO %d\n", TX1, RX2);
#endif

  runFloatingRxDemo();
  runInversionPullDemo();

#if HAS_UART2
  Serial.printf("Board has UART2 — RX2=%d TX2=%d\n", RX2, TX2);
  runWiredLoopbackSection();
#else
  Serial.println("\n=== Part C: skipped (board has fewer than 3 UARTs) ===");
  Serial.println("Wired loopback needs a second UART: jumper peer TX (GPIO) -> peer RX (GPIO), split pins.");
#endif

  Serial.println("\nDemo complete.");
}

void loop() {
}
