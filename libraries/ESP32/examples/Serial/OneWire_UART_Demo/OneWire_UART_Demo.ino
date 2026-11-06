/*
  One-Wire UART Demo (single board)

  Shows how one UART can transmit and receive on a single GPIO (half-duplex,
  one-wire mode). Same-pin RX/TX automatically enables one-wire UART:
  begin(baud, config, pin, pin) makes RX == TX and turns the pad into an
  open-drain one-wire bus.

  This example runs on a single board with no external wiring: it uses the
  UART internal loopback so UART1 receives its own transmission on the same
  GPIO. On a real one-wire bus, an external pull-up resistor to 3.3 V is
  required instead of the internal loopback.

  Open the Serial Monitor at 115200 baud. After the self-test, anything you
  type is sent on the one-wire GPIO and echoed back.
*/

#include <Arduino.h>
#include "driver/gpio.h"

#ifndef ONEWIRE_PIN
#define ONEWIRE_PIN SDA  // any valid GPIO for the board
#endif

static const uint32_t BAUD = 115200;
static const char *TEST_MSG = "ONEWIRE";

static void beginOneWire(HardwareSerial &uart, int8_t pin) {
  uart.end();
  uart.begin(BAUD, SERIAL_8N1, pin, pin);  // same pin for RX and TX -> one-wire
}

static void drain(HardwareSerial &uart) {
  while (uart.available()) {
    uart.read();
  }
}

static bool runSelfEchoTest() {
  Serial.printf("\n=== One-wire self-echo on GPIO %d ===\n", ONEWIRE_PIN);

  beginOneWire(Serial1, ONEWIRE_PIN);

  // No external wire needed: route UART1 TX back to its own RX input on the
  // same GPIO. On real hardware, use an external pull-up on the bus instead.
  uart_internal_loopback(1, ONEWIRE_PIN);

  // The one-wire pad is open-drain, so released HIGH bits float. This weak
  // internal pull-up lets the no-wire loopback observe them; it is only a test
  // fixture and does not replace the external pull-up needed on a real bus.
  gpio_pullup_en((gpio_num_t)ONEWIRE_PIN);

  drain(Serial1);
  Serial1.println(TEST_MSG);
  Serial1.flush();
  delay(50);

  String line = Serial1.readStringUntil('\n');
  line.trim();

  if (line == TEST_MSG) {
    Serial.printf("Received back: \"%s\" -> one-wire OK\n", line.c_str());
    return true;
  }

  Serial.printf("Self-echo failed (got \"%s\").\n", line.c_str());
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== One-Wire UART Demo ===");
  Serial.printf("One-wire GPIO: %d\n", ONEWIRE_PIN);

  if (runSelfEchoTest()) {
    Serial.println("\nType in the Serial Monitor: bytes are sent on the one-wire GPIO and echoed back.");
  } else {
    Serial.println("\nSelf-echo failed. Check that ONEWIRE_PIN is a valid GPIO.");
  }
}

void loop() {
  // Forward Serial Monitor input onto the one-wire UART.
  if (Serial.available()) {
    Serial1.write((uint8_t)Serial.read());
    Serial1.flush();
  }
  // Print whatever comes back on the one-wire GPIO (its own echo).
  if (Serial1.available()) {
    Serial.write(Serial1.read());
  }
}
