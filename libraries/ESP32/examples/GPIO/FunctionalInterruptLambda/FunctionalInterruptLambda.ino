/*
   SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD

   SPDX-License-Identifier: Apache-2.0

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

   ESP32 Lambda FunctionalInterrupt Example
   ========================================

   This example demonstrates how to use lambda functions with FunctionalInterrupt
   for GPIO pin interrupt callbacks on ESP32. It shows CHANGE mode detection
   with LED toggle functionality and proper debouncing.

   Hardware Setup:
   - Use BOOT Button or connect a button between BUTTON_PIN and GND (with internal pullup)
   - Use Builtin Board LED or connect an LED with resistor to GPIO 2 (LED_PIN)

   Features Demonstrated:
   1. CHANGE mode lambda to detect both RISING and FALLING edges
   2. LED toggle on button press (FALLING edge)
   3. Edge type detection using digitalRead() within ISR
   4. Hardware debouncing with configurable timeout

   IMPORTANT NOTE ABOUT ESP32 INTERRUPT BEHAVIOR:
   - Only ONE interrupt handler can be attached per GPIO pin at a time
   - Calling attachInterrupt() on a pin that already has an interrupt will override the previous one
   - This applies regardless of edge type (RISING, FALLING, CHANGE)
   - If you need both RISING and FALLING detection on the same pin, use CHANGE mode
     and determine the edge type within your handler by reading the pin state
*/

#include <Arduino.h>
#include <FunctionalInterrupt.h>

// Pin definitions
#define BUTTON_PIN BOOT_PIN  // BOOT BUTTON - change as needed
#ifdef LED_BUILTIN
#define LED_PIN LED_BUILTIN
#else
#warning Using LED_PIN = GPIO 2 as default - change as needed
#define LED_PIN 2  // change as needed
#endif

// Global variables for interrupt handling (volatile for ISR safety)
volatile uint32_t buttonPressCount = 0;
volatile uint32_t buttonReleaseCount = 0;
volatile bool buttonPressed = false;
volatile bool buttonReleased = false;
volatile bool ledState = false;
volatile bool ledStateChanged = false;  // Flag to indicate LED needs updating

// Debouncing variables (volatile for ISR safety)
volatile unsigned long lastButtonInterruptTime = 0;
const unsigned long DEBOUNCE_DELAY_MS = 50;  // 50ms debounce delay

// State-based debouncing to prevent hysteresis issues
volatile bool lastButtonState = HIGH;  // Track last stable state (HIGH = released)

// Global lambda function (declared at file scope) - ISR in IRAM
IRAM_ATTR std::function<void()> changeModeLambda = []() {
  // Simple debouncing: check if enough time has passed since last interrupt
  unsigned long currentTime = millis();
  if (currentTime - lastButtonInterruptTime < DEBOUNCE_DELAY_MS) {
    return;  // Ignore this interrupt due to bouncing
  }

  // Read current pin state to determine edge type
  bool currentState = digitalRead(BUTTON_PIN);

  // State-based debouncing: only process if state actually changed
  if (currentState == lastButtonState) {
    return;  // No real state change, ignore (hysteresis/noise)
  }

  // Update timing and state
  lastButtonInterruptTime = currentTime;
  lastButtonState = currentState;

  if (currentState == LOW) {
    // FALLING edge detected (button pressed) - set flag for main loop
    // volatile variables require use of temporary value transfer
    uint32_t temp = buttonPressCount + 1;
    buttonPressCount = temp;
    buttonPressed = true;
    ledStateChanged = true;  // Signal main loop to toggle LED
  } else {
    // RISING edge detected (button released) - set flag for main loop
    // volatile variables require use of temporary value transfer
    uint32_t temp = buttonReleaseCount + 1;
    buttonReleaseCount = temp;
    buttonReleased = true;
  }
};

void setup() {
  Serial.begin(115200);
  delay(1000);  // Allow serial monitor to connect

  Serial.println("ESP32 Lambda FunctionalInterrupt Example");
  Serial.println("========================================");

  // Configure pins
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // CHANGE mode lambda to handle both RISING and FALLING edges
  // This toggles the LED on button press (FALLING edge)
  Serial.println("Setting up CHANGE mode lambda for LED toggle");

  // Use the global lambda function
  attachInterrupt(BUTTON_PIN, changeModeLambda, CHANGE);

  Serial.println();
  Serial.printf("Lambda interrupt configured on Pin %d (CHANGE mode)\r\n", BUTTON_PIN);
  Serial.printf("Debounce delay: %lu ms\r\n", DEBOUNCE_DELAY_MS);
  Serial.println();
  Serial.println("Press the button to toggle the LED!");
  Serial.println("Button press (FALLING edge) will toggle the LED.");
  Serial.println("Button release (RISING edge) will be detected and reported.");
  Serial.println("Button includes debouncing to prevent mechanical bounce issues.");
  Serial.println();
}

void loop() {
  // Handle LED state changes (ISR-safe approach)
  if (ledStateChanged) {
    ledStateChanged = false;
    ledState = !ledState;  // Toggle LED state in main loop
    digitalWrite(LED_PIN, ledState);
  }

  // Check for button presses
  if (buttonPressed) {
    buttonPressed = false;
    Serial.printf("==> Button PRESSED! Count: %lu, LED: %s (FALLING edge)\r\n", buttonPressCount, ledState ? "ON" : "OFF");
  }

  // Check for button releases
  if (buttonReleased) {
    buttonReleased = false;
    Serial.printf("==> Button RELEASED! Count: %lu (RISING edge)\r\n", buttonReleaseCount);
  }

  delay(10);
}
