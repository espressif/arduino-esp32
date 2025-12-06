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
#include <Matter.h>
#if !CONFIG_ENABLE_CHIPOBLE
// if the device can be commissioned using BLE, WiFi is not used - save flash space
#include <WiFi.h>
#endif
#include <Preferences.h>

// List of Matter Endpoints for this Node
// Dimmable Plugin Endpoint
MatterDimmablePlugin DimmablePlugin;

// CONFIG_ENABLE_CHIPOBLE is enabled when BLE is used to commission the Matter Network
#if !CONFIG_ENABLE_CHIPOBLE
// WiFi is manually set and started
const char *ssid = "your-ssid";          // Change this to your WiFi SSID
const char *password = "your-password";  // Change this to your WiFi password
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

// Button control
uint32_t button_time_stamp = 0;                // debouncing control
bool button_state = false;                     // false = released | true = pressed
const uint32_t debounceTime = 250;             // button debouncing time (ms)
const uint32_t decommissioningTimeout = 5000;  // keep the button pressed for 5s, or longer, to decommission

// Set the RGB LED Plugin output based on the current state and level
bool setPluginState(bool state, uint8_t level) {
  Serial.printf("User Callback :: New Plugin State = %s, Level = %d\r\n", state ? "ON" : "OFF", level);
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
  pinMode(buttonPin, INPUT_PULLUP);
  // Initialize the RGB LED (plugin) GPIO
  pinMode(pluginPin, OUTPUT);
  digitalWrite(pluginPin, LOW);  // Start with plugin off

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
    Serial.printf("Plugin Level changed to %d\r\n", level);
    return true;
  });

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();
  // This may be a restart of a already commissioned Matter accessory
  if (Matter.isDeviceCommissioned()) {
    Serial.println("Matter Node is commissioned and connected to the network. Ready for use.");
    Serial.printf("Initial state: %s | level: %d\r\n", DimmablePlugin ? "ON" : "OFF", DimmablePlugin.getLevel());
    // configure the Plugin based on initial on-off state and level
    DimmablePlugin.updateAccessory();
  }
}

void loop() {
  // Check Matter Plugin Commissioning state, which may change during execution of loop()
  if (!Matter.isDeviceCommissioned()) {
    Serial.println("");
    Serial.println("Matter Node is not commissioned yet.");
    Serial.println("Initiate the device discovery in your Matter environment.");
    Serial.println("Commission it to your Matter hub with the manual pairing code or QR code");
    Serial.printf("Manual pairing code: %s\r\n", Matter.getManualPairingCode().c_str());
    Serial.printf("QR code URL: %s\r\n", Matter.getOnboardingQRCodeUrl().c_str());
    // waits for Matter Plugin Commissioning.
    uint32_t timeCount = 0;
    while (!Matter.isDeviceCommissioned()) {
      delay(100);
      if ((timeCount++ % 50) == 0) {  // 50*100ms = 5 sec
        Serial.println("Matter Node not commissioned yet. Waiting for commissioning.");
      }
    }
    Serial.printf("Initial state: %s | level: %d\r\n", DimmablePlugin ? "ON" : "OFF", DimmablePlugin.getLevel());
    // configure the Plugin based on initial on-off state and level
    DimmablePlugin.updateAccessory();
    Serial.println("Matter Node is commissioned and connected to the network. Ready for use.");
  }

  // A button is also used to control the plugin
  // Check if the button has been pressed
  if (digitalRead(buttonPin) == LOW && !button_state) {
    // deals with button debouncing
    button_time_stamp = millis();  // record the time while the button is pressed.
    button_state = true;           // pressed.
  }

  // Onboard User Button is used as a Plugin toggle switch or to decommission it
  uint32_t time_diff = millis() - button_time_stamp;
  if (digitalRead(buttonPin) == HIGH && button_state && time_diff > debounceTime) {
    // Toggle button is released - toggle the plugin
    Serial.println("User button released. Toggling Plugin!");
    DimmablePlugin.toggle();  // Matter Controller also can see the change
    button_state = false;     // released
  }

  // Onboard User Button is kept pressed for longer than 5 seconds in order to decommission matter node
  if (button_state && time_diff > decommissioningTimeout) {
    Serial.println("Decommissioning the Plugin Matter Accessory. It shall be commissioned again.");
    DimmablePlugin = false;  // turn the plugin off
    Matter.decommission();
    button_time_stamp = millis();  // avoid running decommissioning again, reboot takes a second or so
  }
}
