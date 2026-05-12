/*
  IrdaMode_Demo.ino

  Demonstrates UART IrDA direction switching with HardwareSerial::setIrdaMode().

  This example is intended for two nodes connected through SIR IrDA transceivers
  (one node running this sketch, and a peer that can receive and transmit replies).

  Functions used:
  - setMode(UART_MODE_IRDA)
  - setIrdaMode(ESP32_UART_IRDA_TX / ESP32_UART_IRDA_RX)

  Hardware notes:
  - Use SIR IrDA UART transceivers (not 38 kHz remote-control receivers).
  - Connect ESP32 UART TX/RX to the transceiver UART side, and align the IR
    optical side between the two nodes.
*/

#include <Arduino.h>

#define IRDA_RX_PIN  RX1  // RX UART1 pin
#define IRDA_TX_PIN  TX1  // TX UART1 pin
#define BAUD_RATE    9600
#define RX_TIMEOUT   300

HardwareSerial &irda = Serial1;

void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println("\n=== UART IrDA direction demo ===");
  Serial.println("Expected setup: two nodes with SIR IrDA transceivers");

  irda.begin(BAUD_RATE, SERIAL_8N1, IRDA_RX_PIN, IRDA_TX_PIN);

  if (!irda.setMode(UART_MODE_IRDA)) {
    Serial.println("ERROR: setMode(UART_MODE_IRDA) failed");
    while (1) {
      delay(100);
    }
  }

  Serial.println("IrDA mode enabled. Loop will:");
  Serial.println("1) Switch to IrDA TX mode (using TX GPIO " + String(IRDA_TX_PIN) + ") and send a frame");
  Serial.println("2) Switch to IrDA RX mode (using RX GPIO " + String(IRDA_RX_PIN) + ") and wait for peer response");
}

void loop() {
  static uint32_t counter = 0;

  if (!irda.setIrdaMode(ESP32_UART_IRDA_TX)) {
    Serial.println("ERROR: failed to switch to IrDA TX mode");
    delay(1000);
    return;
  }

  Serial.printf("TX -> PING %lu\n", (unsigned long)counter);
  irda.printf("PING %lu\n", (unsigned long)counter);
  irda.flush();

  if (!irda.setIrdaMode(ESP32_UART_IRDA_RX)) {
    Serial.println("ERROR: failed to switch to IrDA RX mode");
    delay(1000);
    return;
  }

  uint32_t start = millis();
  bool gotReply = false;

  while ((millis() - start) < RX_TIMEOUT) {
    bool readAny = false;
    while (irda.available()) {
      char c = (char)irda.read();
      Serial.print(c);
      gotReply = true;
      readAny = true;
    }
    if (!readAny) {
      delay(1);
    }
  }

  if (!gotReply) {
    Serial.println("RX <- (no response in RX window)");
  } else {
    Serial.println();
  }

  counter++;
  delay(500);
}
