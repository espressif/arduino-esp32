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

// Matter Status Example
// This example demonstrates how to check enabled Matter features and connectivity status
// It implements a basic on/off light and reports capability and connection status

#include <Arduino.h>
#include <Matter.h>
// Wi-Fi credentials for this sketch. Fill these in when the board cannot
// commission over BLE (Arduino prebuild on ESP32 / ESP32-S2): the sketch
// joins the AP itself. When Matter commissions over BLE (CHIPoBLE), the
// hub sends SSID and password — leave the placeholders; they are unused.
#define WIFI_SSID     "your-ssid"
#define WIFI_PASSWORD "your-password"

// List of Matter Endpoints for this Node
// On/Off Light Endpoint
MatterOnOffLight OnOffLight;

// set your board LED pin here
#ifdef LED_BUILTIN
const uint8_t ledPin = LED_BUILTIN;
#else
const uint8_t ledPin = 2;  // Set your pin here if your board has not defined LED_BUILTIN
#warning "Do not forget to set the LED pin"
#endif

// Matter Protocol Endpoint Callback
bool setLightOnOff(bool state) {
  Serial.printf("User Callback :: New Light State = %s\r\n", state ? "ON" : "OFF");
  if (state) {
    digitalWrite(ledPin, HIGH);
  } else {
    digitalWrite(ledPin, LOW);
  }
  // This callback must return the success state to Matter core
  return true;
}

void setup() {
  // Initialize the LED (light) GPIO
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);  // Start with light OFF

  Serial.begin(115200);
  delay(1000);
  Serial.println("\n========================================");
  Serial.println("Matter Status Example");
  Serial.println("========================================\n");

  // Report enabled features
  Serial.println("=== Enabled Features ===");
  Serial.printf("Wi-Fi Station Enabled: %s\r\n", Matter.isWiFiStationEnabled() ? "YES" : "NO");
  Serial.printf("Wi-Fi Access Point Enabled: %s\r\n", Matter.isWiFiAccessPointEnabled() ? "YES" : "NO");
  Serial.printf("Thread Enabled: %s\r\n", Matter.isThreadEnabled() ? "YES" : "NO");
  Serial.printf("BLE Commissioning Enabled: %s\r\n", Matter.isBLECommissioningEnabled() ? "YES" : "NO");
  Serial.printf("BLE Memory Release Enabled: %s\r\n", Matter.isBLEMemoryReleaseEnabled() ? "YES" : "NO");
  Serial.println();

// Connect Wi-Fi in setup() only when this build has no CHIPoBLE.
#if !CONFIG_ENABLE_CHIPOBLE
  if (Matter.isWiFiStationEnabled()) {
    matterConnectWiFi(WIFI_SSID, WIFI_PASSWORD);
  }
#endif

  // Initialize On/Off Light endpoint
  OnOffLight.begin(false);  // Start with light OFF
  OnOffLight.onChange(setLightOnOff);

  // Start Matter
  matterSetExampleIdentity("OnOff Light");
  Matter.begin();
  matterWaitUntilReady();
  Serial.println("Matter started");
}

void loop() {
  matterRestartIfNoFabric();

  static bool lastCommissioned = false;
  static bool lastConnected = false;
  static bool lastOnline = false;
  static uint32_t lastOnlinePollMs = 0;
  static uint32_t lastStatusPollMs = 0;

  const uint32_t now = millis();

  // CASE session is sampled every 2.5 s. The LED is not gated on isOnline().
  if (lastOnlinePollMs == 0 || (now - lastOnlinePollMs) >= 2500) {
    lastOnlinePollMs = now;
    const bool online = Matter.isOnline();
    if (online != lastOnline) {
      Serial.printf("State change: Online=%s\r\n", online ? "YES" : "NO");
      lastOnline = online;
    }
  }

  // Commissioned / connected / radios every 5 s.
  if (lastStatusPollMs == 0 || (now - lastStatusPollMs) >= 5000) {
    lastStatusPollMs = now;
    const bool commissioned = Matter.isDeviceCommissioned();
    const bool connected = Matter.isDeviceConnected();
    if (commissioned != lastCommissioned || connected != lastConnected) {
      Serial.printf("State change: Commissioned=%s Connected=%s\r\n", commissioned ? "YES" : "NO", connected ? "YES" : "NO");
      lastCommissioned = commissioned;
      lastConnected = connected;
    }

    Serial.println("=== Connection Status ===");
    Serial.printf("Wi-Fi Connected: %s\r\n", Matter.isWiFiConnected() ? "YES" : "NO");
    Serial.printf("Thread Connected: %s\r\n", Matter.isThreadConnected() ? "YES" : "NO");
    Serial.printf("Device Connected: %s\r\n", connected ? "YES" : "NO");
    Serial.printf("Device Commissioned: %s\r\n", commissioned ? "YES" : "NO");
    Serial.printf("Device Online (CASE): %s\r\n", lastOnline ? "YES" : "NO");
    Serial.println();
  }

  delay(50);
}
