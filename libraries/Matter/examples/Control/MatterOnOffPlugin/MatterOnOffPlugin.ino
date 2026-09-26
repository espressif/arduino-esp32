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
#include <Preferences.h>

// List of Matter Endpoints for this Node
// On/Off Plugin Endpoint
MatterOnOffPlugin OnOffPlugin;

// Wi-Fi credentials for this sketch. Fill these in when the board cannot
// commission over BLE (Arduino prebuild on ESP32 / ESP32-S2): the sketch
// joins the AP itself. When Matter commissions over BLE (CHIPoBLE), the
// hub sends SSID and password — leave the placeholders; they are unused.
#define WIFI_SSID     "your-ssid"
#define WIFI_PASSWORD "your-password"

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
  matterConnectWiFi(WIFI_SSID, WIFI_PASSWORD);
#endif

  // Initialize Matter EndPoint
  matterPref.begin("MatterPrefs", false);
  bool lastOnOffState = matterPref.getBool(onOffPrefKey, false);
  OnOffPlugin.begin(lastOnOffState);
  OnOffPlugin.onChange(setPluginOnOff);

  // Matter beginning - Last step, after all EndPoints are initialized
  matterSetExampleIdentity("OnOff Plugin");
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
