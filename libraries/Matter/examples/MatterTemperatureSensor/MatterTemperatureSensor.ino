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
// Matter Temperature Sensor Endpoint
MatterTemperatureSensor SimulatedTemperatureSensor;

// set your board USER BUTTON pin here - decommissioning button
#ifdef BOOT_PIN
const uint8_t buttonPin = BOOT_PIN;  // Set your pin here. Using BOOT Button.
#else
const uint8_t buttonPin = 0;  // Set your button pin here.
#warning "Do not forget to set the USER BUTTON pin"
#endif

// WiFi is manually set and started
const char *ssid = "your-ssid";          // Change this to your WiFi SSID
const char *password = "your-password";  // Change this to your WiFi password

// Simulate a temperature sensor - add your preferred temperature sensor library code here
float getSimulatedTemperature() {
  // The Endpoint implementation keeps an int16_t as internal value information,
  // which stores data in 1/100th of any temperature unit
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

  // set initial temperature sensor measurement
  // Simulated Sensor - it shall initially print -25 degrees and then move to the -10 to 10 range
  SimulatedTemperatureSensor.begin(-25.00);

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
    // waits for Matter Temperature Sensor Commissioning.
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

// Button control - decommision the Matter Node
uint32_t button_time_stamp = 0;                 // debouncing control
bool button_state = false;                      // false = released | true = pressed
const uint32_t decommissioningTimeout = 10000;  // keep the button pressed for 10s to decommission

void loop() {
  Serial.printf("Current Temperature is %.02f <Temperature Units>\r\n", SimulatedTemperatureSensor.getTemperature());

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
  
  // update the temperature sensor value every 5 seconds
  delay(5000);
  // Matter APP shall display the updated temperature
  SimulatedTemperatureSensor.setTemperature(getSimulatedTemperature());  
}
