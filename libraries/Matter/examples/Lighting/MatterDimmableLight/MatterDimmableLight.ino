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

// Matter Manager
#include <Arduino.h>
#include <Matter.h>
#include <Preferences.h>

// List of Matter Endpoints for this Node
// Dimmable Light Endpoint
MatterDimmableLight DimmableLight;

// Wi-Fi credentials for this sketch. Fill these in when the board cannot
// commission over BLE (Arduino prebuild on ESP32 / ESP32-S2): the sketch
// joins the AP itself. When Matter commissions over BLE (CHIPoBLE), the
// hub sends SSID and password — leave the placeholders; they are unused.
#define WIFI_SSID     "your-ssid"
#define WIFI_PASSWORD "your-password"

// it will keep last OnOff & Brightness state stored, using Preferences
Preferences matterPref;
const char *onOffPrefKey = "OnOff";
const char *brightnessPrefKey = "Brightness";

// set your board RGB LED pin here
#ifdef RGB_BUILTIN
const uint8_t ledPin = RGB_BUILTIN;
#else
const uint8_t ledPin = 2;  // Set your pin here if the board has no RGB_BUILTIN
#warning "Do not forget to set the RGB LED pin"
#endif

// set your board USER BUTTON pin here
const uint8_t buttonPin = BOOT_PIN;  // Set your pin here. Using BOOT Button.
MatterButton button;

// Set the RGB LED Light based on the current state of the Dimmable Light
bool setLightState(bool state, uint8_t brightness) {
  if (state) {
#ifdef RGB_BUILTIN
    rgbLedWrite(ledPin, brightness, brightness, brightness);
#else
    analogWrite(ledPin, brightness);
#endif
  } else {
#ifndef RGB_BUILTIN
    // after analogWrite(), it is necessary to set the GPIO to digital mode first
    pinMode(ledPin, OUTPUT);
#endif
    digitalWrite(ledPin, LOW);
  }
  // store last Brightness and OnOff state for when the Light is restarted / power goes off
  matterPref.putUChar(brightnessPrefKey, brightness);
  matterPref.putBool(onOffPrefKey, state);
  // This callback must return the success state to Matter core
  return true;
}

void setup() {
  // Initialize the USER BUTTON (Boot button) GPIO that will act as a toggle switch
  button.begin(buttonPin);
  // Initialize the LED (light) GPIO and Matter End Point
  pinMode(ledPin, OUTPUT);

  Serial.begin(115200);

// CONFIG_ENABLE_CHIPOBLE=n: sketch starts Wi-Fi here; with CHIPoBLE the hub delivers credentials.
#if !CONFIG_ENABLE_CHIPOBLE
  matterConnectWiFi(WIFI_SSID, WIFI_PASSWORD);
#endif

  // Initialize Matter EndPoint
  matterPref.begin("MatterPrefs", false);
  // default OnOff state is ON if not stored before
  bool lastOnOffState = matterPref.getBool(onOffPrefKey, true);
  // default brightness ~= 6% (15/255)
  uint8_t lastBrightness = matterPref.getUChar(brightnessPrefKey, 15);
  DimmableLight.begin(lastOnOffState, lastBrightness);
  // set the callback function to handle the Light state change
  DimmableLight.onChange(setLightState);

  // lambda functions are used to set the attribute change callbacks
  DimmableLight.onChangeOnOff([](bool state) {
    Serial.printf("Light OnOff changed to %s\r\n", state ? "ON" : "OFF");
    return true;
  });
  DimmableLight.onChangeBrightness([](uint8_t level) {
    Serial.printf("Light Brightness changed to %u\r\n", level);
    return true;
  });

  // Matter beginning - Last step, after all EndPoints are initialized
  matterSetExampleIdentity("Dimmable Light");
  Matter.begin();
  matterWaitUntilReady();
  Serial.printf("Initial state: %s | brightness: %u\r\n", DimmableLight ? "ON" : "OFF", DimmableLight.getBrightness());
  DimmableLight.updateAccessory();
}

void loop() {
  matterRestartIfNoFabric();

  matterButtonEvent_t ev;
  while ((ev = button.poll()) != MATTER_BUTTON_NONE) {
    if (ev == MATTER_BUTTON_CLICK) {
      Serial.println("User button released. Toggling Light!");
      DimmableLight.toggle();
    } else if (ev == MATTER_BUTTON_LONG_HOLD) {
      Serial.println("Decommissioning the Light Matter Accessory. It shall be commissioned again.");
      DimmableLight = false;
      Matter.decommission();
    }
  }
}
