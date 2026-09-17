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

/*
 * This example is an example code that will create a Matter Device which can be
 * commissioned and controlled from a Matter Environment APP.
 * Additionally the ESP32 will send debug messages indicating the Matter activity.
 * Turning DEBUG Level ON may be useful to following Matter Accessory and Controller messages.
 *
 * The example will create a Matter Water Leak Detector Device.
 * begin() creates the endpoint with StateValue not detected. Call setLeak() after
 * Matter.begin() with the real or simulated sensor reading.
 * The Water Leak Detector state will be indicated by the onboard LED.
 * The Water Leak Detector state will be simulated to change every simulatedSensorInterval.
 *
 * The onboard button can be kept pressed for 5 seconds to decommission the Matter Node.
 * The example will also show the manual commissioning code and QR code to be used in the Matter environment.
 *
 */

// Matter Manager
#include <Arduino.h>
#include <Matter.h>
// List of Matter Endpoints for this Node
// Matter Water Leak Detector Endpoint
MatterWaterLeakDetector WaterLeakDetector;

// Wi-Fi credentials for this sketch. Fill these in when the board cannot
// commission over BLE (Arduino prebuild on ESP32 / ESP32-S2): the sketch
// joins the AP itself. When Matter commissions over BLE (CHIPoBLE), the
// hub sends SSID and password — leave the placeholders; they are unused.
#define WIFI_SSID     "your-ssid"
#define WIFI_PASSWORD "your-password"

// LED will be used to indicate the Water Leak Detector state
// set your board RGB LED pin here
#ifdef RGB_BUILTIN
const uint8_t ledPin = RGB_BUILTIN;
#else
const uint8_t ledPin = 2;  // Set your pin here if the board has no RGB_BUILTIN
#warning "Do not forget to set the RGB LED pin"
#endif

// set your board USER BUTTON pin here - decommissioning only
const uint8_t buttonPin = BOOT_PIN;  // Set your pin here. Using BOOT Button.
MatterButton button;

// Simulated hardware toggles every 20 seconds. Replace simulatedHWWaterLeakDetector() with a real probe read.
const uint32_t simulatedSensorInterval = 20000;

void setup() {
  // Initialize the USER BUTTON (Boot button) that will be used to decommission the Matter Node
  button.begin(buttonPin);
  // Initialize the LED (light) GPIO and Matter End Point
  pinMode(ledPin, OUTPUT);

  Serial.begin(115200);

// CONFIG_ENABLE_CHIPOBLE=n: sketch starts Wi-Fi here; with CHIPoBLE the hub delivers credentials.
#if !CONFIG_ENABLE_CHIPOBLE
  matterConnectWiFi(WIFI_SSID, WIFI_PASSWORD);
#endif

  // Create the endpoint. Fabric StateValue starts false; call setLeak() after Matter.begin().
  WaterLeakDetector.begin();
  digitalWrite(ledPin, LOW);  // LED OFF

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();
  matterWaitUntilReady();
}

bool simulatedHWWaterLeakDetector() {
  // Simulated Water Leak Detector. Replace this body with a real sensor, e.g. return digitalRead(leakPin);
  static bool leakState = false;
  static uint32_t lastTime = millis();

  if (millis() - lastTime > simulatedSensorInterval) {
    leakState = !leakState;
    lastTime = millis();
  }
  return leakState;
}

void loop() {
  matterRestartIfNoFabric();

  matterButtonEvent_t ev;
  while ((ev = button.poll()) != MATTER_BUTTON_NONE) {
    if (ev == MATTER_BUTTON_LONG_HOLD) {
      Serial.println("Decommissioning Water Leak Detector Matter Accessory. It shall be commissioned again.");
      Matter.decommission();
    }
  }

  // Report simulated (or real) hardware to Matter. First call after Matter.begin() applies the current reading.
  bool previous = WaterLeakDetector.getLeak();
  WaterLeakDetector.setLeak(simulatedHWWaterLeakDetector());
  if (WaterLeakDetector.getLeak() != previous) {
    Serial.printf("Water Leak Detector is %s.\r\n", WaterLeakDetector ? "Detected" : "Not Detected");
  }
  digitalWrite(ledPin, WaterLeakDetector ? HIGH : LOW);

  delay(50);
}
