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
 * Smallest Matter sketch: one On/Off Light, an LED GPIO, and onChange().
 * Commission from the pairing code that matterWaitUntilReady() prints.
 * No button, no matterSetExampleIdentity(), no extra serial after CASE.
 */

#include <Arduino.h>
#include <Matter.h>
MatterOnOffLight OnOffLight;

// Wi-Fi credentials for this sketch. Fill these in when the board cannot
// commission over BLE (Arduino prebuild on ESP32 / ESP32-S2): the sketch
// joins the AP itself. When Matter commissions over BLE (CHIPoBLE), the
// hub sends SSID and password — leave the placeholders; they are unused.
#define WIFI_SSID     "your-ssid"
#define WIFI_PASSWORD "your-password"

#ifdef LED_BUILTIN
const uint8_t ledPin = LED_BUILTIN;
#else
const uint8_t ledPin = 2;  // Set your pin if the board has no LED_BUILTIN
#endif

// Matter Protocol Endpoint (On/OFF Light) Callback
bool onOffLightCallback(bool state) {
  digitalWrite(ledPin, state ? HIGH : LOW);
  // This callback must return the success state to Matter core
  return true;
}

void setup() {
  Serial.begin(115200);

  // Initialize the LED GPIO
  pinMode(ledPin, OUTPUT);

// CONFIG_ENABLE_CHIPOBLE=n: sketch starts Wi-Fi here; with CHIPoBLE the hub delivers credentials.
#if !CONFIG_ENABLE_CHIPOBLE
  matterConnectWiFi(WIFI_SSID, WIFI_PASSWORD);
#endif

  // Initialize at least one Matter EndPoint
  OnOffLight.begin();

  // Associate a callback to the Matter Controller
  OnOffLight.onChange(onOffLightCallback);

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();
  matterWaitUntilReady();
}

void loop() {
  // Reboot if the hub removed the fabric. This sketch has no decommission button.
  matterRestartIfNoFabric();
  delay(500);
}
