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

// Simple Matter smart button — short click only (InitialPress + ShortRelease).
// For long-press and multi-press gestures see MatterEnhancedSmartButton example.

// Matter Manager
#include <Arduino.h>
#include <Matter.h>
// List of Matter Endpoints for this Node
// Generic Switch Endpoint - works as a smart button with a single click
MatterGenericSwitch SmartButton;

// Wi-Fi credentials for this sketch. Fill these in when the board cannot
// commission over BLE (Arduino prebuild on ESP32 / ESP32-S2): the sketch
// joins the AP itself. When Matter commissions over BLE (CHIPoBLE), the
// hub sends SSID and password — leave the placeholders; they are unused.
#define WIFI_SSID     "your-ssid"
#define WIFI_PASSWORD "your-password"

// set your board USER BUTTON pin here
const uint8_t buttonPin = BOOT_PIN;  // Set your pin here. Using BOOT Button.
MatterButton button;

void setup() {
  // Initialize the USER BUTTON (Boot button) GPIO that will act as a smart button or to decommission the Matter Node
  button.begin(buttonPin);

  Serial.begin(115200);

// CONFIG_ENABLE_CHIPOBLE=n: sketch starts Wi-Fi here; with CHIPoBLE the hub delivers credentials.
#if !CONFIG_ENABLE_CHIPOBLE
  matterConnectWiFi(WIFI_SSID, WIFI_PASSWORD);
#endif

  // Initialize the Matter EndPoint
  // FEATURE_SIMPLE (default): momentary switch + short release for a single click
  SmartButton.begin();

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();
  matterWaitUntilReady();
}

void loop() {
  matterRestartIfNoFabric();

  matterButtonEvent_t ev;
  while ((ev = button.poll()) != MATTER_BUTTON_NONE) {
    if (ev == MATTER_BUTTON_PRESS) {
      Serial.println("User button pressed. Sending InitialPress to the Matter Controller!");
      SmartButton.press();
    } else if (ev == MATTER_BUTTON_CLICK) {
      Serial.println("User button released. Sending ShortRelease to the Matter Controller!");
      SmartButton.release();
    } else if (ev == MATTER_BUTTON_LONG_HOLD) {
      Serial.println("Decommissioning the Generic Switch Matter Accessory. It shall be commissioned again.");
      Matter.decommission();
    }
  }
}
