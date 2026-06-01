/*
  IrdaMode_DualUART_Demo.ino

  Demonstrates UART IrDA direction switching using two UARTs on a single ESP32 board.
  This example is for SoCs with 3+ UARTs (ESP32, ESP32-S3, etc.).

  UART Configuration:
  - UART1: IrDA TX mode (transmitter)
  - UART2: IrDA RX mode (receiver)
  - Internal loopback: UART1 TX → UART2 RX (no external hardware needed)

  Functions used:
  - setMode(UART_MODE_IRDA) - Enable IrDA mode
  - setIrdaDirection(ESP32_UART_IRDA_TX / ESP32_UART_IRDA_RX) - Set direction

  Hardware notes:
  - Single ESP32 board with 3+ UARTs required
  - Internal GPIO matrix loopback (no external connections needed)
  - Perfect for development and testing IrDA functionality
*/

#include <Arduino.h>

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

// UART pin definitions
#define UART1_RX_PIN RX1  // RX UART1 pin
#define UART1_TX_PIN TX1  // TX UART1 pin
#if HAS_UART2
#define UART2_RX_PIN RX2  // RX UART2 pin
#define UART2_TX_PIN TX2  // TX UART2 pin
#endif
#define BAUD_RATE  9600
#define RX_TIMEOUT 300

HardwareSerial &uart_tx = Serial1;
#if HAS_UART2
HardwareSerial &uart_rx = Serial2;
String received = "";              // Buffer for received message
const uint16_t MAX_MSG_SIZE = 64;  // Limit message size
#endif

void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println("\n=== UART IrDA Dual-UART Demo (Single Board) ===");

#if HAS_UART2
  Serial.println("✓ Board has 3+ UARTs: Using internal cross-UART loopback");
  setupDualUART();
  received.reserve(MAX_MSG_SIZE);  // Pre-allocate for efficiency
#else
  Serial.println("✗ ERROR: This example requires 3+ UARTs (UART0, UART1, UART2)");
  Serial.println("  Your board only has 2 UARTs.");
  Serial.println("  Use IrdaMode_TwoBoard_Demo.ino instead for two-board setup.");
  while (1) {
    delay(1000);
  }
#endif
}

#if HAS_UART2
void setupDualUART() {
  // Initialize UART1 in TX mode
  uart_tx.begin(BAUD_RATE, SERIAL_8N1, UART1_RX_PIN, UART1_TX_PIN);
  if (!uart_tx.setMode(UART_MODE_IRDA)) {
    Serial.println("ERROR: Failed to set UART1 to IRDA mode");
    while (1) {
      delay(100);
    }
  }
  if (!uart_tx.setIrdaDirection(ESP32_UART_IRDA_TX)) {
    Serial.println("ERROR: Failed to set UART1 to IRDA TX mode");
    while (1) {
      delay(100);
    }
  }
  Serial.println("✓ UART1 initialized in IRDA TX mode");

  // Initialize UART2 in RX mode
  uart_rx.begin(BAUD_RATE, SERIAL_8N1, UART2_RX_PIN, UART2_TX_PIN);
  if (!uart_rx.setMode(UART_MODE_IRDA)) {
    Serial.println("ERROR: Failed to set UART2 to IRDA mode");
    while (1) {
      delay(100);
    }
  }
  if (!uart_rx.setIrdaDirection(ESP32_UART_IRDA_RX)) {
    Serial.println("ERROR: Failed to set UART2 to IRDA RX mode");
    while (1) {
      delay(100);
    }
  }
  Serial.println("✓ UART2 initialized in IRDA RX mode");

  // Set up internal loopback: UART1 TX -> UART2 RX
  uart_internal_loopback(1, UART2_RX_PIN);
  Serial.println("✓ Internal loopback configured (UART1 TX → UART2 RX)");

  Serial.println("\nDemo will alternate between:");
  Serial.println("1) UART1 transmits message in IRDA TX mode");
  Serial.println("2) UART2 receives message in IRDA RX mode");
  Serial.println();
}
#endif

void loop() {
  static uint32_t counter = 0;

#if HAS_UART2
  loopDualUART(counter);
#endif

  counter++;
  delay(500);
}

#if HAS_UART2
void loopDualUART(uint32_t counter) {

  // UART1 TX mode: transmit
  Serial.printf("TX (UART1) -> PING %lu\n", (unsigned long)counter);
  uart_tx.printf("PING %lu\n", (unsigned long)counter);
  uart_tx.flush();
  delay(100);

  // UART2 RX mode: receive
  uint32_t start = millis();
  bool gotReply = false;

  while ((millis() - start) < RX_TIMEOUT) {
    while (uart_rx.available()) {
      char c = (char)uart_rx.read();
      if (received.length() < MAX_MSG_SIZE) {
        received += c;
      } else {
        Serial.printf("WARNING: Received message truncated (exceeds max size = %u)\r\n", MAX_MSG_SIZE);
      }
      gotReply = true;
    }
    delay(1);
  }

  if (gotReply) {
    Serial.print("RX (UART2) <- ");
    Serial.println(received);
    received = "";  // Clear buffer for new message
  } else {
    Serial.println("RX (UART2) <- (no data received)");
  }
}
#endif
