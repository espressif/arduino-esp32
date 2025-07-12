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
   for GPIO pin interrupt callbacks on ESP32. It shows different lambda patterns
   and capture techniques for interrupt handling with debouncing.

   Hardware Setup:
   - Connect a button between GPIO 4 (BUTTON_PIN) and GND (with internal pullup)
   - Connect an LED with resistor to GPIO 2 (LED_PIN or use the built-in LED available on most boards)
   - Optionally connect a second button (BUTTON2_PIN) to another pin in case that there is no BOOT pin button available

   Features Demonstrated:
   1. CHANGE mode lambda to handle both RISING and FALLING edges on same pin
   2. Lambda function with captured variables (pointers)
   3. Object method calls integrated within lambda functions
   4. Edge type detection using digitalRead() within ISR
   5. Hardware debouncing with configurable timeout
   6. Best practices for interrupt-safe lambda functions

   IMPORTANT NOTE ABOUT ESP32 INTERRUPT BEHAVIOR:
   - Only ONE interrupt handler can be attached per GPIO pin at a time
   - Calling attachInterrupt() on a pin that already has an interrupt will override the previous one
   - This applies regardless of edge type (RISING, FALLING, CHANGE)
   - If you need both RISING and FALLING detection on the same pin, use CHANGE mode
     and determine the edge type within your handler by reading the pin state

   For detailed documentation, patterns, and best practices, see README.md

   Note: This example uses proper pointer captures for static/global variables
         to avoid compiler warnings about non-automatic storage duration.
*/

#include <Arduino.h>
#include <FunctionalInterrupt.h>

// Pin definitions
#define BUTTON_PIN      4        // Button pin (GPIO 4) - change as needed
#define BUTTON2_PIN     BOOT_PIN // BOOT BUTTON - change as needed
#ifdef LED_BUILTIN
#define LED_PIN         LED_BUILTIN
#else
#warning Using LED_PIN = GPIO 2 as default - change as needed
#define LED_PIN         2        // change as needed
#endif


// Global variables for interrupt counters (volatile for ISR safety)
volatile uint32_t buttonPressCount = 0;
volatile uint32_t buttonReleaseCount = 0;  // Track button releases separately
volatile uint32_t button2PressCount = 0;
volatile bool buttonPressed = false;
volatile bool buttonReleased = false;     // Flag for button release events
volatile bool button2Pressed = false;
volatile bool ledState = false;
volatile bool ledStateChanged = false;    // Flag to indicate LED needs updating

// Variables to demonstrate lambda captures
volatile uint32_t totalInterrupts = 0;
volatile unsigned long lastInterruptTime = 0;

// Debouncing variables (volatile for ISR safety)
volatile unsigned long lastButton1InterruptTime = 0;
volatile unsigned long lastButton2InterruptTime = 0;
const unsigned long DEBOUNCE_DELAY_MS = 50;  // 50ms debounce delay

// State-based debouncing to prevent hysteresis issues
volatile bool lastButton1State = HIGH;  // Track last stable state (HIGH = released)
volatile bool lastButton2State = HIGH;  // Track last stable state (HIGH = released)

// Class example for demonstrating lambda with object methods
class InterruptHandler {
  public:
    volatile uint32_t objectPressCount = 0;
    volatile bool stateChanged = false;
    String name;

    InterruptHandler(const String& handlerName) : name(handlerName) {}

    void handleButtonPress() {
      uint32_t temp = objectPressCount;
      temp++;
      objectPressCount = temp;
      stateChanged = true;
    }

    void printStatus() {
      if (stateChanged) {
        Serial.printf("Handler '%s': Press count = %lu\r\n", name.c_str(), objectPressCount);
        stateChanged = false;
      }
    }
};

// Global handler instance for object method example
static InterruptHandler globalHandler("ButtonHandler");

void setup() {
  Serial.begin(115200);
  delay(1000); // Allow serial monitor to connect

  Serial.println("ESP32 Lambda FunctionalInterrupt Example");
  Serial.println("========================================");

  // Configure pins
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Example 1: CHANGE mode lambda to handle both RISING and FALLING edges
  // This demonstrates how to properly handle both edges on the same pin
  // Includes: object method calls, pointer captures, and state-based debouncing
  Serial.println("Setting up Example 1: CHANGE mode lambda for both edges");

  // Create pointers for safe capture (avoiding non-automatic storage duration warnings)
  InterruptHandler* handlerPtr = &globalHandler;
  volatile unsigned long* lastButton1TimePtr = &lastButton1InterruptTime;
  volatile bool* lastButton1StatePtr = &lastButton1State;
  const unsigned long* debounceDelayPtr = &DEBOUNCE_DELAY_MS;

  std::function<void()> changeModeLambda = [handlerPtr, lastButton1TimePtr, lastButton1StatePtr, debounceDelayPtr]() {
    // Debouncing: check if enough time has passed since last interrupt
    unsigned long currentTime = millis();
    if (currentTime - (*lastButton1TimePtr) < (*debounceDelayPtr)) {
      return; // Ignore this interrupt due to bouncing
    }

    // Read current pin state to determine edge type
    bool currentState = digitalRead(BUTTON_PIN);

    // State-based debouncing: only process if state actually changed
    if (currentState == (*lastButton1StatePtr)) {
      return; // No real state change, ignore (hysteresis/noise)
    }

    // Update timing and state
    (*lastButton1TimePtr) = currentTime;
    (*lastButton1StatePtr) = currentState;

    if (currentState == LOW) {
      // FALLING edge detected (button pressed)
      uint32_t temp = buttonPressCount;
      temp++;
      buttonPressCount = temp;
      buttonPressed = true;

      // Call object method for press events
      handlerPtr->handleButtonPress();
    } else {
      // RISING edge detected (button released)
      uint32_t temp = buttonReleaseCount;
      temp++;
      buttonReleaseCount = temp;
      buttonReleased = true;

      // Object method calls can be different for release events
      // For demonstration, we'll call the same method but could be different
      handlerPtr->handleButtonPress();
    }
  };
  attachInterrupt(BUTTON_PIN, changeModeLambda, CHANGE);

  // Example 2: Lambda with captured variables (Pointer Captures)
  // This demonstrates safe capture of global variables via pointers
  // NOTE: We avoid calling digitalWrite() directly in ISR to prevent FreeRTOS scheduler issues
  Serial.println("Setting up Example 2: Lambda with pointer captures");

  // Create pointers to avoid capturing static/global variables directly
  uint32_t* totalInterruptsPtr = &totalInterrupts;
  unsigned long* lastInterruptTimePtr = &lastInterruptTime;
  volatile bool* ledStatePtr = &ledState;
  volatile bool* ledStateChangedPtr = &ledStateChanged;
  volatile unsigned long* lastButton2TimePtr = &lastButton2InterruptTime;
  volatile bool* lastButton2StatePtr = &lastButton2State;

  std::function<void()> captureLambda = [totalInterruptsPtr, lastInterruptTimePtr, ledStatePtr, ledStateChangedPtr, lastButton2TimePtr, lastButton2StatePtr, debounceDelayPtr]() {
    // Debouncing: check if enough time has passed since last interrupt
    unsigned long currentTime = millis();
    if (currentTime - (*lastButton2TimePtr) < (*debounceDelayPtr)) {
      return; // Ignore this interrupt due to bouncing
    }

    // Read current pin state and check for real state change
    bool currentState = digitalRead(BUTTON2_PIN);

    // State-based debouncing: only process if state actually changed to LOW (pressed)
    // and the last state was HIGH (released)
    if (currentState != LOW || (*lastButton2StatePtr) != HIGH) {
      return; // Not a valid press event, ignore
    }

    // Update timing and state
    (*lastButton2TimePtr) = currentTime;
    (*lastButton2StatePtr) = currentState;

    // Update button press count
    uint32_t temp = button2PressCount;
    temp++;
    button2PressCount = temp;
    button2Pressed = true;

    // Update captured variables via pointers
    (*totalInterruptsPtr)++;
    (*lastInterruptTimePtr) = millis();

    // Toggle LED state and set flag for main loop to handle
    (*ledStatePtr) = !(*ledStatePtr);
    (*ledStateChangedPtr) = true;  // Signal main loop to update LED
  };
  attachInterrupt(BUTTON2_PIN, captureLambda, FALLING);

  Serial.println();
  Serial.println("Lambda interrupts configured:");
  Serial.printf("- Button 1 (Pin %d): CHANGE mode lambda (handles both press and release)\r\n", BUTTON_PIN);
  Serial.printf("- Button 2 (Pin %d): FALLING mode lambda with LED control\r\n", BUTTON2_PIN);
  Serial.printf("- Debounce delay: %lu ms for both buttons\r\n", DEBOUNCE_DELAY_MS);
  Serial.println();
  Serial.println("Press and release the buttons to see lambda interrupts in action!");
  Serial.println("Button 1 will detect both press (FALLING) and release (RISING) events.");
  Serial.println("Button 2 (FALLING only) will toggle the LED.");
  Serial.println("Both buttons include debouncing to prevent mechanical bounce issues.");
  Serial.println();
}

void loop() {
  static unsigned long lastPrintTime = 0;
  static uint32_t lastButton1PressCount = 0;
  static uint32_t lastButton1ReleaseCount = 0;
  static uint32_t lastButton2Count = 0;

  // Handle LED state changes (ISR-safe approach)
  if (ledStateChanged) {
    ledStateChanged = false;
    digitalWrite(LED_PIN, ledState);
  }

  // Update button states in main loop (for proper state tracking)
  // This helps prevent hysteresis issues by updating state when buttons are actually released
  static bool lastButton2Reading = HIGH;
  bool currentButton2Reading = digitalRead(BUTTON2_PIN);
  if (currentButton2Reading == HIGH && lastButton2Reading == LOW) {
    // Button 2 was released, update state
    lastButton2State = HIGH;
  }
  lastButton2Reading = currentButton2Reading;

  // Check for button 1 presses and releases (CHANGE mode lambda)
  if (buttonPressed) {
    buttonPressed = false;
    Serial.printf("==> Button 1 PRESSED! Count: %lu (FALLING edge detected)\r\n", buttonPressCount);
  }

  if (buttonReleased) {
    buttonReleased = false;
    Serial.printf("==> Button 1 RELEASED! Count: %lu (RISING edge detected)\r\n", buttonReleaseCount);
  }

  // Check for button 2 presses (capture lambda)
  if (button2Pressed) {
    button2Pressed = false;
    Serial.printf("==> Button 2 pressed! Count: %lu, LED: %s (Capture lambda)\r\n",
                  button2PressCount, ledState ? "ON" : "OFF");
  }

  // Check object handler status (object method lambda)
  globalHandler.printStatus();

  // Print statistics every 5 seconds if there's been activity
  if (millis() - lastPrintTime >= 5000) {
    lastPrintTime = millis();

    bool hasActivity = (buttonPressCount != lastButton1PressCount ||
                        buttonReleaseCount != lastButton1ReleaseCount ||
                        button2PressCount != lastButton2Count);

    if (hasActivity) {
      Serial.println("============================");
      Serial.println("Lambda Interrupt Statistics:");
      Serial.println("============================");
      Serial.printf("Button 1 presses:     %8lu\r\n", buttonPressCount);
      Serial.printf("Button 1 releases:    %8lu\r\n", buttonReleaseCount);
      Serial.printf("Button 2 presses:     %8lu\r\n", button2PressCount);
      Serial.printf("Object handler calls: %8lu\r\n", globalHandler.objectPressCount);
      Serial.printf("Total interrupts:     %8lu\r\n", totalInterrupts);
      Serial.printf("LED state:            %8s\r\n", ledState ? "ON" : "OFF");
      if (lastInterruptTime > 0) {
        Serial.printf("Last interrupt:       %8lu ms ago\r\n", millis() - lastInterruptTime);
      }
      Serial.println();

      lastButton1PressCount = buttonPressCount;
      lastButton1ReleaseCount = buttonReleaseCount;
      lastButton2Count = button2PressCount;
    }
  }

  // Small delay to prevent overwhelming the serial output
  delay(10);
}