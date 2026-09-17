// Copyright 2025 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Matter smart button gestures on one BOOT button (FEATURE_ALL).
//   tap            -> single click   (InitialPress + ShortRelease + MultiPressComplete 1)
//   tap tap        -> double click   (… + MultiPressOngoing 2 + MultiPressComplete 2)
//   tap tap tap    -> triple click   (same pattern, count 3; up to multiPressMax)
//   hold ~1 s      -> long press     (LongPress + LongRelease)
//   hold 5 s       -> factory reset  (Matter.decommission(), not a Switch event)
// For a short-click-only button see MatterSmartButton. For named sibling buttons see MatterSmartButtonsTagList.

// Matter Manager
#include <Arduino.h>
#include <Matter.h>
// List of Matter Endpoints for this Node
// Generic Switch Endpoint - works as a smart button with full gesture support
MatterGenericSwitch SmartButton;

// Wi-Fi credentials for this sketch. Fill these in when the board cannot
// commission over BLE (Arduino prebuild on ESP32 / ESP32-S2): the sketch
// joins the AP itself. When Matter commissions over BLE (CHIPoBLE), the
// hub sends SSID and password — leave the placeholders; they are unused.
#define WIFI_SSID     "your-ssid"
#define WIFI_PASSWORD "your-password"

// set your board USER BUTTON pin here
const uint8_t buttonPin = BOOT_PIN;  // Set your pin here. Using BOOT Button.

// Timing (adjust to match your hub / use case)
const uint32_t debounceMs = 50;                // button debouncing time (ms)
const uint32_t longPressMs = 1000;             // hold duration to trigger LongPress (ms)
const uint32_t multiPressWindowMs = 400;       // max gap between taps for double/triple click (ms)
const uint32_t decommissioningTimeout = 5000;  // hold 5s or longer to decommission (not a Switch gesture)
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

static const char *clickGestureName(uint8_t count) {
  switch (count) {
    case 1:  return "SINGLE CLICK";
    case 2:  return "DOUBLE CLICK";
    case 3:  return "TRIPLE CLICK";
    default: return "MULTI CLICK";
  }
}

static void handlePressDown() {
  pressStartMs = millis();
  longPressSent = false;

  if (inMultiPressSequence && millis() < multiPressDeadlineMs && pressesInSequence >= 1) {
    // Additional press within the multi-press window
    uint8_t count = pressesInSequence + 1;
    Serial.printf("Tap %u in this sequence. Sending MultiPressOngoing (count=%u).\r\n", count, count);
    SmartButton.multiPressOngoing(count);
  } else {
    // First press in a new sequence
    pressesInSequence = 0;
    inMultiPressSequence = true;
    Serial.println("Button down. Sending InitialPress.");
    SmartButton.press();
  }
}

static void handlePressUp() {
  if (longPressSent) {
    // Release after a long press
    Serial.println("Button up after hold. Sending LongRelease.");
    Serial.println(">>> Gesture: LONG PRESS");
    SmartButton.longRelease();
    longPressSent = false;
    inMultiPressSequence = false;
    pressesInSequence = 0;
    multiPressDeadlineMs = 0;
    return;
  }

  // Short release
  Serial.println("Button up. Sending ShortRelease.");
  SmartButton.release();
  pressesInSequence++;
  multiPressDeadlineMs = millis() + multiPressWindowMs;
}

static void checkLongPress() {
  if (buttonPressed && !longPressSent && (millis() - pressStartMs >= longPressMs)) {
    longPressSent = true;
    Serial.println("Hold reached 1 s. Sending LongPress.");
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
    Serial.printf(">>> Gesture: %s (MultiPressComplete count=%u)\r\n", clickGestureName(pressesInSequence), pressesInSequence);
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

// CONFIG_ENABLE_CHIPOBLE=n: sketch starts Wi-Fi here; with CHIPoBLE the hub delivers credentials.
#if !CONFIG_ENABLE_CHIPOBLE
  matterConnectWiFi(WIFI_SSID, WIFI_PASSWORD);
#endif

  // Initialize the Matter EndPoint
  // FEATURE_ALL: momentary switch + short release + long press + multi-press
  SmartButton.begin(MatterGenericSwitch::FEATURE_ALL, multiPressMax);

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();
  matterWaitUntilReady();
}

void loop() {
  matterRestartIfNoFabric();

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
