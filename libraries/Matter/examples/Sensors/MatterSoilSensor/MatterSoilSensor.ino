// Copyright 2026 Espressif Systems (Shanghai) PTE LTD
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

// Matter Soil Sensor Endpoint
MatterSoilSensor SoilSensor;

// Wi-Fi credentials for this sketch. Fill these in when the board cannot
// commission over BLE (Arduino prebuild on ESP32 / ESP32-S2): the sketch
// joins the AP itself. When Matter commissions over BLE (CHIPoBLE), the
// hub sends SSID and password — leave the placeholders; they are unused.
#define WIFI_SSID     "your-ssid"
#define WIFI_PASSWORD "your-password"

// set your board USER BUTTON pin here - decommissioning button
const uint8_t buttonPin = BOOT_PIN;  // Set your pin here. Using BOOT Button.
MatterButton button;

// Simulate a soil moisture sensor - add your preferred soil moisture sensor library code here
uint8_t getSimulatedSoilMoisture() {
  // The Soil Measurement cluster only reports whole percent [0..100]
  static uint8_t simulatedSoilMoistureHWSensor = 20;

  simulatedSoilMoistureHWSensor++;
  if (simulatedSoilMoistureHWSensor > 60) {
    simulatedSoilMoistureHWSensor = 20;
  }

  return simulatedSoilMoistureHWSensor;
}

void setup() {
  button.begin(buttonPin);

  Serial.begin(115200);

#if !CONFIG_ENABLE_CHIPOBLE
  matterConnectWiFi(WIFI_SSID, WIFI_PASSWORD);
#endif

  // Live Soil Measurement cluster exists only after Matter.begin(); set readings after start.
  SoilSensor.begin();

  matterSetExampleIdentity("Soil Sensor");
  Matter.begin();
  matterWaitUntilReady();

  SoilSensor.setSoilMoisture(getSimulatedSoilMoisture());
}

void loop() {
  matterRestartIfNoFabric();

  static uint32_t timeCounter = 0;

  if (!(timeCounter++ % 10)) {  // 500 ms x 10 = 5 s
    Serial.printf("Current Soil Moisture is %u%%\r\n", SoilSensor.getSoilMoisture());
    SoilSensor.setSoilMoisture(getSimulatedSoilMoisture());
  }

  matterButtonEvent_t ev;
  while ((ev = button.poll()) != MATTER_BUTTON_NONE) {
    if (ev == MATTER_BUTTON_LONG_HOLD) {
      Serial.println("Decommissioning Soil Sensor Matter Accessory. It shall be commissioned again.");
      Matter.decommission();
    }
  }

  delay(500);
}
