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
#if !CONFIG_ENABLE_CHIPOBLE
// WiFi.h / WiFi.begin() only when this build has no CHIPoBLE (CONFIG_ENABLE_CHIPOBLE=n). Hub-delivered Wi-Fi still uses CHIP's stack.
#include <WiFi.h>
#endif

// List of Matter Endpoints for this Node
// Generic Switch Endpoint - works as a smart button with a single click
MatterGenericSwitch SmartButton;

// CONFIG_ENABLE_CHIPOBLE=n: sketch starts Wi-Fi here; with CHIPoBLE the hub delivers credentials.
#if !CONFIG_ENABLE_CHIPOBLE
// Wi-Fi is manually set and started
const char *ssid = "your-ssid";          // Change this to your Wi-Fi SSID
const char *password = "your-password";  // Change this to your Wi-Fi password
#endif

// set your board USER BUTTON pin here
const uint8_t buttonPin = BOOT_PIN;  // Set your pin here. Using BOOT Button.
MatterButton button;

void setup() {
  // Initialize the USER BUTTON (Boot button) GPIO that will act as a smart button or to decommission the Matter Node
  button.begin(buttonPin);

  Serial.begin(115200);

// CONFIG_ENABLE_CHIPOBLE=n: sketch starts Wi-Fi here; with CHIPoBLE the hub delivers credentials.
#if !CONFIG_ENABLE_CHIPOBLE
  // We start by connecting to a Wi-Fi network
  Serial.print("Connecting to ");
  Serial.println(ssid);

  // Manually connect to Wi-Fi
  WiFi.begin(ssid, password);
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\r\nWi-Fi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  delay(500);
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
