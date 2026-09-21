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

// Matter node identity and commissioning codes on the Matter singleton.
// Call setters before Matter.begin(). Pairing codes printed after begin() are generated.

#include <Arduino.h>
#include <Matter.h>
#include <Preferences.h>

MatterOnOffLight OnOffLight;

// Wi-Fi credentials for this sketch. Fill these in when the board cannot
// commission over BLE (Arduino prebuild on ESP32 / ESP32-S2): the sketch
// joins the AP itself. When Matter commissions over BLE (CHIPoBLE), the
// hub sends SSID and password — leave the placeholders; they are unused.
#define WIFI_SSID     "your-ssid"
#define WIFI_PASSWORD "your-password"

Preferences matterPref;
const char *onOffPrefKey = "OnOff";

#ifdef LED_BUILTIN
const uint8_t ledPin = LED_BUILTIN;
#else
const uint8_t ledPin = 2;
#warning "Do not forget to set the LED pin"
#endif

const uint8_t buttonPin = BOOT_PIN;
MatterButton button;

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
static const char *kSoftwareVersionString = "1.0.7";
static const uint16_t kHardwareVersion = 7;
static const uint32_t kSoftwareVersion = 7;
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
  Serial.printf("  SoftwareVersion: %lu\r\n", static_cast<unsigned long>(kSoftwareVersion));
  Serial.printf("  SoftwareVersionString: %s\r\n", kSoftwareVersionString);
  Serial.printf("  Setup discriminator: 0x%03X\r\n", kSetupDiscriminator);
  Serial.printf("  Setup passcode: %lu\r\n", static_cast<unsigned long>(kSetupPasscode));
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
  button.begin(buttonPin);
  pinMode(ledPin, OUTPUT);
  Serial.begin(115200);

#if !CONFIG_ENABLE_CHIPOBLE
  matterConnectWiFi(WIFI_SSID, WIFI_PASSWORD);
#endif

  // Other examples use matterSetExampleIdentity("Color Light") for vendor Espressif
  // and product "<SoC> <endpoint>". This sketch sets custom values.
  // Must be called before Matter.begin(). Late calls log a warning and are ignored.
  Matter.setVendorName(kVendorName);
  Matter.setProductName(kProductName);
  Matter.setDeviceName(kDeviceName);
  Matter.setSerialNumber(kSerialNumber);
  Matter.setHardwareVersion(kHardwareVersion);
  Matter.setHardwareVersionString(kHardwareVersionString);
  Matter.setSoftwareVersion(kSoftwareVersion);
  Matter.setSoftwareVersionString(kSoftwareVersionString);
  // Not the Arduino test pair 0xF00 / 20202021. Live pairing codes come from matterWaitUntilReady().
  Matter.setSetupDiscriminator(kSetupDiscriminator);
  Matter.setSetupPasscode(kSetupPasscode);

  matterPref.begin("MatterPrefs", false);
  bool lastOnOffState = matterPref.getBool(onOffPrefKey, true);
  OnOffLight.begin(lastOnOffState);
  OnOffLight.onChange(setLightOnOff);

  Matter.begin();
  matterWaitUntilReady();
  printIdentity();
  Serial.printf("Initial state: %s\r\n", OnOffLight.getOnOff() ? "ON" : "OFF");
  OnOffLight.updateAccessory();
}

void loop() {
  matterRestartIfNoFabric();

  pollControllerOnline();

  matterButtonEvent_t ev;
  while ((ev = button.poll()) != MATTER_BUTTON_NONE) {
    if (ev == MATTER_BUTTON_CLICK) {
      Serial.println("User button released. Toggling Light!");
      OnOffLight.toggle();
    } else if (ev == MATTER_BUTTON_LONG_HOLD) {
      Serial.println("Decommissioning the Light Matter Accessory. It shall be commissioned again.");
      OnOffLight.setOnOff(false);
      Matter.decommission();
    }
  }
}
