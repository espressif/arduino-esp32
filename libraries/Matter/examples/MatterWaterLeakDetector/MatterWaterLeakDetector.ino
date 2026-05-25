// Copyright 2025 Espressif Systems (Shanghai) PTE LTD
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
 * This example is an example code that will create a Matter Device which can be
 * commissioned and controlled from a Matter Environment APP.
 * Additionally the ESP32 will send debug messages indicating the Matter activity.
 * Turning DEBUG Level ON may be useful to following Matter Accessory and Controller messages.
 *
 * The example will create a Matter Water Leak Detector Device.
 * The Water Leak Detector state can be toggled by pressing the onboard button.
 * The Water Leak Detector state will be indicated by the onboard LED.
 * The Water Leak Detector state will be simulated to change every 20 seconds.
 *
 * The onboard button can be kept pressed for 5 seconds to decommission the Matter Node.
 * The example will also show the manual commissioning code and QR code to be used in the Matter environment.
 *
 */

// Matter Manager
#include <Arduino.h>
#include <Matter.h>
#if !CONFIG_ENABLE_CHIPOBLE
// if the device can be commissioned using BLE, WiFi is not used - save flash space
#include <WiFi.h>
#endif

// List of Matter Endpoints for this Node
// Matter Water Leak Detector Endpoint
MatterWaterLeakDetector WaterLeakDetector;

// CONFIG_ENABLE_CHIPOBLE is enabled when BLE is used to commission the Matter Network
#if !CONFIG_ENABLE_CHIPOBLE
// WiFi is manually set and started
const char *ssid = "your-ssid";          // Change this to your WiFi SSID
const char *password = "your-password";  // Change this to your WiFi password
#endif

// LED will be used to indicate the Water Leak Detector state
// set your board RGB LED pin here
#ifdef RGB_BUILTIN
const uint8_t ledPin = RGB_BUILTIN;
#else
const uint8_t ledPin = 2;  // Set your pin here if your board has not defined LED_BUILTIN
#warning "Do not forget to set the RGB LED pin"
#endif

// set your board USER BUTTON pin here - decommissioning and Manual Water Leak Detector toggle button
const uint8_t buttonPin = BOOT_PIN;  // Set your pin here. Using BOOT Button.

// Button control
uint32_t button_time_stamp = 0;                // debouncing control
bool button_state = false;                     // false = released | true = pressed
const uint32_t debounceTime = 250;             // button debouncing time (ms)
const uint32_t decommissioningTimeout = 5000;  // keep the button pressed for 5s, or longer, to decommission

void setup() {
  // Initialize the USER BUTTON (Boot button) that will be used to decommission the Matter Node
  // The button will also be used to manually toggle the Water Leak Detector state
  pinMode(buttonPin, INPUT_PULLUP);
  // Initialize the LED (light) GPIO and Matter End Point
  pinMode(ledPin, OUTPUT);

  Serial.begin(115200);

// CONFIG_ENABLE_CHIPOBLE is enabled when BLE is used to commission the Matter Network
#if !CONFIG_ENABLE_CHIPOBLE
  // Manually connect to WiFi
  WiFi.begin(ssid, password);
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
#endif

  // set initial water leak detector state as false (default)
  WaterLeakDetector.begin();
  digitalWrite(ledPin, LOW);  // LED OFF

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();

  // Check Matter Accessory Commissioning state, which may change during execution of loop()
  if (!Matter.isDeviceCommissioned()) {
    Serial.println("");
    Serial.println("Matter Node is not commissioned yet.");
    Serial.println("Initiate the device discovery in your Matter environment.");
    Serial.println("Commission it to your Matter hub with the manual pairing code or QR code");
    Serial.printf("Manual pairing code: %s\r\n", Matter.getManualPairingCode().c_str());
    Serial.printf("QR code URL: %s\r\n", Matter.getOnboardingQRCodeUrl().c_str());
    // waits for Matter Water Leak Detector Commissioning.
    uint32_t timeCount = 0;
    while (!Matter.isDeviceCommissioned()) {
      delay(100);
      if ((timeCount++ % 50) == 0) {  // 50*100ms = 5 sec
        Serial.println("Matter Node not commissioned yet. Waiting for commissioning.");
      }
    }
    Serial.println("Matter Node is commissioned and connected to the network. Ready for use.");
  }
}

bool simulatedHWWaterLeakDetector() {
  // Simulated Water Leak Detector
  static bool leakState = false;
  static uint32_t lastTime = 0;

  // Simulate a Water Leak Detector state change every 20 seconds
  if (millis() - lastTime > 20000) {
    leakState = !leakState;
    lastTime = millis();
  }
  return leakState;
}

void loop() {
  // Check if the button has been pressed
  if (digitalRead(buttonPin) == LOW && !button_state) {
    // deals with button debouncing
    button_time_stamp = millis();  // record the time while the button is pressed.
    button_state = true;           // pressed.
  }

  uint32_t time_diff = millis() - button_time_stamp;
  if (button_state && time_diff > debounceTime && digitalRead(buttonPin) == HIGH) {
    button_state = false;  // released
    // button is released - toggle Leak State (Not Detected/Detected)
    WaterLeakDetector.setLeak(!WaterLeakDetector.getLeak());  // same as WaterLeakDetector = !WaterLeakDetector;
    Serial.printf("User button released. Setting the Water Leak Detector to %s.\r\n", WaterLeakDetector ? "Detected" : "Not Detected");
    // LED will indicate the Water Leak Detector state
    if (WaterLeakDetector) {
      digitalWrite(ledPin, HIGH);  // LED ON
    } else {
      digitalWrite(ledPin, LOW);  // LED OFF
    }
  }

  // Onboard User Button is kept pressed for longer than 5 seconds in order to decommission matter node
  if (button_state && time_diff > decommissioningTimeout) {
    Serial.println("Decommissioning Water Leak Detector Matter Accessory. It shall be commissioned again.");
    Matter.decommission();
    button_time_stamp = millis();  // avoid running decommissioning again, reboot takes a second or so
  }

  // Simulated Water Leak Detector
  WaterLeakDetector.setLeak(simulatedHWWaterLeakDetector());

  delay(50);
}
