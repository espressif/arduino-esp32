// Copyright 2024 Espressif Systems (Shanghai) PTE LTD
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

// Matter Manager
#include <Matter.h>
#include <WiFi.h>
#include <Preferences.h>

// List of Matter Endpoints for this Node
// On/Off Plugin Endpoint
MatterOnOffPlugin OnOffPlugin;

// WiFi is manually set and started
const char *ssid = "your-ssid";          // Change this to your WiFi SSID
const char *password = "your-password";  // Change this to your WiFi password

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

// Button control
uint32_t button_time_stamp = 0;                // debouncing control
bool button_state = false;                     // false = released | true = pressed
const uint32_t decommissioningTimeout = 5000;  // keep the button pressed for 5s, or longer, to decommission

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
  pinMode(buttonPin, INPUT_PULLUP);
  // Initialize the Power Relay (plugin) GPIO
  pinMode(onoffPin, OUTPUT);

  Serial.begin(115200);

  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
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

  // Initialize Matter EndPoint
  matterPref.begin("MatterPrefs", false);
  bool lastOnOffState = matterPref.getBool(onOffPrefKey, false);
  OnOffPlugin.begin(lastOnOffState);
  OnOffPlugin.onChange(setPluginOnOff);

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();
  // This may be a restart of a already commissioned Matter accessory
  if (Matter.isDeviceCommissioned()) {
    Serial.println("Matter Node is commissioned and connected to Wi-Fi. Ready for use.");
    Serial.printf("Initial state: %s\r\n", OnOffPlugin.getOnOff() ? "ON" : "OFF");
    OnOffPlugin.updateAccessory();  // configure the Plugin based on initial state
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
    Serial.printf("Initial state: %s\r\n", OnOffPlugin.getOnOff() ? "ON" : "OFF");
    OnOffPlugin.updateAccessory();  // configure the Plugin based on initial state
    Serial.println("Matter Node is commissioned and connected to Wi-Fi. Ready for use.");
  }

  // Check if the button has been pressed
  if (digitalRead(buttonPin) == LOW && !button_state) {
    // deals with button debouncing
    button_time_stamp = millis();  // record the time while the button is pressed.
    button_state = true;           // pressed.
  }

  // Onboard User Button is used to decommission the Matter Node
  if (button_state && digitalRead(buttonPin) == HIGH) {
    button_state = false;  // released
  }

  // Onboard User Button is kept pressed for longer than 5 seconds in order to decommission matter node
  uint32_t time_diff = millis() - button_time_stamp;
  if (button_state && time_diff > decommissioningTimeout) {
    Serial.println("Decommissioning the Plugin Matter Accessory. It shall be commissioned again.");
    OnOffPlugin.setOnOff(false);  // turn the plugin off
    Matter.decommission();
    button_time_stamp = millis();  // avoid running decommissining again, reboot takes a second or so
  }
}
