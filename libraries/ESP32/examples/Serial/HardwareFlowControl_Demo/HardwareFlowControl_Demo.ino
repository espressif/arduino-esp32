/*
  Hardware Flow Control Demo for ESP32
  
  This sketch demonstrates UART hardware flow control using RTS (Request To Send) 
  and CTS (Clear To Send) signals with UART1 (HardwareSerial Serial1).
  
  CONFIGURATION:
  ==============
  
  Set USE_INTERNAL_MATRIX_PIN_LOOPBACK to 1 for internal GPIO matrix connections
  (no external wires needed). Set to 0 to use external wire connections.
  
  PIN CONNECTIONS (when USE_INTERNAL_MATRIX_PIN_LOOPBACK = 0):
  ============================================================
  
  For basic loopback with hardware flow control:
  - Connect GPIO2 (RTS1) to GPIO4 (CTS1) - Flow control loopback
  - Connect TX1 pin to RX1 pin - Data loopback
  
  For GPIO-controlled flow control demonstration:
  - Connect TX1 pin to RX1 pin - Data loopback
  - Connect GPIO2 (RTS1) to GPIO5 (GPIO_RTS_MONITOR) - Monitor RTS state
  - Connect GPIO4 (CTS1) to GPIO13 (GPIO_CTS_CTRL) - Control CTS signal
  - Use GPIO13 to manually control CTS signal (LOW = allow, HIGH = block)
  
  HARDWARE FLOW CONTROL EXPLANATION:
  ===================================
  
  RTS (Request To Send):
  - Output signal from UART (GPIO2 in this example)
  - Asserted LOW when UART is ready to receive data (RX buffer has space)
  - De-asserted HIGH when RX buffer is getting full (threshold reached)
  
  CTS (Clear To Send):
  - Input signal to UART (GPIO4 in this example)
  - UART will only transmit when CTS is LOW (asserted)
  - UART will pause transmission when CTS is HIGH (de-asserted)
  
  OPERATION:
  ==========
  
  The sketch demonstrates:
  - Periodic transmission of messages every second
  - Automatic flow control when USE_GPIO_CONTROL = false
  - Manual CTS control when USE_GPIO_CONTROL = true
  - Loopback reception of transmitted data
  - Status monitoring of RTS/CTS pin states
  
  NOTE: When USE_INTERNAL_MATRIX_PIN_LOOPBACK = 1, no external connections
  are needed as the ESP32 GPIO matrix handles the loopback internally.
*/

// setting it to 1 will allow internal matrix pin connection for RX1<->TX1 and RTS1<->CTS1
// otherwise it needs a wire for cross connecting the pins
#define USE_INTERNAL_MATRIX_PIN_LOOPBACK 1

// Pin definitions for UART1
#define UART1_RX_PIN RX1   // Default GPIO - UART1 RX pin
#define UART1_TX_PIN TX1   // Default GPIO - UART1 TX pin
#define UART1_RTS_PIN 2    // GPIO2 - UART1 RTS pin (output from UART)
#define UART1_CTS_PIN 4    // GPIO4 - UART1 CTS pin (input to UART)

// Optional: GPIO pins for manual flow control demonstration
// If using GPIO-controlled flow control, connect:
// - RTS1 to GPIO_RTS_MONITOR (to monitor RTS state)
// - CTS1 to GPIO_CTS_CTRL (to control CTS signal)
#define GPIO_RTS_MONITOR 5   // GPIO5 - Monitor RTS signal (connect RTS1 to this)
#define GPIO_CTS_CTRL    13  // GPIO13 - Control CTS signal (connect CTS1 to this)

// Set to true to use GPIO-controlled flow control, false for simple loopback
// Note: this control will be overriden by USE_INTERNAL_MATRIX_PIN_LOOPBACK when it is 1
#define USE_GPIO_CONTROL false

// Variables for demonstration
unsigned long lastSendTime = 0;
unsigned long lastStatusTime = 0;
const unsigned long sendInterval = 1000;    // Send data every 1 second
const unsigned long statusInterval = 2000;  // Print status every 2 seconds
int sendCounter = 0;

void printPinStatus() {
  Serial.println("\n=== UART1 Pin Status ===");
  Serial.printf("RX Pin (GPIO%d): Receiving data\n", UART1_RX_PIN);
  Serial.printf("TX Pin (GPIO%d): Transmitting data\n", UART1_TX_PIN);

  if (USE_GPIO_CONTROL) {
    // Read RTS state from monitor GPIO (connected to RTS1)
    bool rtsState = digitalRead(GPIO_RTS_MONITOR);
    Serial.printf("RTS Pin (GPIO%d): %s (LOW = ready to receive)\n",
                  UART1_RTS_PIN,
                  rtsState == LOW ? "LOW (Ready)" : "HIGH (Busy)");

    // Read CTS state from control GPIO (connected to CTS1)
    bool ctsState = digitalRead(GPIO_CTS_CTRL);
    Serial.printf("CTS Pin (GPIO%d): %s (LOW = can transmit)\n",
                  UART1_CTS_PIN,
                  ctsState == LOW ? "LOW (Clear)" : "HIGH (Blocked)");
  } else {
    Serial.printf("RTS Pin (GPIO%d): Hardware controlled (LOW = ready to receive)\n", UART1_RTS_PIN);
    Serial.printf("CTS Pin (GPIO%d): Hardware controlled (LOW = can transmit)\n", UART1_CTS_PIN);
    Serial.println("Note: RTS/CTS pins are hardware-controlled. Connect RTS1 to CTS1 for loopback.");
  }

  Serial.printf("Available for write: %d bytes\n", Serial1.availableForWrite());
  Serial.printf("Available to read: %d bytes\n", Serial1.available());
  Serial.println("========================\n");
}

void setup() {
  // Initialize Serial (USB) for debugging
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n\n========================================");
  Serial.println("ESP32 Hardware Flow Control Demo");
  Serial.println("========================================\n");

  // Configure GPIOs for flow control (only if using GPIO-controlled mode)
  if (USE_GPIO_CONTROL) {
    // Configure CTS control GPIO - this will control the CTS signal
    pinMode(GPIO_CTS_CTRL, OUTPUT);
    digitalWrite(GPIO_CTS_CTRL, LOW);  // Start with CTS LOW (clear to send)

    // Configure RTS monitor GPIO - this will monitor the RTS signal
    pinMode(GPIO_RTS_MONITOR, INPUT);
    Serial.println("Using GPIO-controlled flow control mode");
  } else {
    Serial.println("Using hardware-controlled flow control (simple loopback)");
  }

  // Initialize UART1 with hardware flow control
  Serial.println("Initializing UART1...");

  // Begin UART1 with 115200 baud, 8N1 configuration
  Serial1.begin(115200);

  // Set all pins for UART1
  if (!Serial1.setPins(UART1_RX_PIN, UART1_TX_PIN, UART1_CTS_PIN, UART1_RTS_PIN)) {
    Serial.println("ERROR: Failed to set CTS and RTS UART1 pins!");
    while (1) delay(1000);
  }

  Serial.println("Enabling hardware flow control...");
  if (!Serial1.setHwFlowCtrlMode(UART_HW_FLOWCTRL_CTS_RTS, 64)) {
    Serial.println("ERROR: Failed to enable hardware flow control!");
    while (1) delay(1000);
  }

#if USE_INTERNAL_MATRIX_PIN_LOOPBACK
  uart_internal_loopback(1, UART1_RX_PIN);
  uart_internal_hw_flow_ctrl_loopback(1, UART1_CTS_PIN);
#endif

  // Diagnostic: Check initial state after enabling flow control
  Serial.println("\nPost-initialization diagnostics:");
  Serial.printf("  Serial1.available(): %d bytes\n", Serial1.available());
  Serial.printf("  Serial1.availableForWrite(): %d bytes\n", Serial1.availableForWrite());
  if (USE_GPIO_CONTROL) {
    Serial.printf("  GPIO%d (CTS control): %s\n", GPIO_CTS_CTRL, digitalRead(GPIO_CTS_CTRL) == LOW ? "LOW" : "HIGH");
    Serial.printf("  GPIO%d (RTS monitor): %s\n", GPIO_RTS_MONITOR, digitalRead(GPIO_RTS_MONITOR) == LOW ? "LOW" : "HIGH");
  }
  Serial.println();

  Serial.println("UART1 initialized successfully!");
  Serial.println("Hardware flow control: ENABLED (RTS + CTS)");
  Serial.printf("RX Pin: GPIO%d\n", UART1_RX_PIN);
  Serial.printf("TX Pin: GPIO%d\n", UART1_TX_PIN);
  Serial.printf("RTS Pin: GPIO%d (output from UART)\n", UART1_RTS_PIN);
  Serial.printf("CTS Pin: GPIO%d (input to UART)\n", UART1_CTS_PIN);

#if USE_INTERNAL_MATRIX_PIN_LOOPBACK
  Serial.println("\nNO EXTERNAL PIN CONNECTIONS ARE REQUIRED:");
  Serial.println("-------------------------");
    Serial.println("Internal GPIO Matrix connection with flow control mode:");
    Serial.printf("  1. Automatic Internal Connection of GPIO%d (TX1) to GPIO%d (RX1) - Loopback\n", UART1_TX_PIN, UART1_RX_PIN);
    Serial.printf("  2. Automatic Internal Connection of GPIO%d (RTS1) to GPIO%d (CTS1) - Flow control loopback\n", UART1_RTS_PIN, UART1_CTS_PIN);
    Serial.println("\n  Note: In this mode, RTS/CTS are automatically controlled by hardware.");
    Serial.println("  RTS goes LOW when ready to receive, HIGH when buffer is full.");
    Serial.println("  CTS must be LOW for transmission to proceed.");
#else
  Serial.println("\nPIN CONNECTIONS REQUIRED:");
  Serial.println("-------------------------");
  if (USE_GPIO_CONTROL) {
    Serial.println("GPIO-controlled flow control mode:");
    Serial.printf("  1. Connect GPIO%d (TX1) to GPIO%d (RX1) - Loopback\n", UART1_TX_PIN, UART1_RX_PIN);
    Serial.printf("  2. Connect GPIO%d (RTS1) to GPIO%d - Monitor RTS state\n", UART1_RTS_PIN, GPIO_RTS_MONITOR);
    Serial.printf("  3. Connect GPIO%d (CTS1) to GPIO%d - Control CTS signal\n", UART1_CTS_PIN, GPIO_CTS_CTRL);
  } else {
    Serial.println("Hardware-controlled flow control (simple loopback):");
    Serial.printf("  1. Connect GPIO%d (TX1) to GPIO%d (RX1) - Loopback\n", UART1_TX_PIN, UART1_RX_PIN);
    Serial.printf("  2. Connect GPIO%d (RTS1) to GPIO%d (CTS1) - Flow control loopback\n", UART1_RTS_PIN, UART1_CTS_PIN);
    Serial.println("\n  Note: In this mode, RTS/CTS are automatically controlled by hardware.");
    Serial.println("  RTS goes LOW when ready to receive, HIGH when buffer is full.");
    Serial.println("  CTS must be LOW for transmission to proceed.");
  }
#endif
  Serial.println("\nStarting demonstration in 2 seconds...\n");
  delay(2000);
}

void loop() {
  unsigned long currentTime = millis();

  // Print status periodically
  if (currentTime - lastStatusTime >= statusInterval) {
    lastStatusTime = currentTime;
    printPinStatus();

    // Demonstrate flow control by toggling CTS (only in GPIO-controlled mode)
    if (USE_GPIO_CONTROL) {
      static bool ctsState = false;
      ctsState = !ctsState;

      if (ctsState) {
        Serial.println(">>> Blocking transmission (CTS HIGH)...");
        digitalWrite(GPIO_CTS_CTRL, HIGH);  // Block transmission
      } else {
        Serial.println(">>> Allowing transmission (CTS LOW)...");
        digitalWrite(GPIO_CTS_CTRL, LOW);  // Allow transmission
      }
    }
  }

  // Send data periodically
  if (currentTime - lastSendTime >= sendInterval) {
    lastSendTime = currentTime;
    sendCounter++;

    // Check if we can transmit (CTS must be LOW)
    // In GPIO-controlled mode, check the control GPIO; otherwise hardware handles it
    bool canTransmit = true;
    if (USE_GPIO_CONTROL) {
      canTransmit = (digitalRead(GPIO_CTS_CTRL) == LOW);
    }

    if (canTransmit) {
      char message[64];
      snprintf(message, sizeof(message),
               "Message #%d: Hello from UART1! Time: %lu ms\r\n",
               sendCounter, currentTime);

      Serial.print("Sending: ");
      Serial.print(message);

      size_t bytesWritten = Serial1.write((const uint8_t*)message, strlen(message));
      Serial.printf("  -> Written: %d bytes\n", bytesWritten);

      // Flush to ensure data is sent
      Serial1.flush();
    } else {
      Serial.println("!!! Transmission blocked - CTS is HIGH !!!");
    }
  }

  // Read and echo received data
  if (Serial1.available()) {
    Serial.print("Received: ");
    while (Serial1.available()) {
      char c = Serial1.read();
      Serial.write(c);
    }
    Serial.println();
  }

  // Small delay to prevent tight loop
  delay(10);
}
