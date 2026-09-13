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

// Matter Manager
#include <Arduino.h>
#include <Matter.h>
#if !CONFIG_ENABLE_CHIPOBLE
// WiFi.h / WiFi.begin() only when this build has no CHIPoBLE (CONFIG_ENABLE_CHIPOBLE=n). Hub-delivered Wi-Fi still uses CHIP's stack.
#include <WiFi.h>
#endif
#include <Preferences.h>

// List of Matter Endpoints for this Node
// On/Off Plugin Endpoint
MatterOnOffPlugin OnOffPlugin;

// CONFIG_ENABLE_CHIPOBLE=n: sketch starts Wi-Fi here; with CHIPoBLE the hub delivers credentials.
#if !CONFIG_ENABLE_CHIPOBLE
// Wi-Fi is manually set and started
const char *ssid = "your-ssid";          // Change this to your Wi-Fi SSID
const char *password = "your-password";  // Change this to your Wi-Fi password
#endif

// it will keep last OnOff state stored, using Preferences
Preferences matterPref;
const char *onOffPrefKey = "OnOff";

// set your board Power Relay pin here - this example uses the built-in LED for easy visualization
#ifdef LED_BUILTIN
const uint8_t onoffPin = LED_BUILTIN;
#else
const uint8_t onoffPin = 2;  // Set your pin here - usually a power relay
#warning "Do not forget to set the Power Relay pin"
#endif

// board USER BUTTON pin necessary for Decommissioning
const uint8_t buttonPin = BOOT_PIN;  // Set your pin here. Using BOOT Button.
MatterButton button;

// Matter Protocol Endpoint Callback
bool setPluginOnOff(bool state) {
  Serial.printf("User Callback :: New Plugin State = %s\r\n", state ? "ON" : "OFF");
  if (state) {
    digitalWrite(onoffPin, HIGH);
  } else {
    digitalWrite(onoffPin, LOW);
  }
  // store last OnOff state for when the Plugin is restarted / power goes off
  matterPref.putBool(onOffPrefKey, state);
  // This callback must return the success state to Matter core
  return true;
}

void setup() {
  // Initialize the USER BUTTON
  button.begin(buttonPin);
  // Initialize the Power Relay (plugin) GPIO
  pinMode(onoffPin, OUTPUT);

  Serial.begin(115200);

// CONFIG_ENABLE_CHIPOBLE=n: sketch starts Wi-Fi here; with CHIPoBLE the hub delivers credentials.
#if !CONFIG_ENABLE_CHIPOBLE
  // We start by connecting to a Wi-Fi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
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

  // Initialize Matter EndPoint
  matterPref.begin("MatterPrefs", false);
  bool lastOnOffState = matterPref.getBool(onOffPrefKey, false);
  OnOffPlugin.begin(lastOnOffState);
  OnOffPlugin.onChange(setPluginOnOff);

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();
  matterWaitUntilReady();
  Serial.printf("Initial state: %s\r\n", OnOffPlugin.getOnOff() ? "ON" : "OFF");
  OnOffPlugin.updateAccessory();
}

void loop() {
  matterRestartIfNoFabric();

  matterButtonEvent_t ev;
  while ((ev = button.poll()) != MATTER_BUTTON_NONE) {
    if (ev == MATTER_BUTTON_LONG_HOLD) {
      Serial.println("Decommissioning the Plugin Matter Accessory. It shall be commissioned again.");
      OnOffPlugin.setOnOff(false);
      Matter.decommission();
    }
  }
}
