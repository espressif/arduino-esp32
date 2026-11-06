/*
  IrdaMode_TwoBoard_Demo.ino

  Demonstrates UART IrDA communication between two ESP32 boards.
  This example is for SoCs with only 2 UARTs (UART0 + UART1).

  Setup Required:
  - 2 ESP32 boards connected via infrared (IR LED on one, IR receiver on other)
  - Serial Monitor for mode selection
  - User selects TX or RX mode at startup

  UART Configuration:
  - UART0 (Serial): Used for Serial Monitor communication and mode selection
  - UART1 (Serial1): Configured for IrDA in user-selected mode (TX or RX)

  Features:
  - Simple mode selection via Serial Monitor
  - One board transmits (TX mode), other receives (RX mode)
  - Demonstrates bidirectional IrDA communication concept
  - Real infrared hardware required (IR LED for TX, IR receiver for RX)

  Hardware Requirements:
  - Board 1: IR LED (950nm) + resistor (100-330Ω) connected to TX pin
  - Board 2: IR photodiode + amplifier circuit connected to RX pin
  - Align IR optical components for line-of-sight communication
*/

#include <Arduino.h>

// UART1 pin definitions
#define UART1_RX_PIN RX1  // RX UART1 pin
#define UART1_TX_PIN TX1  // TX UART1 pin
#define BAUD_RATE    9600

// IrDA mode selection
#define MODE_NOT_SET 0
#define MODE_TX      1
#define MODE_RX      2

HardwareSerial &irda_uart = Serial1;
uint8_t irda_mode = MODE_NOT_SET;

String received = "";              // Buffer for received message
const uint16_t MAX_MSG_SIZE = 64;  // Limit message size

void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println("\n=== UART IrDA Two-Board Demo ===");
  Serial.println("This example requires 2 ESP32 boards connected via infrared\n");

  selectMode();
}

void selectMode() {
  Serial.println("Select IrDA mode:");
  Serial.println("  T - Transmit mode (this board sends IrDA data)");
  Serial.println("  R - Receive mode (this board receives IrDA data)");
  Serial.println("\nWait for mode selection from Serial Monitor...");

  // Wait for user input from Serial Monitor
  while (irda_mode == MODE_NOT_SET) {
    if (Serial.available()) {
      char cmd = Serial.read();
      if (cmd == 'T' || cmd == 't') {
        irda_mode = MODE_TX;
        Serial.println("\n✓ Mode selected: TRANSMIT (TX)");
        setupTXMode();
        break;
      } else if (cmd == 'R' || cmd == 'r') {
        irda_mode = MODE_RX;
        received.reserve(MAX_MSG_SIZE);  // Pre-allocate for efficiency
        Serial.println("\n✓ Mode selected: RECEIVE (RX)");
        setupRXMode();
        break;
      }
    }
    delay(10);
  }
}

void setupTXMode() {
  // Initialize UART1 for IrDA TX
  irda_uart.begin(BAUD_RATE, SERIAL_8N1, UART1_RX_PIN, UART1_TX_PIN);
  if (!irda_uart.setMode(UART_MODE_IRDA)) {
    Serial.println("ERROR: Failed to set IRDA mode");
    while (1) {
      delay(100);
    }
  }
  if (!irda_uart.setIrdaDirection(ESP32_UART_IRDA_TX)) {
    Serial.println("ERROR: Failed to set IRDA TX mode");
    while (1) {
      delay(100);
    }
  }
  Serial.println("UART1 configured in IRDA TX mode");
  Serial.println("Transmitting IrDA frames...\n");
}

void setupRXMode() {
  // Initialize UART1 for IrDA RX
  irda_uart.begin(BAUD_RATE, SERIAL_8N1, UART1_RX_PIN, UART1_TX_PIN);
  if (!irda_uart.setMode(UART_MODE_IRDA)) {
    Serial.println("ERROR: Failed to set IRDA mode");
    while (1) {
      delay(100);
    }
  }
  if (!irda_uart.setIrdaDirection(ESP32_UART_IRDA_RX)) {
    Serial.println("ERROR: Failed to set IRDA RX mode");
    while (1) {
      delay(100);
    }
  }
  Serial.println("UART1 configured in IRDA RX mode");
  Serial.println("Waiting for IrDA frames...\n");
}

void loop() {
  if (irda_mode == MODE_TX) {
    loopTXMode();
  } else if (irda_mode == MODE_RX) {
    loopRXMode();
  }
}

void loopTXMode() {
  static uint32_t counter = 0;

  // Send IrDA data
  Serial.printf("TX -> Sending frame %lu\n", (unsigned long)counter);
  irda_uart.printf("FRAME_%lu", (unsigned long)counter);
  irda_uart.print("\n");  // Add newline as delimiter
  irda_uart.flush();

  counter++;
  delay(1000);  // Send new frame every 1 second
}

// Receive IrDA data
void loopRXMode() {
  uint32_t start = millis();
  bool gotData = false;

  while ((millis() - start) < 5000) {  // Wait 5 seconds for data
    while (irda_uart.available()) {
      char c = (char)irda_uart.read();
      if (c == '\n') {
        // End of frame
        if (received.length() > 0) {
          Serial.printf("RX <- Received: %s\n", received.c_str());
          gotData = true;
        }
        received = "";
      } else if (c > 31 && c < 127) {
        // Printable character
        if (received.length() < MAX_MSG_SIZE) {
          received += c;
        } else {
          Serial.printf("So far RX <- Received: %s%c\n", received.c_str(), c);
          Serial.printf("WARNING: Received message truncated (exceeds max size = %u)\r\n", MAX_MSG_SIZE);
          received = "";  // Clear buffer for new message
        }
      }
    }

    if (gotData) {
      // Optional: Send acknowledgment back
      // irda_uart.println("ACK");
      // irda_uart.flush();
      delay(100);
      break;
    }
    delay(1);
  }

  if (!gotData) {
    Serial.println("RX <- (waiting for IrDA frames from peer TX mode)");
  }

  delay(500);
}
