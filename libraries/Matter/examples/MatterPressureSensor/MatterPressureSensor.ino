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
 * This example is an example code that will create a Matter Device which can be
 * commissioned and controlled from a Matter Environment APP.
 * Additionally the ESP32 will send debug messages indicating the Matter activity.
 * Turning DEBUG Level ON may be useful to following Matter Accessory and Controller messages.
 */

// Matter Manager
#include <Matter.h>
#include <WiFi.h>

// List of Matter Endpoints for this Node
// Matter Pressure Sensor Endpoint
MatterPressureSensor SimulatedPressureSensor;

// WiFi is manually set and started
const char *ssid = "your-ssid";          // Change this to your WiFi SSID
const char *password = "your-password";  // Change this to your WiFi password

// set your board USER BUTTON pin here - decommissioning button
const uint8_t buttonPin = BOOT_PIN;  // Set your pin here. Using BOOT Button.

// Button control - decommision the Matter Node
uint32_t button_time_stamp = 0;                // debouncing control
bool button_state = false;                     // false = released | true = pressed
const uint32_t decommissioningTimeout = 5000;  // keep the button pressed for 5s, or longer, to decommission

// Simulate a pressure sensor - add your preferred pressure sensor library code here
float getSimulatedPressure() {
  // The Endpoint implementation keeps an uint16_t as internal value information,
  // which stores data in hPa (pressure measurement unit)
  static float simulatedPressureHWSensor = 950;

  // it will increase from 950 to 1100 hPa in steps of 10 hPa to simulate a pressure sensor
  simulatedPressureHWSensor = simulatedPressureHWSensor + 10;
  if (simulatedPressureHWSensor > 1100) {
    simulatedPressureHWSensor = 950;
  }

  return simulatedPressureHWSensor;
}

void setup() {
  // Initialize the USER BUTTON (Boot button) that will be used to decommission the Matter Node
  pinMode(buttonPin, INPUT_PULLUP);

  Serial.begin(115200);

  // Manually connect to WiFi
  WiFi.begin(ssid, password);
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  // set initial pressure sensor measurement
  // Simulated Sensor - it shall initially print 900hPa and then move to the 950 to 1100 hPa as pressure range
  SimulatedPressureSensor.begin(900.00);

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
    // waits for Matter Pressure Sensor Commissioning.
    uint32_t timeCount = 0;
    while (!Matter.isDeviceCommissioned()) {
      delay(100);
      if ((timeCount++ % 50) == 0) {  // 50*100ms = 5 sec
        Serial.println("Matter Node not commissioned yet. Waiting for commissioning.");
      }
    }
    Serial.println("Matter Node is commissioned and connected to Wi-Fi. Ready for use.");
  }
}

void loop() {
  static uint32_t timeCounter = 0;

  // Print the current pressure value every 5s
  if (!(timeCounter++ % 10)) {  // delaying for 500ms x 10 = 5s
    // Print the current pressure value
    Serial.printf("Current Pressure is %.02fhPa\r\n", SimulatedPressureSensor.getPressure());
    // Update Pressure from the (Simulated) Hardware Sensor
    // Matter APP shall display the updated pressure percent
    SimulatedPressureSensor.setPressure(getSimulatedPressure());
  }

  // Check if the button has been pressed
  if (digitalRead(buttonPin) == LOW && !button_state) {
    // deals with button debouncing
    button_time_stamp = millis();  // record the time while the button is pressed.
    button_state = true;           // pressed.
  }

  if (digitalRead(buttonPin) == HIGH && button_state) {
    button_state = false;  // released
  }

  // Onboard User Button is kept pressed for longer than 5 seconds in order to decommission matter node
  uint32_t time_diff = millis() - button_time_stamp;
  if (button_state && time_diff > decommissioningTimeout) {
    // Factory reset is triggered if the button is pressed longer than 10 seconds
    Serial.println("Decommissioning the Light Matter Accessory. It shall be commissioned again.");
    Matter.decommission();
  }

  delay(500);
}
