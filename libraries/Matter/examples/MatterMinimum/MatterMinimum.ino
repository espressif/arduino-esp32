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

/*
 * This example is the smallest code that will create a Matter Device which can be
 * commissioned and controlled from a Matter Environment APP.
 * It controls a GPIO that could be attached to a LED for visualization.
 * Additionally the ESP32 will send debug messages indicating the Matter activity.
 * Turning DEBUG Level ON may be useful to following Matter Accessory and Controller messages.
 */

// Matter Manager
#include <Matter.h>
#include <WiFi.h>

// List of Matter Endpoints for this Node
// Single On/Off Light Endpoint - at least one per node
MatterOnOffLight OnOffLight;

// Light GPIO that can be controlled by Matter APP
#ifdef LED_BUILTIN
const uint8_t ledPin = LED_BUILTIN;
#else
const uint8_t ledPin = 2;  // Set your pin here if your board has not defined LED_BUILTIN
#endif

// set your board USER BUTTON pin here
#ifdef BOOT_PIN
const uint8_t buttonPin = BOOT_PIN;  // Set your pin here. Using BOOT Button.
#else
const uint8_t buttonPin = 0;  // Set your button pin here.
#warning "Do not forget to set the USER BUTTON pin"
#endif

// Matter Protocol Endpoint (On/OFF Light) Callback
bool matterCB(bool state) {
  digitalWrite(ledPin, state ? HIGH : LOW);
  // This callback must return the success state to Matter core
  return true;
}

// WiFi is manually set and started
const char *ssid = "your-ssid";          // Change this to your WiFi SSID
const char *password = "your-password";  // Change this to your WiFi password

void setup() {
  // Initialize the USER BUTTON (Boot button) that will be used to decommission the Matter Node
  pinMode(buttonPin, INPUT_PULLUP);
  // Initialize the LED GPIO
  pinMode(ledPin, OUTPUT);

  WiFi.enableIPv6(true);
  // Manually connect to WiFi
  WiFi.begin(ssid, password);
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  // Initialize at least one Matter EndPoint
  OnOffLight.begin();

  // Associate a callback to the Matter Controller
  OnOffLight.onChange(matterCB);

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();

  if (!Matter.isDeviceCommissioned()) {
    log_i("Matter Node is not commissioned yet.");
    log_i("Initiate the device discovery in your Matter environment.");
    log_i("Commission it to your Matter hub with the manual pairing code or QR code");
    log_i("Manual pairing code: %s\r\n", Matter.getManualPairingCode().c_str());
    log_i("QR code URL: %s\r\n", Matter.getOnboardingQRCodeUrl().c_str());
  }
}

// Button control - decommision the Matter Node
uint32_t button_time_stamp = 0;                 // debouncing control
bool button_state = false;                      // false = released | true = pressed
const uint32_t decommissioningTimeout = 10000;  // keep the button pressed for 10s to decommission

void loop() {
  // Check if the button has been pressed
  if (digitalRead(buttonPin) == LOW && !button_state) {
    // deals with button debouncing
    button_time_stamp = millis();  // record the time while the button is pressed.
    button_state = true;           // pressed.
  }

  // Onboard User Button is used to decommission matter node
  uint32_t time_diff = millis() - button_time_stamp;
  if (button_state && time_diff > decommissioningTimeout && digitalRead(buttonPin) == HIGH) {
    button_state = false;  // released
    // Factory reset is triggered if the button is pressed longer than 10 seconds
    if (time_diff > decommissioningTimeout) {
      Serial.println("Decommissioning the Light Matter Accessory. It shall be commissioned again.");
      Matter.decommission();
    }
  }

  delay(5000);
}
