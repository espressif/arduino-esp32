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
 * The example will create a Matter Water Freeze Detector Device.
 * begin() creates the endpoint with StateValue not detected. Call setFreeze() after
 * Matter.begin() with the real or simulated sensor reading.
 * The Water Freeze Detector state will be indicated by the onboard LED.
 * The Water Freeze Detector state will be simulated to change every simulatedSensorInterval.
 *
 * The onboard button can be kept pressed for 5 seconds to decommission the Matter Node.
 * The example will also show the manual commissioning code and QR code to be used in the Matter environment.
 *
 */

// Matter Manager
#include <Arduino.h>
#include <Matter.h>
#if !CONFIG_ENABLE_CHIPOBLE
// WiFi.h / WiFi.begin() only when this build has no CHIPoBLE (CONFIG_ENABLE_CHIPOBLE=n). Hub-delivered Wi-Fi still uses CHIP's stack.
#include <WiFi.h>
#endif

// List of Matter Endpoints for this Node
// Matter Water Freeze Detector Endpoint
MatterWaterFreezeDetector WaterFreezeDetector;

// CONFIG_ENABLE_CHIPOBLE=n: sketch starts Wi-Fi here; with CHIPoBLE the hub delivers credentials.
#if !CONFIG_ENABLE_CHIPOBLE
// Wi-Fi is manually set and started
const char *ssid = "your-ssid";          // Change this to your Wi-Fi SSID
const char *password = "your-password";  // Change this to your Wi-Fi password
#endif

// LED will be used to indicate the Water Freeze Detector state
// set your board RGB LED pin here
#ifdef RGB_BUILTIN
const uint8_t ledPin = RGB_BUILTIN;
#else
const uint8_t ledPin = 2;  // Set your pin here if the board has no RGB_BUILTIN
#warning "Do not forget to set the RGB LED pin"
#endif

// set your board USER BUTTON pin here - decommissioning only
const uint8_t buttonPin = BOOT_PIN;  // Set your pin here. Using BOOT Button.

// Button control
uint32_t button_time_stamp = 0;                // debouncing control
bool button_state = false;                     // false = released | true = pressed
const uint32_t decommissioningTimeout = 5000;  // keep the button pressed for 5s, or longer, to decommission

// Simulated hardware toggles every 20 seconds. Replace simulatedHWWaterFreezeDetector() with a real probe read.
const uint32_t simulatedSensorInterval = 20000;

void setup() {
  // Initialize the USER BUTTON (Boot button) that will be used to decommission the Matter Node
  pinMode(buttonPin, INPUT_PULLUP);
  // Initialize the LED (light) GPIO and Matter End Point
  pinMode(ledPin, OUTPUT);

  Serial.begin(115200);

// CONFIG_ENABLE_CHIPOBLE=n: sketch starts Wi-Fi here; with CHIPoBLE the hub delivers credentials.
#if !CONFIG_ENABLE_CHIPOBLE
  // Manually connect to Wi-Fi
  WiFi.begin(ssid, password);
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
#endif

  // Create the endpoint. Fabric StateValue starts false; call setFreeze() after Matter.begin().
  WaterFreezeDetector.begin();
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
    // waits for Matter Water Freeze Detector Commissioning.
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

bool simulatedHWWaterFreezeDetector() {
  // Simulated Water Freeze Detector. Replace this body with a real sensor, e.g. return digitalRead(freezePin);
  static bool freezeState = false;
  static uint32_t lastTime = millis();

  if (millis() - lastTime > simulatedSensorInterval) {
    freezeState = !freezeState;
    lastTime = millis();
  }
  return freezeState;
}

void loop() {
  // Check if the button has been pressed
  if (digitalRead(buttonPin) == LOW && !button_state) {
    // deals with button debouncing
    button_time_stamp = millis();  // record the time while the button is pressed.
    button_state = true;           // pressed.
  }

  if (button_state && digitalRead(buttonPin) == HIGH) {
    button_state = false;  // released
  }

  // Onboard User Button is kept pressed for longer than 5 seconds in order to decommission matter node
  uint32_t time_diff = millis() - button_time_stamp;
  if (button_state && time_diff > decommissioningTimeout) {
    Serial.println("Decommissioning Water Freeze Detector Matter Accessory. It shall be commissioned again.");
    Matter.decommission();
    button_time_stamp = millis();  // avoid running decommissioning again, reboot takes a second or so
  }

  // Report simulated (or real) hardware to Matter. First call after Matter.begin() applies the current reading.
  bool previous = WaterFreezeDetector.getFreeze();
  WaterFreezeDetector.setFreeze(simulatedHWWaterFreezeDetector());
  if (WaterFreezeDetector.getFreeze() != previous) {
    Serial.printf("Water Freeze Detector is %s.\r\n", WaterFreezeDetector ? "Detected" : "Not Detected");
  }
  digitalWrite(ledPin, WaterFreezeDetector ? HIGH : LOW);

  delay(50);
}
