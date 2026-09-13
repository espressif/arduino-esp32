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
 */

// Matter Manager
#include <Arduino.h>
#include <Matter.h>
#if !CONFIG_ENABLE_CHIPOBLE
// WiFi.h / WiFi.begin() only when this build has no CHIPoBLE (CONFIG_ENABLE_CHIPOBLE=n). Hub-delivered Wi-Fi still uses CHIP's stack.
#include <WiFi.h>
#endif

// List of Matter Endpoints for this Node
// Matter Temperature Sensor Endpoint
MatterTemperatureSensor SimulatedTemperatureSensor;

// CONFIG_ENABLE_CHIPOBLE=n: sketch starts Wi-Fi here; with CHIPoBLE the hub delivers credentials.
#if !CONFIG_ENABLE_CHIPOBLE
// Wi-Fi is manually set and started
const char *ssid = "your-ssid";          // Change this to your Wi-Fi SSID
const char *password = "your-password";  // Change this to your Wi-Fi password
#endif

// set your board USER BUTTON pin here - decommissioning button
const uint8_t buttonPin = BOOT_PIN;  // Set your pin here. Using BOOT Button.
MatterButton button;

// Simulate a temperature sensor - add your preferred temperature sensor library code here
float getSimulatedTemperature() {
  // The Endpoint implementation keeps an int16_t as internal value information,
  // which stores data in 1/100th Celsius.
  static float simulatedTempHWSensor = -10.0;

  // it will increase from -10C to 10C in 0.5C steps to simulate a temperature sensor
  simulatedTempHWSensor = simulatedTempHWSensor + 0.5;
  if (simulatedTempHWSensor > 10) {
    simulatedTempHWSensor = -10;
  }

  return simulatedTempHWSensor;
}

void setup() {
  // Initialize the USER BUTTON (Boot button) that will be used to decommission the Matter Node
  button.begin(buttonPin);

  Serial.begin(115200);

// CONFIG_ENABLE_CHIPOBLE=n: sketch starts Wi-Fi here; with CHIPoBLE the hub delivers credentials.
#if !CONFIG_ENABLE_CHIPOBLE
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("Wi-Fi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
#endif

  // set initial temperature sensor measurement
  // Simulated Sensor - it shall initially print -25C and then move to the -10C to 10C range
  SimulatedTemperatureSensor.begin(-25.00);

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();
  matterWaitUntilReady();

}

void loop() {
  matterRestartIfNoFabric();

  static uint32_t timeCounter = 0;

  // Print the current temperature value every 5s
  if (!(timeCounter++ % 10)) {  // delaying for 500ms x 10 = 5s
    // Print the current temperature value
    Serial.printf("Current Temperature is %.02fC\r\n", SimulatedTemperatureSensor.getTemperature());
    // Update Temperature from the (Simulated) Hardware Sensor
    // Matter APP shall display the updated temperature.
    SimulatedTemperatureSensor.setTemperature(getSimulatedTemperature());
  }

  matterButtonEvent_t ev;
  while ((ev = button.poll()) != MATTER_BUTTON_NONE) {
    if (ev == MATTER_BUTTON_LONG_HOLD) {
      Serial.println("Decommissioning Temperature Sensor Matter Accessory. It shall be commissioned again.");
      Matter.decommission();
    }
  }

  delay(500);
}
