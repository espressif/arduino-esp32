/*
  This Sketch demonstrates how to use the IRDA mode with HardwareSerial.

  IRDA mode allows the ESP32 to communicate using infrared signals. The UART can be set to operate
  in either IRDA RX (receive) or IRDA TX (transmit) mode, but not both simultaneously.

  Functions used:
  - setMode(UART_MODE_IRDA) - Enables IRDA mode
  - setIrdaMode(uart_irda_mode_t irdaMode) - Switches between TX (ESP32_UART_IRDA_TX) and RX (ESP32_UART_IRDA_RX) modes

  IRDA operates in two modes:
  1. IRDA TX mode (ESP32_UART_IRDA_TX):  UART transmits data in IRDA format, only transmit works
  2. IRDA RX mode (ESP32_UART_IRDA_RX): UART receives data in IRDA format, only receive works

  Note: This example uses internal pin loopback for testing purposes. For real IRDA applications,
  use actual IRDA transceivers (transmitter/receiver modules) connected to the TX and RX pins.
*/

#include <Arduino.h>

// GPIO pins for Serial1 (modify these based on your ESP32 board)
#define IRDA_RX_PIN  4   // GPIO 4 => RX for Serial1 (IRDA receiver)
#define IRDA_TX_PIN  5   // GPIO 5 => TX for Serial1 (IRDA transmitter)
#define BAUD_RATE    9600

void setup() {
  // Initialize Serial0 (UART0) for debug output
  Serial.begin(115200);
  delay(100);
  Serial.println("\n\n================================");
  Serial.println("IRDA Mode Demonstration");
  Serial.println("================================\n");

  // Initialize Serial1 (UART1) with custom pins
  Serial1.begin(BAUD_RATE, SERIAL_8N1, IRDA_RX_PIN, IRDA_TX_PIN);
  while (!Serial1) {
    delay(10);
  }

  // Configure Serial1 for IRDA mode
  if (!Serial1.setMode(UART_MODE_IRDA)) {
    Serial.println("ERROR: Failed to set IRDA mode");
    while (1) delay(100);  // Halt on error
  }
  Serial.println("✓ IRDA mode enabled on Serial1");

  // Set up internal loopback for testing (TX to RX)
  // This allows testing without external IRDA hardware
  uart_internal_loopback(1, IRDA_RX_PIN);
  Serial.println("✓ Internal loopback configured (TX -> RX)");
  delay(100);

  // Demonstrate IRDA TX mode
  Serial.println("\n--- Testing IRDA TX Mode ---");
  testIrdaTxMode();

  delay(500);

  // Demonstrate IRDA RX mode
  Serial.println("\n--- Testing IRDA RX Mode ---");
  testIrdaRxMode();

  Serial.println("\n================================");
  Serial.println("IRDA demonstration complete!");
  Serial.println("================================\n");
}

void loop() {
  // No continuous operation in this demo
  delay(1000);
}

void testIrdaTxMode() {
  // Switch Serial1 to IRDA TX mode (transmit)
  if (!Serial1.setIrdaMode(ESP32_UART_IRDA_TX)) {
    Serial.println("ERROR: Failed to set IRDA TX mode");
    return;
  }
  Serial.println("Set to IRDA TX mode (transmit only)");

  // Send test message
  const char *testMessage = "HELLO";
  Serial.printf("Transmitting: %s\n", testMessage);
  Serial1.print(testMessage);
  Serial1.flush();

  delay(100);
  Serial.println("✓ Message transmitted in IRDA TX mode");
}

void testIrdaRxMode() {
  // Switch Serial1 to IRDA RX mode (receive)
  if (!Serial1.setIrdaMode(ESP32_UART_IRDA_RX)) {
    Serial.println("ERROR: Failed to set IRDA RX mode");
    return;
  }
  Serial.println("Set to IRDA RX mode (receive only)");

  // Wait a moment for any buffered data
  delay(50);

  // Read received data
  Serial.print("Received: ");
  int bytesRead = 0;
  while (Serial1.available()) {
    char c = Serial1.read();
    Serial.print(c);
    bytesRead++;
  }
  if (bytesRead > 0) {
    Serial.print(" (");
    Serial.print(bytesRead);
    Serial.println(" bytes)");
  } else {
    Serial.println("(no data received)");
  }
  Serial.println("✓ Ready to receive in IRDA RX mode");
}

/*
  IMPORTANT NOTES FOR REAL IRDA APPLICATIONS:

  1. Hardware Requirements:
     - IRDA TX (Transmitter): Infrared LED (e.g., 940nm) with suitable driver circuit
     - IRDA RX (Receiver): Infrared photodiode receiver module (e.g., TSOP38238)
     - Connect TX pin to IRDA transmitter, RX pin to IRDA receiver

  2. IRDA Mode Characteristics:
     - IRDA operates in exclusive modes (TX or RX, not both)
     - IRDA signals are typically modulated at a carrier frequency (e.g., 38kHz for TSOP receivers)
     - The ESP32 UART handles modulation/demodulation automatically in IRDA mode

  3. Switching Modes:
     - Call setIrdaMode(true) to switch to TX mode
     - Call setIrdaMode(false) to switch to RX mode
     - Switching modes does not require calling begin() again

  4. Common IRDA Frequencies:
     - 38 kHz: Most common, used in TVs, air conditioners
     - 56 kHz: Some devices
     - Other frequencies: Rare, check your IRDA device specifications

  5. For Serial Communication Over IRDA:
     - Ensure proper line-of-sight between transmitter and receiver
     - Test distance and angle tolerances
     - Consider adding error detection/correction in your protocol
*/
