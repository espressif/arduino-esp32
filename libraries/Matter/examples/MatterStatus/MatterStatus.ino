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

// Matter Status Example
// This example demonstrates how to check enabled Matter features and connectivity status
// It implements a basic on/off light and reports capability and connection status

#include <Arduino.h>
#include <Matter.h>
// CONFIG_ENABLE_CHIPOBLE is enabled when BLE is used to commission the Matter Network
#if !CONFIG_ENABLE_CHIPOBLE  // ESP32 and ESP32-S2 do not support BLE commissioning
// if the device can be commissioned using BLE, WiFi is not used - save flash space
#include <WiFi.h>
// WiFi is manually set and started
const char *ssid = "your-ssid";          // Change this to your WiFi SSID
const char *password = "your-password";  // Change this to your WiFi password
#endif

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
  Serial.printf("WiFi Station Enabled: %s\r\n", Matter.isWiFiStationEnabled() ? "YES" : "NO");
  Serial.printf("WiFi Access Point Enabled: %s\r\n", Matter.isWiFiAccessPointEnabled() ? "YES" : "NO");
  Serial.printf("Thread Enabled: %s\r\n", Matter.isThreadEnabled() ? "YES" : "NO");
  Serial.printf("BLE Commissioning Enabled: %s\r\n", Matter.isBLECommissioningEnabled() ? "YES" : "NO");
  Serial.println();

// CONFIG_ENABLE_CHIPOBLE is enabled when BLE is used to commission the Matter Network
#if !CONFIG_ENABLE_CHIPOBLE
  // We start by connecting to a WiFi network
  if (Matter.isWiFiStationEnabled()) {
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
#endif

  // Initialize On/Off Light endpoint
  OnOffLight.begin(false);  // Start with light OFF
  OnOffLight.onChange(setLightOnOff);

  // Start Matter
  Matter.begin();
  Serial.println("Matter started");
  Serial.println();

  // Print commissioning information
  Serial.println("========================================");
  Serial.println("Matter Node is not commissioned yet.");
  Serial.println("Initiate the device discovery in your Matter environment.");
  Serial.println("Commission it to your Matter hub with the manual pairing code or QR code");
  Serial.printf("Manual pairing code: %s\r\n", Matter.getManualPairingCode().c_str());
  Serial.printf("QR code URL: %s\r\n", Matter.getOnboardingQRCodeUrl().c_str());
  Serial.println("========================================\n");
}

void loop() {
  // Report connection status every 10 seconds
  Serial.println("=== Connection Status ===");
  Serial.printf("WiFi Connected: %s\r\n", Matter.isWiFiConnected() ? "YES" : "NO");
  Serial.printf("Thread Connected: %s\r\n", Matter.isThreadConnected() ? "YES" : "NO");
  Serial.printf("Device Connected: %s\r\n", Matter.isDeviceConnected() ? "YES" : "NO");
  Serial.printf("Device Commissioned: %s\r\n", Matter.isDeviceCommissioned() ? "YES" : "NO");
  Serial.println();
  delay(10000);
}
