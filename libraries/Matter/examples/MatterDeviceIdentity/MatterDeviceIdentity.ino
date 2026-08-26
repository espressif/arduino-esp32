// Copyright 2026 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/License-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing terms and
// limitations under the License.

// Matter node identity and commissioning codes on the Matter singleton.
// Call setters before Matter.begin(). Pairing codes printed after begin() are generated.

#include <Arduino.h>
#include <Matter.h>
#if !CONFIG_ENABLE_CHIPOBLE
#include <WiFi.h>
#endif
#include <Preferences.h>

MatterOnOffLight OnOffLight;

#if !CONFIG_ENABLE_CHIPOBLE
const char *ssid = "your-ssid";
const char *password = "your-password";
#endif

Preferences matterPref;
const char *onOffPrefKey = "OnOff";

#ifdef LED_BUILTIN
const uint8_t ledPin = LED_BUILTIN;
#else
const uint8_t ledPin = 2;
#warning "Do not forget to set the LED pin"
#endif

const uint8_t buttonPin = BOOT_PIN;

uint32_t button_time_stamp = 0;
bool button_state = false;
const uint32_t debouceTime = 250;
const uint32_t decommissioningTimeout = 5000;

bool setLightOnOff(bool state) {
  Serial.printf("User Callback :: New Light State = %s\r\n", state ? "ON" : "OFF");
  digitalWrite(ledPin, state ? HIGH : LOW);
  matterPref.putBool(onOffPrefKey, state);
  return true;
}

static const char *kVendorName = "Espressif";
static const char *kProductName = "KitchenLight";
static const char *kDeviceName = "KitchenHub";
static const char *kSerialNumber = "KH-000123";
static const char *kHardwareVersionString = "RevA";
static const uint16_t kHardwareVersion = 7;
static const uint16_t kSetupDiscriminator = 0xF01;
static const uint32_t kSetupPasscode = 20202024;

void printIdentity() {
  Serial.println("Applied Matter identity (Basic Information / commissioning):");
  Serial.printf("  VendorName:  %s\r\n", kVendorName);
  Serial.printf("  ProductName: %s\r\n", kProductName);
  Serial.printf("  DeviceName:  %s (NodeLabel)\r\n", kDeviceName);
  Serial.printf("  SerialNumber: %s\r\n", kSerialNumber);
  Serial.printf("  HardwareVersion: %u\r\n", kHardwareVersion);
  Serial.printf("  HardwareVersionString: %s\r\n", kHardwareVersionString);
  Serial.printf("  Setup discriminator: 0x%03X\r\n", kSetupDiscriminator);
  Serial.printf("  Setup passcode: %lu\r\n", static_cast<unsigned long>(kSetupPasscode));
  Serial.printf("  Manual pairing code: %s\r\n", Matter.getManualPairingCode().c_str());
  Serial.printf("  QR code URL: %s\r\n", Matter.getOnboardingQRCodeUrl().c_str());
}

// Log CASE session up/down without blocking loop() (button / decommission stay live).
// Sample every 2.5 s — isOnline() takes the CHIP stack lock.
void pollControllerOnline() {
  static bool announced = false;
  static uint32_t lastPollMs = 0;
  const uint32_t now = millis();
  if (lastPollMs != 0 && (now - lastPollMs) < 2500) {
    return;
  }
  lastPollMs = now;
  if (!Matter.isDeviceCommissioned()) {
    announced = false;
    return;
  }
  const bool online = Matter.isOnline();
  if (online && !announced) {
    Serial.println("Matter controller has an active CASE session. Node is online.");
    announced = true;
  } else if (!online && announced) {
    Serial.println("Matter controller CASE session ended.");
    announced = false;
  }
}

void setup() {
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);
  Serial.begin(115200);

#if !CONFIG_ENABLE_CHIPOBLE
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\r\nWi-Fi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  delay(500);
#endif

  // Must be called before Matter.begin(). Late calls log a warning and are ignored.
  Matter.setVendorName(kVendorName);
  Matter.setProductName(kProductName);
  Matter.setDeviceName(kDeviceName);
  Matter.setSerialNumber(kSerialNumber);
  Matter.setHardwareVersion(kHardwareVersion);
  Matter.setHardwareVersionString(kHardwareVersionString);
  // Not the Arduino test pair 0xF00 / 20202021. Use the generated pairing codes below.
  Matter.setSetupDiscriminator(kSetupDiscriminator);
  Matter.setSetupPasscode(kSetupPasscode);

  matterPref.begin("MatterPrefs", false);
  bool lastOnOffState = matterPref.getBool(onOffPrefKey, true);
  OnOffLight.begin(lastOnOffState);
  OnOffLight.onChange(setLightOnOff);

  Matter.begin();
  printIdentity();

  if (Matter.isDeviceCommissioned()) {
    Serial.printf("Initial state: %s\r\n", OnOffLight.getOnOff() ? "ON" : "OFF");
    OnOffLight.updateAccessory();
    Serial.println("Matter Node is commissioned. Light restored from local state.");
  }
}

void loop() {
  if (!Matter.isDeviceCommissioned()) {
    Serial.println("");
    Serial.println("Matter Node is not commissioned yet.");
    Serial.println("Initiate the device discovery in your Matter environment.");
    Serial.println("Commission it using the generated manual pairing code or QR code");
    Serial.printf("Manual pairing code: %s\r\n", Matter.getManualPairingCode().c_str());
    Serial.printf("QR code URL: %s\r\n", Matter.getOnboardingQRCodeUrl().c_str());
    uint32_t timeCount = 0;
    while (!Matter.isDeviceCommissioned()) {
      delay(100);
      if ((timeCount++ % 50) == 0) {
        Serial.println("Matter Node not commissioned yet. Waiting for commissioning.");
      }
    }
    Serial.printf("Initial state: %s\r\n", OnOffLight.getOnOff() ? "ON" : "OFF");
    OnOffLight.updateAccessory();
    Serial.println("Matter Node is commissioned. Applying last local light state.");
  }

  pollControllerOnline();

  if (digitalRead(buttonPin) == LOW && !button_state) {
    button_time_stamp = millis();
    button_state = true;
  }

  uint32_t time_diff = millis() - button_time_stamp;
  if (button_state && time_diff > debouceTime && digitalRead(buttonPin) == HIGH) {
    button_state = false;
    Serial.println("User button released. Toggling Light!");
    OnOffLight.toggle();
  }

  if (button_state && time_diff > decommissioningTimeout) {
    Serial.println("Decommissioning the Light Matter Accessory. It shall be commissioned again.");
    OnOffLight.setOnOff(false);
    Matter.decommission();
    button_time_stamp = millis();
  }
}
