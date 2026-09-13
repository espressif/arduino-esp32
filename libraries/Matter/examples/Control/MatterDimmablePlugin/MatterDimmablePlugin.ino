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
// Dimmable Plugin Endpoint
MatterDimmablePlugin DimmablePlugin;

// CONFIG_ENABLE_CHIPOBLE=n: sketch starts Wi-Fi here; with CHIPoBLE the hub delivers credentials.
#if !CONFIG_ENABLE_CHIPOBLE
// Wi-Fi is manually set and started
const char *ssid = "your-ssid";          // Change this to your Wi-Fi SSID
const char *password = "your-password";  // Change this to your Wi-Fi password
#endif

// it will keep last OnOff & Level state stored, using Preferences
Preferences matterPref;
const char *onOffPrefKey = "OnOff";
const char *levelPrefKey = "Level";

// set your board RGB LED pin here
#ifdef RGB_BUILTIN
const uint8_t pluginPin = RGB_BUILTIN;  // Using built-in RGB LED for visualization
#else
const uint8_t pluginPin = 2;  // Set your pin here if your board has not defined RGB_BUILTIN
#warning "Do not forget to set the RGB LED pin"
#endif

// set your board USER BUTTON pin here
const uint8_t buttonPin = BOOT_PIN;  // Set your pin here. Using BOOT Button.
MatterButton button;

// Set the RGB LED Plugin output based on the current state and level
bool setPluginState(bool state, uint8_t level) {
  Serial.printf("User Callback :: New Plugin State = %s, Level = %u\r\n", state ? "ON" : "OFF", level);
  if (state) {
    // Plugin is ON - set RGB LED level (0-255 maps to 0-100% power)
#ifdef RGB_BUILTIN
    rgbLedWrite(pluginPin, level, level, level);
#else
    analogWrite(pluginPin, level);
#endif
  } else {
    // Plugin is OFF - turn off output
#ifndef RGB_BUILTIN
    // After analogWrite(), it is necessary to set the GPIO to digital mode first
    pinMode(pluginPin, OUTPUT);
#endif
    digitalWrite(pluginPin, LOW);
  }
  // store last Level and OnOff state for when the Plugin is restarted / power goes off
  matterPref.putUChar(levelPrefKey, level);
  matterPref.putBool(onOffPrefKey, state);
  // This callback must return the success state to Matter core
  return true;
}

void setup() {
  // Initialize the USER BUTTON (Boot button) GPIO that will act as a toggle switch
  button.begin(buttonPin);
  // Initialize the RGB LED (plugin) GPIO
  pinMode(pluginPin, OUTPUT);
  digitalWrite(pluginPin, LOW);  // Start with plugin off

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
  // default OnOff state is OFF if not stored before
  bool lastOnOffState = matterPref.getBool(onOffPrefKey, false);
  // default level ~= 25% (64/255)
  uint8_t lastLevel = matterPref.getUChar(levelPrefKey, 64);
  DimmablePlugin.begin(lastOnOffState, lastLevel);
  // set the callback function to handle the Plugin state change
  DimmablePlugin.onChange(setPluginState);

  // lambda functions are used to set the attribute change callbacks
  DimmablePlugin.onChangeOnOff([](bool state) {
    Serial.printf("Plugin OnOff changed to %s\r\n", state ? "ON" : "OFF");
    return true;
  });
  DimmablePlugin.onChangeLevel([](uint8_t level) {
    Serial.printf("Plugin Level changed to %u\r\n", level);
    return true;
  });

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();
  matterWaitUntilReady();
  Serial.printf("Initial state: %s | level: %u\r\n", DimmablePlugin ? "ON" : "OFF", DimmablePlugin.getLevel());
  DimmablePlugin.updateAccessory();
}

void loop() {
  matterRestartIfNoFabric();

  matterButtonEvent_t ev;
  while ((ev = button.poll()) != MATTER_BUTTON_NONE) {
    if (ev == MATTER_BUTTON_CLICK) {
      Serial.println("User button released. Toggling Plugin!");
      DimmablePlugin.toggle();
    } else if (ev == MATTER_BUTTON_LONG_HOLD) {
      Serial.println("Decommissioning the Plugin Matter Accessory. It shall be commissioned again.");
      DimmablePlugin = false;
      Matter.decommission();
    }
  }
}
