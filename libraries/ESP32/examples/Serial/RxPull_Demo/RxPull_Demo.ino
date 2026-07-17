/*
  RX Internal Pull Demo

  Demonstrates why enableRxInternalPull() matters on split RX/TX pins:
    A) Floating RX — pull defines idle when nothing drives the line
    B) Inverted RX — pull direction tracks begin(invert) / setRxInvert()

  Wiring: leave UART1 RX1 unconnected (no wire on the RX pin).

  Open Serial Monitor at 115200 baud.
*/

#include <Arduino.h>
#include "driver/gpio.h"

#ifndef RX1
#define RX1 4
#endif
#ifndef TX1
#define TX1 5
#endif

static const uint32_t BAUD = 115200;
static const uint32_t FLOAT_SAMPLE_MS = 500;

static volatile uint32_t g_rxErrors = 0;

static void onRxError(hardwareSerial_error_t err) {
  if (err != UART_NO_ERROR) {
    g_rxErrors++;
  }
}

static void printPullState(int8_t rxPin, const char *expect) {
  gpio_io_config_t cfg = {};
  const char *mode = "floating";
  if (gpio_get_io_config((gpio_num_t)rxPin, &cfg) == ESP_OK) {
    if (cfg.pu && !cfg.pd) {
      mode = "pull-up";
    } else if (cfg.pd && !cfg.pu) {
      mode = "pull-down";
    } else if (cfg.pu && cfg.pd) {
      mode = "pull-up+down";
    }
  }
  Serial.printf("  RX GPIO %d pull: %s (expect %s)\n", rxPin, mode, expect);
}

static void sampleFloatingIdle(bool pullEnabled, const char *label) {
  Serial1.end();
  Serial1.enableRxInternalPull(pullEnabled);
  Serial1.begin(BAUD, SERIAL_8N1, RX1, TX1);

  Serial.printf("\n--- %s ---\n", label);
  Serial.printf("  enableRxInternalPull(%s)\n", pullEnabled ? "true" : "false");
  printPullState(RX1, pullEnabled ? "pull-up" : "floating");

  g_rxErrors = 0;
  Serial1.onReceiveError(onRxError);
  delay(FLOAT_SAMPLE_MS);
  Serial1.onReceiveError(nullptr);

  Serial.printf("  %lu ms idle on unconnected RX: UART errors=%lu, available=%u\n", FLOAT_SAMPLE_MS, g_rxErrors,
                Serial1.available());
  if (!pullEnabled) {
    Serial.println("  Tip: floating RX may pick up noise; pull-up holds idle HIGH.");
  } else {
    Serial.println("  Pull-up holds logical idle HIGH so the receiver sees a defined line level.");
  }

  Serial1.end();
}

static void runFloatingRxDemo() {
  Serial.println("\n=== Part A: Floating RX (leave RX1 unconnected) ===");
  Serial.printf("  No jumper on UART1 RX1 (GPIO %d).\n", RX1);
  sampleFloatingIdle(false, "Pull disabled — RX pad floats");
  sampleFloatingIdle(true, "Default pull enabled — RX idle defined");
}

static void runInversionPullDemo() {
  Serial.println("\n=== Part B: Pull follows RX inversion ===");

  Serial1.end();
  Serial1.enableRxInternalPull(true);
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

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== RX Internal Pull Demo ===");
  Serial.printf("UART1 pins: RX=%d TX=%d\n", RX1, TX1);
  Serial.println("Leave RX1 unconnected for the whole demo.");

  runFloatingRxDemo();
  runInversionPullDemo();

  Serial.println("\nDemo complete.");
}

void loop() {}
