// Copyright 2025 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Enhanced Matter smart button — short click, long press, and multi-press gestures.
// Maps physical button actions to all Matter Generic Switch momentary events.
// For a minimal short-click-only implementation see MatterSmartButton example.

// Matter Manager
#include <Arduino.h>
#include <Matter.h>
#if !CONFIG_ENABLE_CHIPOBLE
// if the device can be commissioned using BLE, WiFi is not used - save flash space
#include <WiFi.h>
#endif

// List of Matter Endpoints for this Node
// Generic Switch Endpoint - works as a smart button with full gesture support
MatterGenericSwitch SmartButton;

// CONFIG_ENABLE_CHIPOBLE is enabled when BLE is used to commission the Matter Network
#if !CONFIG_ENABLE_CHIPOBLE
// WiFi is manually set and started
const char *ssid = "your-ssid";          // Change this to your WiFi SSID
const char *password = "your-password";  // Change this to your WiFi password
#endif

// set your board USER BUTTON pin here
const uint8_t buttonPin = BOOT_PIN;  // Set your pin here. Using BOOT Button.

// Timing (adjust to match your hub / use case)
const uint32_t debounceMs = 50;                // button debouncing time (ms)
const uint32_t longPressMs = 1000;             // hold duration to trigger LongPress event (ms)
const uint32_t multiPressWindowMs = 300;       // max gap between clicks for multi-press (ms)
const uint32_t decommissioningTimeout = 5000;  // keep the button pressed for 5s, or longer, to decommission
const uint8_t multiPressMax = 5;               // maximum press count reported to Matter (2–255)

// Button state
bool buttonPressed = false;  // raw debounced state: false = released | true = pressed
bool stablePressed = false;  // stable press state after debounce
uint32_t lastChangeMs = 0;   // debouncing control
uint32_t pressStartMs = 0;   // time when the current press started
bool longPressSent = false;  // true after LongPress has been sent for the current hold

// Multi-press sequence state (mirrors esp-matter generic_switch example)
bool inMultiPressSequence = false;  // true while a multi-press sequence is in progress
uint8_t pressesInSequence = 0;      // number of presses counted in the current sequence
uint32_t multiPressDeadlineMs = 0;  // time when the multi-press window expires

static bool readButtonPressed() {
  return digitalRead(buttonPin) == LOW;
}

static void handlePressDown() {
  pressStartMs = millis();
  longPressSent = false;

  if (inMultiPressSequence && millis() < multiPressDeadlineMs && pressesInSequence >= 1) {
    // Additional press within the multi-press window
    uint8_t count = pressesInSequence + 1;
    Serial.printf("User button pressed again. Sending MultiPressOngoing (count=%u) to the Matter Controller!\r\n", count);
    SmartButton.multiPressOngoing(count);
  } else {
    // First press in a new sequence
    pressesInSequence = 0;
    inMultiPressSequence = true;
    Serial.println("User button pressed. Sending InitialPress to the Matter Controller!");
    SmartButton.press();
  }
}

static void handlePressUp() {
  if (longPressSent) {
    // Release after a long press
    Serial.println("User button released after long press. Sending LongRelease to the Matter Controller!");
    SmartButton.longRelease();
    longPressSent = false;
    inMultiPressSequence = false;
    pressesInSequence = 0;
    multiPressDeadlineMs = 0;
    return;
  }

  // Short release
  Serial.println("User button released. Sending ShortRelease to the Matter Controller!");
  SmartButton.release();
  pressesInSequence++;
  multiPressDeadlineMs = millis() + multiPressWindowMs;
}

static void checkLongPress() {
  if (buttonPressed && !longPressSent && (millis() - pressStartMs >= longPressMs)) {
    longPressSent = true;
    Serial.println("User button held. Sending LongPress to the Matter Controller!");
    SmartButton.longPress();
  }
}

static void checkMultiPressComplete() {
  if (!inMultiPressSequence || multiPressDeadlineMs == 0 || buttonPressed) {
    return;
  }
  if (millis() < multiPressDeadlineMs) {
    return;
  }

  if (SmartButton.hasFeature(MatterGenericSwitch::FEATURE_MULTI_PRESS) && pressesInSequence > 0) {
    Serial.printf("Multi-press window expired. Sending MultiPressComplete (count=%u) to the Matter Controller!\r\n", pressesInSequence);
    SmartButton.multiPressComplete(pressesInSequence);
  }

  inMultiPressSequence = false;
  pressesInSequence = 0;
  multiPressDeadlineMs = 0;
}

void setup() {
  // Initialize the USER BUTTON (Boot button) GPIO that will act as a smart button or to decommission the Matter Node
  pinMode(buttonPin, INPUT_PULLUP);

  Serial.begin(115200);

// CONFIG_ENABLE_CHIPOBLE is enabled when BLE is used to commission the Matter Network
#if !CONFIG_ENABLE_CHIPOBLE
  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);

  // Manually connect to WiFi
  WiFi.begin(ssid, password);
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\r\nWiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  delay(500);
#endif

  // Initialize the Matter EndPoint
  // FEATURE_ALL: momentary switch + short release + long press + multi-press
  SmartButton.begin(MatterGenericSwitch::FEATURE_ALL, multiPressMax);

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();
  // This may be a restart of a already commissioned Matter accessory
  if (Matter.isDeviceCommissioned()) {
    Serial.println("Matter Node is commissioned and connected to the network. Ready for use.");
  }
}

void loop() {
  // Check Matter Accessory Commissioning state, which may change during execution of loop()
  if (!Matter.isDeviceCommissioned()) {
    Serial.println("");
    Serial.println("Matter Node is not commissioned yet.");
    Serial.println("Initiate the device discovery in your Matter environment.");
    Serial.println("Commission it to your Matter hub with the manual pairing code or QR code");
    Serial.printf("Manual pairing code: %s\r\n", Matter.getManualPairingCode().c_str());
    Serial.printf("QR code URL: %s\r\n", Matter.getOnboardingQRCodeUrl().c_str());
    // waits for Matter Generic Switch Commissioning.
    uint32_t timeCount = 0;
    while (!Matter.isDeviceCommissioned()) {
      delay(100);
      if ((timeCount++ % 50) == 0) {  // 50*100ms = 5 sec
        Serial.println("Matter Node not commissioned yet. Waiting for commissioning.");
      }
    }
    Serial.println("Matter Node is commissioned and connected to the network. Ready for use.");
  }

  // A builtin button is used to trigger gesture events to the Matter Controller
  bool rawPressed = readButtonPressed();

  // deals with button debouncing
  if (rawPressed != buttonPressed) {
    if (millis() - lastChangeMs >= debounceMs) {
      buttonPressed = rawPressed;
      lastChangeMs = millis();

      if (buttonPressed && !stablePressed) {
        stablePressed = true;
        handlePressDown();
      } else if (!buttonPressed && stablePressed) {
        stablePressed = false;
        handlePressUp();
      }
    }
  } else {
    lastChangeMs = millis();
  }

  // Check for long press while the button is held
  checkLongPress();

  // Check if the multi-press window has expired after the last release
  checkMultiPressComplete();

  // Onboard User Button is kept pressed for longer than 5 seconds in order to decommission matter node
  // (factory reset — separate from the Matter long-press gesture)
  if (buttonPressed && (millis() - pressStartMs > decommissioningTimeout)) {
    Serial.println("Decommissioning the Generic Switch Matter Accessory. It shall be commissioned again.");
    Matter.decommission();
    pressStartMs = millis();  // avoid running decommissining again, reboot takes a second or so
  }
}
