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

// CONFIG_ENABLE_CHIPOBLE=n: sketch starts Wi-Fi here; with CHIPoBLE the hub delivers credentials.
#if !CONFIG_ENABLE_CHIPOBLE
// Wi-Fi is manually set and started
const char *ssid = "your-ssid";          // Change this to your Wi-Fi SSID
const char *password = "your-password";  // Change this to your Wi-Fi password
#endif

// List of Matter Endpoints for this Node
// On/Off Light Endpoint
MatterOnOffLight OnOffLight;

// it will keep last OnOff state stored, using Preferences
Preferences matterPref;
const char *onOffPrefKey = "OnOff";

// set your board LED pin here
#ifdef LED_BUILTIN
const uint8_t ledPin = LED_BUILTIN;
#else
const uint8_t ledPin = 2;  // Set your pin here if your board has not defined LED_BUILTIN
#warning "Do not forget to set the LED pin"
#endif

// set your board USER BUTTON pin here
const uint8_t buttonPin = BOOT_PIN;  // Set your pin here. Using BOOT Button.
MatterButton button;

// Matter Protocol Endpoint Callback
bool setLightOnOff(bool state) {
  Serial.printf("User Callback :: New Light State = %s\r\n", state ? "ON" : "OFF");
  if (state) {
    digitalWrite(ledPin, HIGH);
  } else {
    digitalWrite(ledPin, LOW);
  }
  // store last OnOff state for when the Light is restarted / power goes off
  matterPref.putBool(onOffPrefKey, state);
  // This callback must return the success state to Matter core
  return true;
}

void setup() {
  // Initialize the USER BUTTON (Boot button) GPIO that will act as a toggle switch
  button.begin(buttonPin);
  // Initialize the LED (light) GPIO and Matter End Point
  pinMode(ledPin, OUTPUT);

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

  // Initialize Matter EndPoint
  matterPref.begin("MatterPrefs", false);
  bool lastOnOffState = matterPref.getBool(onOffPrefKey, true);
  OnOffLight.begin(lastOnOffState);
  OnOffLight.onChange(setLightOnOff);

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();
  matterWaitUntilReady();
  Serial.printf("Initial state: %s\r\n", OnOffLight.getOnOff() ? "ON" : "OFF");
  OnOffLight.updateAccessory();
}

void loop() {
  matterRestartIfNoFabric();

  matterButtonEvent_t ev;
  while ((ev = button.poll()) != MATTER_BUTTON_NONE) {
    if (ev == MATTER_BUTTON_CLICK) {
      Serial.println("User button released. Toggling Light!");
      OnOffLight.toggle();
    } else if (ev == MATTER_BUTTON_LONG_HOLD) {
      Serial.println("Decommissioning the Light Matter Accessory. It shall be commissioned again.");
      OnOffLight.setOnOff(false);
      Matter.decommission();
    }
  }
}
